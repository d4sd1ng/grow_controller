/*
 * Grow Controller / ESP32-C3 SuperMini / Leistungscontroller v6
 * Genau 5 SSR-Steuerausgaenge, 1 DC-Luefterkanal und 6 Dosierpumpen.
 * KEIN OLED / TFT / Display. Niedervolt-Steuerung; keine Netzlast-Verdrahtung.
 *
 * Die Ausgaenge sind aus Sicherheitsgruenden standardmaessig gesperrt.
 * ENABLE_OUTPUTS_AFTER_HARDWARE_VALIDATION erst nach realem Low-Voltage-Test
 * und unabhaengiger Sicherheitsfreigabe bewusst einschalten.
 *
 * HTTP-Protokoll zum Sensor-ESP32 (grow-sensor.local:8081):
 *   GET  /api/local/actuators
 *   POST /api/local/actuator-status
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <esp_timer.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
const char *WIFI_SSID = "YOUR_SSID";
const char *WIFI_PASS = "YOUR_PASSWORD";
const char *TELEMETRY_API_KEY = "";
#endif

// Werkseitiger, sicherer Zustand: niemals Ausgaenge freigeben, solange
// Schaltpegel, SSR-Eingang, Treiber, Lastschutz und Stromaufnahme ungetestet sind.
#ifndef ENABLE_OUTPUTS_AFTER_HARDWARE_VALIDATION
#define ENABLE_OUTPUTS_AFTER_HARDWARE_VALIDATION 0
#endif

static constexpr uint8_t PUMP_COUNT = 6; // genau sechs; KEIN siebter Kanal
static constexpr uint8_t PIN_SSR_LIGHT = 0;
static constexpr uint8_t PIN_SSR_CIRC = 1;
static constexpr uint8_t PIN_SSR_EXHAUST = 3;
static constexpr uint8_t PIN_SSR_HEATER = 4;
static constexpr uint8_t PIN_SSR_WATER = 5;
static constexpr uint8_t PIN_LAMP_COOLING = 6;
static constexpr uint8_t PIN_I2C_SDA = 7;
static constexpr uint8_t PIN_I2C_SCL = 10;
static constexpr uint8_t PIN_PCA_OE = 20; // HIGH: Pumpentreiber gesperrt
static constexpr uint8_t PCA_ADDRESS = 0x40;
static constexpr uint16_t SENSOR_HTTP_PORT = 8081;
static constexpr unsigned long POLL_MS = 1000;
static constexpr unsigned long STATUS_MS = 2000;
static constexpr unsigned long FAILSAFE_MS = 7000;
static constexpr unsigned long WIFI_RETRY_MS = 5000;
static constexpr unsigned long MDNS_RETRY_MS = 5000;
static constexpr uint16_t MAX_PUMP_PULSE_MS = 2000;
// Nur nach Verifikation des jeweiligen SSR-/MOSFET-Treibers konfigurieren.
static constexpr bool OUTPUT_ACTIVE_HIGH = true;

struct Outputs {
  bool light = false, heater = false, circulation = false;
  bool exhaust = false, water = false, cooling = false;
};
Outputs actual;
IPAddress sensorIp;
unsigned long lastGoodMs = 0, lastPollMs = 0, lastStatusMs = 0;
unsigned long lastWifiRetry = 0, lastMdnsRetry = 0;
bool failSafe = true, pcaReady = false, pumpServiceOff = true;
bool firstSync = true;
uint8_t pumpActive = 0;
uint32_t lastPumpSeq = 0;
uint32_t pumpAckSeq = 0;
esp_timer_handle_t pumpStopTimer = nullptr;

// Sicherheitsabschaltung ueber OE-Hardwarepin auch waehrend blockierender HTTP-Aufrufe.
void pumpTimeout(void *) {
  digitalWrite(PIN_PCA_OE, HIGH);
}
void gatePumpsOff() {
  digitalWrite(PIN_PCA_OE, HIGH);
  if (pumpStopTimer) esp_timer_stop(pumpStopTimer);
  pumpActive = 0;
}
bool pcaWrite(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(PCA_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
bool pcaPump(uint8_t channel, bool enabled) {
  if (channel >= PUMP_COUNT || !pcaReady) return false;
  const uint8_t r = 0x06 + 4 * channel;
  // FULL_ON/FULL_OFF Bit; ohne Pulsbreiten-/Frequenzannahme.
  return pcaWrite(r + 0, 0x00) &&
         pcaWrite(r + 1, enabled ? 0x10 : 0x00) &&
         pcaWrite(r + 2, 0x00) &&
         pcaWrite(r + 3, enabled ? 0x00 : 0x10);
}
void pumpsAllOff() {
  gatePumpsOff();
  if (pcaReady)
    for (uint8_t i = 0; i < PUMP_COUNT; ++i) pcaPump(i, false);
}
void setOut(uint8_t pin, bool on) {
  digitalWrite(pin, on == OUTPUT_ACTIVE_HIGH ? HIGH : LOW);
}
void applyOutputs(const Outputs &s) {
  if (!ENABLE_OUTPUTS_AFTER_HARDWARE_VALIDATION) return;
  setOut(PIN_SSR_LIGHT, s.light);
  setOut(PIN_SSR_CIRC, s.circulation);
  setOut(PIN_SSR_EXHAUST, s.exhaust);
  setOut(PIN_SSR_HEATER, s.heater);
  setOut(PIN_SSR_WATER, s.water);
  setOut(PIN_LAMP_COOLING, s.cooling);
  actual = s;
}
void allOff() {
  pumpsAllOff();
  const Outputs off;
  setOut(PIN_SSR_LIGHT, false);
  setOut(PIN_SSR_CIRC, false);
  setOut(PIN_SSR_EXHAUST, false);
  setOut(PIN_SSR_HEATER, false);
  setOut(PIN_SSR_WATER, false);
  setOut(PIN_LAMP_COOLING, false);
  actual = off;
}
bool resolveSensor() {
  const IPAddress ip = MDNS.queryHost("grow-sensor", 1000);
  if (ip == IPAddress(0,0,0,0)) return false;
  sensorIp = ip;
  return true;
}
String sensorUrl(const char *path) {
  return String("http://") + sensorIp.toString() + ":" +
      String(SENSOR_HTTP_PORT) + path;
}
void addKey(HTTPClient &http) {
  if (strlen(TELEMETRY_API_KEY))
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
}
bool startPump(uint8_t index, uint16_t durationMs) {
  if (!ENABLE_OUTPUTS_AFTER_HARDWARE_VALIDATION || !pcaReady ||
      pumpServiceOff || pumpActive != 0 || index < 1 ||
      index > PUMP_COUNT || durationMs < 100 ||
      durationMs > MAX_PUMP_PULSE_MS || !pumpStopTimer) return false;
  pumpsAllOff();
  if (!pcaPump(index - 1, true)) { pumpsAllOff(); return false; }
  // Timer vor Freigabe starten; OE kann nach Ablauf nie versehentlich LOW bleiben.
  if (esp_timer_start_once(pumpStopTimer, static_cast<uint64_t>(durationMs) * 1000ULL) != ESP_OK) {
    pumpsAllOff();
    return false;
  }
  pumpActive = index;
  digitalWrite(PIN_PCA_OE, LOW);
  return true;
}
bool fetchTargets() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (sensorIp == IPAddress(0,0,0,0) && !resolveSensor()) return false;
  HTTPClient http;
  http.setTimeout(1000);
  if (!http.begin(sensorUrl("/api/local/actuators"))) return false;
  addKey(http);
  const int code = http.GET();
  if (code != 200) {
    http.end();
    if (code < 0) sensorIp = IPAddress(0,0,0,0);
    return false;
  }
  String body = http.getString();
  http.end();
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, body) || !doc["ok"].as<bool>() ||
      doc["pump_count"].as<int>() != PUMP_COUNT) return false;
  JsonObject c = doc["commands"];
  if (c.isNull()) return false;

  Outputs next;
  next.light = c["light"] | false;
  next.heater = c["heater"] | false;
  next.circulation = c["circulation_vent"] | false;
  next.exhaust = c["exhaust_vent"] | false;
  next.water = c["water_pump"] | false;
  next.cooling = c["lamp_cooling"] | false;
  const bool newServiceOff = doc["pump_service_off"] | true;
  const char *mode = doc["control_mode"] | "";
  pumpServiceOff = newServiceOff;
  if (pumpServiceOff || strcmp(mode, "manual") != 0) pumpsAllOff();

  lastGoodMs = millis();
  failSafe = false;
  applyOutputs(next);

  const uint32_t seq = doc["pump_seq"] | 0U;
  if (firstSync) {
    lastPumpSeq = seq; // nach Neustart keine schon vorher gesendete Dosis ausfuehren
    firstSync = false;
  } else if (seq != lastPumpSeq) {
    lastPumpSeq = seq;
    const bool pending = doc["pump_pending"] | false;
    const int index = doc["pump_index"] | 0;
    const int duration = doc["pump_duration_ms"] | 0;
    if (pending && strcmp(mode, "manual") == 0 && !newServiceOff &&
        index >= 1 && index <= PUMP_COUNT &&
        duration >= 100 && duration <= MAX_PUMP_PULSE_MS) {
      if (startPump(static_cast<uint8_t>(index), static_cast<uint16_t>(duration)))
        pumpAckSeq = seq;
    }
  }
  return true;
}
bool postStatus() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (sensorIp == IPAddress(0,0,0,0) && !resolveSensor()) return false;
  StaticJsonDocument<512> doc;
  doc["device_id"] = "grow_power_c3";
  doc["firmware_version"] = "6.0.0-c3-six-pumps";
  doc["pump_count"] = PUMP_COUNT;
  doc["outputs_enabled"] = (ENABLE_OUTPUTS_AFTER_HARDWARE_VALIDATION != 0);
  doc["failsafe"] = failSafe;
  doc["pump_service_off"] = pumpServiceOff && pumpActive == 0;
  doc["pump_active"] = pumpActive;
  doc["pump_ack_seq"] = pumpAckSeq;
  JsonObject a = doc.createNestedObject("actual");
  a["light"] = actual.light;
  a["heater"] = actual.heater;
  a["circulation_vent"] = actual.circulation;
  a["exhaust_vent"] = actual.exhaust;
  a["water_pump"] = actual.water;
  a["lamp_cooling"] = actual.cooling;
  String body;
  serializeJson(doc, body);
  HTTPClient http;
  http.setTimeout(1000);
  if (!http.begin(sensorUrl("/api/local/actuator-status"))) return false;
  http.addHeader("Content-Type", "application/json");
  addKey(http);
  const int code = http.POST(body);
  http.end();
  return code == 200;
}
void setup() {
  Serial.begin(115200);
  // OE zuerst aktiv-high sperren, erst danach I2C und Treiber vorbereiten.
  digitalWrite(PIN_PCA_OE, HIGH);
  pinMode(PIN_PCA_OE, OUTPUT);
  const uint8_t pins[] = {
    PIN_SSR_LIGHT, PIN_SSR_CIRC, PIN_SSR_EXHAUST,
    PIN_SSR_HEATER, PIN_SSR_WATER, PIN_LAMP_COOLING
  };
  for (uint8_t pin : pins) {
    digitalWrite(pin, OUTPUT_ACTIVE_HIGH ? LOW : HIGH);
    pinMode(pin, OUTPUT);
  }
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  // PCA9685 MODE1: Autoincrement, Ausgangstreiber bleiben via OE gesperrt.
  pcaReady = pcaWrite(0x00, 0x20);
  if (pcaReady) {
    for (uint8_t i = 0; i < PUMP_COUNT; ++i)
      if (!pcaPump(i, false)) pcaReady = false;
  }
  esp_timer_create_args_t timerCfg = {};
  timerCfg.callback = &pumpTimeout;
  timerCfg.name = "pump_stop";
  if (esp_timer_create(&timerCfg, &pumpStopTimer) != ESP_OK)
    pumpStopTimer = nullptr;
  allOff();
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}
void loop() {
  const unsigned long now = millis();
  // Zeitbedingung kontrolliert den Zustand; Hardware-OE wird bereits
  // unabhaengig davon durch esp_timer gesperrt.
  if (pumpActive && pumpStopTimer &&
      !esp_timer_is_active(pumpStopTimer)) pumpsAllOff();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiRetry >= WIFI_RETRY_MS) {
      lastWifiRetry = now;
      WiFi.reconnect();
      firstSync = true;
    }
    if (!failSafe) { failSafe = true; allOff(); }
  } else if (sensorIp == IPAddress(0,0,0,0) &&
             now - lastMdnsRetry >= MDNS_RETRY_MS) {
    lastMdnsRetry = now;
    resolveSensor();
  }
  if (now - lastPollMs >= POLL_MS) {
    lastPollMs = now;
    fetchTargets();
  }
  if (lastGoodMs == 0 || now - lastGoodMs > FAILSAFE_MS) {
    if (!failSafe) { failSafe = true; allOff(); }
  }
  if (now - lastStatusMs >= STATUS_MS) {
    lastStatusMs = now;
    postStatus();
  }
  delay(5);
}
