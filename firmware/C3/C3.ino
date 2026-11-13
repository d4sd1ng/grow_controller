/*
 Grow Controller - C3 SuperMini - Aktor-Firmware 1.2.1 (5 dosing channels)
 Kein OLED. Kein Sensor-Eingriff. Die Leistungsausgaenge sind bis zur
 Hardware-Freigabe standardmaessig elektronisch gesperrt.

 Sensor-Schnittstelle nach hardware-v2-firmware:
 GET  http://grow-sensor.local:8081/api/local/actuators
 POST http://grow-sensor.local:8081/api/local/actuator-status

 LVGL -> C3:
 GET  /api/v1/actuator/state
 POST /api/v1/actuator/fans   {"fan_group_1":true,"fan_group_2":false}
 POST /api/v1/actuator/dose   {"pump":1,"duration_ms":500}
 POST /api/v1/actuator/stop   (Software-Sperre bis Neustart)

 Alle geschalteten Signale sind KLEINSPANNUNGS-Steuersignale. Dies ist kein
 Schaltplan und keine Anleitung zum Anschluss von Netzspannungsleitungen.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <esp_timer.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
#error "Credentials missing: configure local .env and run python export_firmware_secrets_from_env.py"
const char *WIFI_SSID = "";
const char *WIFI_PASS = "";
const char *TELEMETRY_API_KEY = "";
#endif

// Freigabe nach Plausibilisierung der Kleinspannungsplatine, nicht vorher.
static constexpr bool HARDWARE_OUTPUTS_ARMED = false;
// Keine Dosierdauer ohne pumpenspezifische Kalibrierung festlegen.
static constexpr uint32_t MAX_DOSE_MS[5] = {0, 0, 0, 0, 0};
static constexpr int8_t PIN_SSR[5] = {0, 1, 3, 4, 5};
static constexpr const char *SSR_KEY[5] = {
  "light", "exhaust_vent", "circulation_vent", "heater", "water_pump"
};
static constexpr uint8_t PIN_I2C_SDA = 7;
static constexpr uint8_t PIN_I2C_SCL = 10;
static constexpr uint8_t PIN_PCA_OE = 20;  // PCA9685 /OE, HIGH = deaktiviert
static constexpr uint8_t PCA_ADDR = 0x40;
static constexpr uint8_t PCA_DOSE_FIRST = 0;  // PCA OUT0..4: 5 Dosierpumpen, OUT5 ungenutzt
static constexpr uint8_t PCA_FAN_FIRST = 6;   // PWM-Ausgaenge 6-7: 2 Lueftergruppen
static constexpr uint32_t COMMAND_TIMEOUT_MS = 10000UL;
static constexpr uint32_t SENSOR_POLL_MS = 1000UL;
static constexpr uint32_t STATUS_POST_MS = 3000UL;
static constexpr uint32_t RECONNECT_MS = 5000UL;

WebServer api(80);
IPAddress sensorIp;
bool pcaAvailable = false;
bool hardwareReady = false;
bool outputsEnabled = false;  // OE bleibt gesperrt, bis ein gueltiger Sensorbefehl kommt
bool stoppedLatch = false;
bool ssr[5] = {};
bool fans[2] = {};
int8_t activeDose = -1;
uint32_t doseStartMs = 0;
uint32_t doseTimeMs = 0;
uint32_t lastCommandMs = 0;
uint32_t lastSensorPoll = 0;
uint32_t lastStatusPost = 0;
uint32_t lastWifiAttempt = 0;
bool sensorWaterPumpField = false;
// Der Timer laeuft unabhaengig von HTTP-Aufrufen: ein haengender Netzwerk-Request
// darf den DOSIERIMPULS NICHT verlaengern. Nach Ablauf sperrt der Timer OE.
esp_timer_handle_t doseGuardTimer = nullptr;
volatile bool doseGuardExpired = false;
void doseGuardCallback(void *) {
  digitalWrite(PIN_PCA_OE, HIGH);  // fail-safe: alle PCA-Ausgaenge sperren
  doseGuardExpired = true;
}


bool writePca(uint8_t reg, const uint8_t *data, size_t len) {
  Wire.beginTransmission(PCA_ADDR);
  Wire.write(reg);
  for (size_t i = 0; i < len; ++i) Wire.write(data[i]);
  return Wire.endTransmission() == 0;
}
bool setPcaChannel(uint8_t channel, bool on) {
  if (channel > 15) return false;
  const uint8_t data[4] = {0, uint8_t(on ? 0x10 : 0), 0,
                            uint8_t(on ? 0 : 0x10)};
  return writePca(uint8_t(0x06 + channel * 4), data, 4);
}
bool allPcaOff() {
  const uint8_t data[4] = {0, 0, 0, 0x10};
  return writePca(0xFA, data, 4);
}
bool initPca() {
  Wire.beginTransmission(PCA_ADDR);
  if (Wire.endTransmission() != 0) return false;
  // MODE1: Auto-Increment. Die Einzelregister werden explizit gesetzt.
  const uint8_t mode = 0x20;
  if (!writePca(0x00, &mode, 1)) return false;
  return allPcaOff();
}
void shutAllOutputs() {
  digitalWrite(PIN_PCA_OE, HIGH);
  outputsEnabled = false;
  if (doseGuardTimer) esp_timer_stop(doseGuardTimer);
  doseGuardExpired = false;
  for (int i = 0; i < 5; ++i) {
    digitalWrite(PIN_SSR[i], LOW);
    ssr[i] = false;
  }
  if (pcaAvailable && !allPcaOff()) {
    // Keine automatische Wiederfreigabe nach I2C-Fehler.
    hardwareReady = false;
    stoppedLatch = true;
  }
  for (int i = 0; i < 2; ++i) fans[i] = false;
  activeDose = -1;
  doseTimeMs = 0;
}
void faultStop(const char *reason) {
  Serial.printf("FAULT STOP: %s\n", reason);
  shutAllOutputs();
  hardwareReady = false;
  stoppedLatch = true;
}
bool writeOutputPca(uint8_t channel, bool value) {
  if (!hardwareReady || !outputsEnabled || stoppedLatch) return false;
  if (!setPcaChannel(channel, value)) {
    faultStop("PCA9685 I2C");
    return false;
  }
  return true;
}
void stopDose() {
  if (doseGuardTimer) esp_timer_stop(doseGuardTimer);
  if (activeDose >= 0 && activeDose < 5 && pcaAvailable) {
    if (!setPcaChannel(PCA_DOSE_FIRST + uint8_t(activeDose), false)) {
      faultStop("Dose OFF I2C");
      return;
    }
  }
  activeDose = -1;
  doseTimeMs = 0;
  // Erst Pumpenkanal deaktivieren, DANN andere PCA-Verbraucher wieder freigeben.
  if (doseGuardExpired) {
    doseGuardExpired = false;
    if (hardwareReady && outputsEnabled && !stoppedLatch &&
        lastCommandMs && millis() - lastCommandMs <= COMMAND_TIMEOUT_MS) {
      digitalWrite(PIN_PCA_OE, LOW);
    }
  }
}
bool applySsr(int channel, bool value) {
  if (channel < 0 || channel >= 5 || !hardwareReady || stoppedLatch) return false;
  digitalWrite(PIN_SSR[channel], value ? HIGH : LOW);
  ssr[channel] = value;
  return true;
}
bool applyFan(int channel, bool value) {
  if (channel < 0 || channel >= 2 || !writeOutputPca(PCA_FAN_FIRST + channel, value)) return false;
  fans[channel] = value;
  return true;
}

bool keyOkay() {
  if (!TELEMETRY_API_KEY || !TELEMETRY_API_KEY[0]) {
    api.send(503, "application/json", "{\"ok\":false,\"error\":\"key_unconfigured\"}");
    return false;
  }
  if (!api.hasHeader("X-Telemetry-Key") ||
      api.header("X-Telemetry-Key") != String(TELEMETRY_API_KEY)) {
    api.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
    return false;
  }
  return true;
}
void replyError(int status, const char *error) {
  StaticJsonDocument<128> d;
  d["ok"] = false;
  d["error"] = error;
  String out;
  serializeJson(d, out);
  api.send(status, "application/json", out);
}
void stateApi() {
  if (!keyOkay()) return;
  StaticJsonDocument<1024> d;
  d["ok"] = true;
  d["firmware"] = "c3-1.2.1-pca9685-5ch";
  d["hardware_ready"] = hardwareReady && outputsEnabled;
  d["hardware_config_valid"] = hardwareReady;
  d["pca9685_detected"] = pcaAvailable;
  d["stopped_latch"] = stoppedLatch;
  d["sensor_online"] = lastCommandMs != 0 && millis() - lastCommandMs <= COMMAND_TIMEOUT_MS;
  d["water_pump_control_available"] = sensorWaterPumpField;
  JsonObject r = d.createNestedObject("relays");
  for (int i = 0; i < 5; ++i) r[SSR_KEY[i]] = ssr[i];
  JsonObject f = d.createNestedObject("fans");
  f["fan_group_1"] = fans[0];
  f["fan_group_2"] = fans[1];
  JsonObject p = d.createNestedObject("dosing");
  p["active_pump"] = activeDose < 0 ? 0 : activeDose + 1;
  p["channel_count"] = 5;
  JsonArray limits = p.createNestedArray("max_duration_ms");
  for (uint32_t v : MAX_DOSE_MS) limits.add(v);
  String out;
  serializeJson(d, out);
  api.send(200, "application/json", out);
}
void fanApi() {
  if (!keyOkay()) return;
  if (!hardwareReady || !outputsEnabled || stoppedLatch) return replyError(503, "hardware_locked");
  if (!lastCommandMs || millis() - lastCommandMs > COMMAND_TIMEOUT_MS)
    return replyError(503, "sensor_offline");
  StaticJsonDocument<256> d;
  if (deserializeJson(d, api.arg("plain")) || !d.is<JsonObject>())
    return replyError(400, "invalid_json");
  JsonObject obj = d.as<JsonObject>();
  if (obj.size() == 0 || obj.size() > 2) return replyError(422, "invalid_fields");
  for (JsonPair kv : obj) {
    const char *k = kv.key().c_str();
    if ((strcmp(k, "fan_group_1") && strcmp(k, "fan_group_2")) ||
        !kv.value().is<bool>()) return replyError(422, "invalid_fields");
  }
  bool next[2] = {fans[0], fans[1]};
  if (obj.containsKey("fan_group_1")) next[0] = obj["fan_group_1"].as<bool>();
  if (obj.containsKey("fan_group_2")) next[1] = obj["fan_group_2"].as<bool>();
  if (!applyFan(0, next[0]) || !applyFan(1, next[1])) return replyError(503, "io_failed");
  api.send(200, "application/json", "{\"ok\":true}");
}
void doseApi() {
  if (!keyOkay()) return;
  if (!hardwareReady || !outputsEnabled || stoppedLatch) return replyError(503, "hardware_locked");
  if (!lastCommandMs || millis() - lastCommandMs > COMMAND_TIMEOUT_MS)
    return replyError(503, "sensor_offline");
  StaticJsonDocument<192> d;
  if (deserializeJson(d, api.arg("plain")) || !d.is<JsonObject>())
    return replyError(400, "invalid_json");
  JsonObject obj = d.as<JsonObject>();
  if (obj.size() != 2 || !obj.containsKey("pump") || !obj.containsKey("duration_ms") ||
      !obj["pump"].is<int>() ||
      !(obj["duration_ms"].is<int>() || obj["duration_ms"].is<unsigned int>() ||
        obj["duration_ms"].is<long>() || obj["duration_ms"].is<unsigned long>()))
    return replyError(422, "invalid_fields");
  const int pump = obj["pump"].as<int>();
  const uint32_t duration = obj["duration_ms"].as<uint32_t>();
  if (pump < 1 || pump > 5 || duration == 0 ||
      MAX_DOSE_MS[pump - 1] == 0 || duration > MAX_DOSE_MS[pump - 1])
    return replyError(422, "dose_not_calibrated_or_out_of_range");
  if (activeDose >= 0) return replyError(409, "another_pump_active");
  if (!doseGuardTimer) return replyError(503, "dose_guard_unavailable");
  if (!writeOutputPca(PCA_DOSE_FIRST + pump - 1, true)) return replyError(503, "io_failed");
  activeDose = pump - 1;
  doseStartMs = millis();
  doseTimeMs = duration;
  doseGuardExpired = false;
  if (esp_timer_start_once(doseGuardTimer, uint64_t(duration) * 1000ULL) != ESP_OK) {
    stopDose();
    faultStop("Dose timer could not start");
    return replyError(503, "dose_timer_failed");
  }
  api.send(200, "application/json", "{\"ok\":true}");
}
void stopApi() {
  if (!keyOkay()) return;
  stoppedLatch = true;
  shutAllOutputs();
  api.send(200, "application/json", "{\"ok\":true,\"stopped_latch\":true}");
}

bool resolveSensor() {
  IPAddress ip = MDNS.queryHost("grow-sensor", 1200);
  if (ip == IPAddress(0,0,0,0)) return false;
  sensorIp = ip;
  return true;
}
String sensorUrl(const char *path) {
  return String("http://") + sensorIp.toString() + ":8081" + path;
}
void addAuth(HTTPClient &http) {
  if (TELEMETRY_API_KEY && TELEMETRY_API_KEY[0])
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
}
void pollSensor() {
  if (WiFi.status() != WL_CONNECTED || stoppedLatch) return;
  if (sensorIp == IPAddress(0,0,0,0) && !resolveSensor()) return;
  HTTPClient http;
  http.setTimeout(1200);
  if (!http.begin(sensorUrl("/api/local/actuators"))) return;
  addAuth(http);
  int status = http.GET();
  if (status != 200) {
    http.end();
    if (status < 0) sensorIp = IPAddress();
    return;
  }
  String body = http.getString();
  http.end();
  StaticJsonDocument<768> d;
  if (deserializeJson(d, body) || !d["ok"].as<bool>() || !d["commands"].is<JsonObject>()) return;
  JsonObject c = d["commands"].as<JsonObject>();
  const char *required[] = {"light", "exhaust_vent", "circulation_vent", "heater"};
  for (auto k : required) if (!c[k].is<bool>()) return;
  bool next[5] = {c["light"].as<bool>(), c["exhaust_vent"].as<bool>(),
                  c["circulation_vent"].as<bool>(), c["heater"].as<bool>(), false};
  // Ohne bestaetigtes Steuersignal bleibt die Filterwasserpumpe AUS.
  sensorWaterPumpField = c["water_pump"].is<bool>();
  if (sensorWaterPumpField) next[4] = c["water_pump"].as<bool>();
  if (hardwareReady && !stoppedLatch) {
    // Nach Boot oder Verbindungsabbruch erst ALLE PCA-Kanaele sperren.
    // Erst nach gueltigem Steuertelegramm OE wieder freigeben.
    if (!outputsEnabled) {
      if (!allPcaOff()) { faultStop("PCA reset before enable"); return; }
      outputsEnabled = true;
      digitalWrite(PIN_PCA_OE, LOW);
    }
    for (int i = 0; i < 5; ++i)
      if (!applySsr(i, next[i])) return;
  }
  lastCommandMs = millis();
}
void sendStatus() {
  if (WiFi.status() != WL_CONNECTED || sensorIp == IPAddress(0,0,0,0) ||
      !TELEMETRY_API_KEY || !TELEMETRY_API_KEY[0]) return;
  // Contract: Sensor::handleActuatorStatus() requires pump_count and actual.
  // Report real state only; pending sensor dosing commands are NOT implemented.
  StaticJsonDocument<1024> d;
  d["device_id"] = "grow_power_c3_01";
  d["firmware_version"] = "1.2.1";
  d["pump_count"] = 5;
  d["outputs_enabled"] = hardwareReady && outputsEnabled && !stoppedLatch;
  d["pump_active"] = activeDose < 0 ? 0 : activeDose + 1;
  d["pump_ack_seq"] = 0U;  // No dosing command acknowledged via sensor API.
  d["failsafe"] = !hardwareReady || !outputsEnabled || stoppedLatch ||
                    lastCommandMs == 0 || millis() - lastCommandMs > COMMAND_TIMEOUT_MS;
  JsonObject actual = d.createNestedObject("actual");
  for (int i = 0; i < 5; ++i) actual[SSR_KEY[i]] = ssr[i];
  actual["lamp_cooling"] = false; // C3 firmware has no lamp-cooling driver.
  String body;
  serializeJson(d, body);
  HTTPClient http;
  http.setTimeout(1000);
  if (!http.begin(sensorUrl("/api/local/actuator-status"))) return;
  http.addHeader("Content-Type", "application/json");
  addAuth(http);
  http.POST(body);
  http.end();
}

void setup() {
  Serial.begin(115200);
  // Sofortige Abschaltung des PCA-Ausgangstreibers, bevor I2C startet.
  digitalWrite(PIN_PCA_OE, HIGH);
  pinMode(PIN_PCA_OE, OUTPUT);
  for (int i = 0; i < 5; ++i) {
    digitalWrite(PIN_SSR[i], LOW);
    pinMode(PIN_SSR[i], OUTPUT);
  }
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  pcaAvailable = initPca();
  esp_timer_create_args_t guard = {};
  guard.callback = &doseGuardCallback;
  guard.name = "grow-dose-stop";
  if (esp_timer_create(&guard, &doseGuardTimer) != ESP_OK) {
    doseGuardTimer = nullptr;
    Serial.println("WARNUNG: Dosier-Zeitwaechter nicht initialisiert; Dosierung gesperrt.");
  }
  hardwareReady = HARDWARE_OUTPUTS_ARMED && pcaAvailable;
  // OE bleibt HIGH bis zum ersten gueltigen Sensorkommando.
  outputsEnabled = false;

  const char *headers[] = {"X-Telemetry-Key"};
  api.collectHeaders(headers, 1);
  api.on("/api/v1/actuator/state", HTTP_GET, stateApi);
  api.on("/api/v1/actuator/fans", HTTP_POST, fanApi);
  api.on("/api/v1/actuator/dose", HTTP_POST, doseApi);
  api.on("/api/v1/actuator/stop", HTTP_POST, stopApi);
  api.onNotFound([](){ replyError(404, "not_found"); });
  api.begin();

  WiFi.mode(WIFI_STA);
  if (WIFI_SSID && WIFI_SSID[0]) WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("C3 ready; hardware=%s, PCA=%s\n",
                hardwareReady ? "armed" : "LOCKED", pcaAvailable ? "yes" : "no");
}
void loop() {
  const uint32_t now = millis();
  static bool mdnsStarted = false;
  if (WiFi.status() == WL_CONNECTED) {
    if (!mdnsStarted) mdnsStarted = MDNS.begin("grow-power");
    api.handleClient();
    if (now - lastSensorPoll >= SENSOR_POLL_MS) {
      lastSensorPoll = now;
      pollSensor();
    }
    if (now - lastStatusPost >= STATUS_POST_MS) {
      lastStatusPost = now;
      sendStatus();
    }
  } else {
    if (now - lastWifiAttempt >= RECONNECT_MS && WIFI_SSID && WIFI_SSID[0]) {
      lastWifiAttempt = now;
      WiFi.reconnect();
    }
    if (outputsEnabled) shutAllOutputs();
    mdnsStarted = false;
    sensorIp = IPAddress();
  }
  if (activeDose >= 0 && (doseGuardExpired || now - doseStartMs >= doseTimeMs)) stopDose();
  if (hardwareReady && outputsEnabled && (lastCommandMs == 0 ? now > COMMAND_TIMEOUT_MS :
                        now - lastCommandMs > COMMAND_TIMEOUT_MS)) {
    // Kommunikation verloren: keine Verbraucher eingeschaltet lassen.
    shutAllOutputs();
  }
  delay(5);
}