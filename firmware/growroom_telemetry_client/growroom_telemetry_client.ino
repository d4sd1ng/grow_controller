/**
 * Sensor-Knoten V4 fuer Grow Controller
 *
 * Rolle:
 * - liest alle Sensoren im Sensorkasten
 * - berechnet VPD und AUTO-Ziele fuer Heizung/Umluft/Abluft
 * - stellt die vier Aktor-Sollwerte per lokalem HTTP fuer den Stromkasten-ESP bereit
 * - stellt Live-Werte + MANUAL-Steuerung fuer das CYD bereit
 * - sendet Telemetrie an den Homeserver (Logging/Dashboard)
 *
 * Hardware laut wiring_plan_v2.md:
 *   GPIO21/22 -> interner I2C-Bus: OLED + ADS1115
 *   GPIO25    -> DHT22 #1 Pflanzenzone
 *   GPIO26    -> DHT22 #2 Unter-/Zuluftzone
 *   GPIO27    -> DHT22 #3 Ober-/Abluftzone
 *   GPIO32    -> 1-Wire: 2x DS18B20
 *   GPIO13/14 -> separater I2C-Bus: BH1750
 *
 * Keine SSR-Ausgaenge an diesem ESP32.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_ADS1X15.h>
#include <DHT.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
const char *WIFI_SSID = "YOUR_SSID";
const char *WIFI_PASS = "YOUR_PASSWORD";
const char *SERVER_HOST = "192.168.1.50";
const uint16_t SERVER_PORT = 8765;
const char *TELEMETRY_API_KEY = "";
const bool SERVER_USE_HTTPS = false;
const bool SERVER_TLS_INSECURE = false;
#endif

#include "grow_http_client.h"

/* ---------- Pins ---------- */
static constexpr uint8_t PIN_DHT1 = 25;
static constexpr uint8_t PIN_DHT2 = 26;
static constexpr uint8_t PIN_DHT3 = 27;
static constexpr uint8_t PIN_ONEWIRE = 32;

static constexpr int PIN_I2C_SDA = 21;
static constexpr int PIN_I2C_SCL = 22;
static constexpr int PIN_BH_SDA = 13;
static constexpr int PIN_BH_SCL = 14;

/* ---------- Intervale ---------- */
static constexpr unsigned long SENSOR_READ_MS = 2000;
static constexpr unsigned long OLED_REFRESH_MS = 1000;
static constexpr unsigned long TELEMETRY_INTERVAL_MS = 10000;
static constexpr unsigned long WIFI_RECONNECT_MS = 5000;
static constexpr unsigned long POWER_STATUS_TIMEOUT_MS = 15000;

/* ---------- Regelparameter ---------- */
static constexpr float TARGET_AIR_TEMP = 24.0f;
static constexpr float TARGET_HUMIDITY = 60.0f;
static constexpr float TEMP_HYST = 0.5f;
static constexpr float HUM_HYST = 2.0f;

/* ---------- Analog-Kalibrierung ----------
 * pH bleibt NAN, bis zwei reale Kalibrierpunkte eingetragen sind.
 * PH_CAL_VOLT_4 / PH_CAL_VOLT_7 = Spannung AM ADS1115-EINGANG,
 * also nach dem 10k/20k-Spannungsteiler.
 */
static constexpr float PH_CAL_VOLT_4 = NAN;
static constexpr float PH_CAL_VOLT_7 = NAN;

/* ---------- OLED / I2C ---------- */
static constexpr uint8_t OLED_ADDR = 0x3C;
static constexpr int OLED_RESET = -1;
Adafruit_SSD1306 display(128, 64, &Wire, OLED_RESET);
Adafruit_ADS1115 ads;

/* BH1750 auf zweitem ESP32-I2C-Controller */
TwoWire bhWire = TwoWire(1);
static uint8_t bh1750Addr = 0;
static bool bh1750Present = false;

/* ---------- Sensoren ---------- */
DHT dht1(PIN_DHT1, DHT22);
DHT dht2(PIN_DHT2, DHT22);
DHT dht3(PIN_DHT3, DHT22);
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature ds18b20(&oneWire);

struct AirReading {
  float tempC = NAN;
  float rh = NAN;
  bool ok = false;
};

struct SensorData {
  AirReading dht[3];
  float ds1C = NAN;
  float ds2C = NAN;
  uint8_t dsCount = 0;

  float phInputV = NAN;
  float ph = NAN;
  float tdsInputV = NAN;
  float tdsPpm = NAN;
  float ec = NAN;  // absichtlich erst nach echter EC/TDS-Kalibrierung befuellen

  float vpd = NAN;
  uint32_t lux = 0;

  bool adsOk = false;
} sensors;

/* ---------- Aktor-Soll / Ist ---------- */
struct ActuatorState {
  bool light = false;
  bool heater = false;
  bool circulation = false;
  bool exhaust = false;
};

ActuatorState targets;
ActuatorState powerActual;
unsigned long lastPowerStatusMs = 0;
uint32_t commandSeq = 0;
char controlMode[8] = "auto";

/* ---------- Lokale API ---------- */
static constexpr uint16_t LOCAL_HTTP_PORT = 8081;
WebServer localServer(LOCAL_HTTP_PORT);
const char *LOCAL_HEADER_KEYS[] = {"X-Telemetry-Key"};

/* ---------- Timing ---------- */
unsigned long lastSensorRead = 0;
unsigned long lastOledRefresh = 0;
unsigned long lastTelemetry = 0;
unsigned long lastWifiReconnect = 0;

/* ---------- Hilfsfunktionen ---------- */

static float computeVpd(float tC, float rh) {
  if (isnan(tC) || isnan(rh) || rh < 0.0f || rh > 100.0f) return NAN;
  const float svp = 0.61078f * expf((17.27f * tC) / (tC + 237.3f));
  return svp * (1.0f - rh / 100.0f);
}

static AirReading readDht(DHT &dht) {
  AirReading r;
  r.tempC = dht.readTemperature();
  r.rh = dht.readHumidity();
  r.ok = !isnan(r.tempC) && !isnan(r.rh);
  if (!r.ok) {
    r.tempC = NAN;
    r.rh = NAN;
  }
  return r;
}

static float adsCountsToVolts(int16_t raw) {
  /* GAIN_ONE = +/-4.096 V -> 125 uV/bit */
  return static_cast<float>(raw) * 0.000125f;
}

static float phFromVoltage(float v) {
  if (isnan(v) || isnan(PH_CAL_VOLT_4) || isnan(PH_CAL_VOLT_7)) return NAN;
  const float dv = PH_CAL_VOLT_4 - PH_CAL_VOLT_7;
  if (fabsf(dv) < 0.001f) return NAN;
  return 7.0f + (v - PH_CAL_VOLT_7) * (4.0f - 7.0f) / dv;
}

static float cqrobotTdsPpm(float inputV, float temperatureC) {
  if (isnan(inputV)) return NAN;
  float temp = isnan(temperatureC) ? 25.0f : temperatureC;
  float coefficient = 1.0f + 0.02f * (temp - 25.0f);
  if (coefficient < 0.1f) coefficient = 0.1f;
  const float compensatedV = inputV / coefficient;
  const float ppm =
      (133.42f * compensatedV * compensatedV * compensatedV
       - 255.86f * compensatedV * compensatedV
       + 857.39f * compensatedV) * 0.5f;
  return ppm < 0.0f ? 0.0f : ppm;
}

static bool localAuthorized() {
  if (strlen(TELEMETRY_API_KEY) == 0) return true;
  return localServer.hasHeader("X-Telemetry-Key") &&
         localServer.header("X-Telemetry-Key") == TELEMETRY_API_KEY;
}

static void sendUnauthorized() {
  localServer.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
}

static void writeActuators(JsonObject obj, const ActuatorState &s) {
  obj["light"] = s.light;
  obj["heater"] = s.heater;
  obj["circulation_vent"] = s.circulation;
  obj["exhaust_vent"] = s.exhaust;
}

static bool powerOnline() {
  return lastPowerStatusMs != 0 && (millis() - lastPowerStatusMs) <= POWER_STATUS_TIMEOUT_MS;
}

/* ---------- BH1750 ---------- */

static void scanBh1750Bus() {
  bh1750Present = false;
  bh1750Addr = 0;
  const uint8_t candidates[] = {0x23, 0x5C};
  for (uint8_t addr : candidates) {
    bhWire.beginTransmission(addr);
    if (bhWire.endTransmission() == 0) {
      bh1750Present = true;
      bh1750Addr = addr;
      break;
    }
  }
}

static uint32_t readBh1750() {
  if (!bh1750Present) return 0;

  bhWire.beginTransmission(bh1750Addr);
  bhWire.write(0x01);  // power on
  if (bhWire.endTransmission() != 0) return 0;
  delay(10);

  bhWire.beginTransmission(bh1750Addr);
  bhWire.write(0x10);  // continuously H-resolution mode
  if (bhWire.endTransmission() != 0) return 0;
  delay(180);

  if (bhWire.requestFrom(static_cast<int>(bh1750Addr), 2) != 2) return 0;
  uint16_t raw = (static_cast<uint16_t>(bhWire.read()) << 8) | bhWire.read();
  return static_cast<uint32_t>(raw / 1.2f);
}

/* ---------- Sensor-Lesung ---------- */

static void readSensors() {
  sensors.dht[0] = readDht(dht1);
  sensors.dht[1] = readDht(dht2);
  sensors.dht[2] = readDht(dht3);

  sensors.vpd = sensors.dht[0].ok
      ? computeVpd(sensors.dht[0].tempC, sensors.dht[0].rh)
      : NAN;

  ds18b20.requestTemperatures();
  sensors.dsCount = static_cast<uint8_t>(min(ds18b20.getDeviceCount(), 2));
  sensors.ds1C = sensors.dsCount >= 1 ? ds18b20.getTempCByIndex(0) : NAN;
  sensors.ds2C = sensors.dsCount >= 2 ? ds18b20.getTempCByIndex(1) : NAN;
  if (sensors.ds1C == DEVICE_DISCONNECTED_C) sensors.ds1C = NAN;
  if (sensors.ds2C == DEVICE_DISCONNECTED_C) sensors.ds2C = NAN;

  if (sensors.adsOk) {
    const int16_t rawPh = ads.readADC_SingleEnded(0);
    const int16_t rawTds = ads.readADC_SingleEnded(1);
    sensors.phInputV = adsCountsToVolts(rawPh);
    sensors.tdsInputV = adsCountsToVolts(rawTds);
    sensors.ph = phFromVoltage(sensors.phInputV);

    /*
     * DS18B20-Zuordnung wird nach dem physischen Test ueber die ROM-Adressen
     * festgelegt. Bis dahin keine willkuerliche Index-Zuordnung fuer die
     * Temperaturkompensation: bei zwei unzugeordneten Sensoren wird 25 C
     * verwendet.
     */
    const float tdsTempC = sensors.dsCount == 1 ? sensors.ds1C : NAN;
    sensors.tdsPpm = cqrobotTdsPpm(sensors.tdsInputV, tdsTempC);
  } else {
    sensors.phInputV = NAN;
    sensors.tdsInputV = NAN;
    sensors.ph = NAN;
    sensors.tdsPpm = NAN;
  }

  sensors.lux = readBh1750();
}

/* ---------- Steuerung ---------- */

static void autoControl() {
  if (strcmp(controlMode, "auto") != 0) return;

  /* Licht wird spaeter durch Zeitplan/Serverziel ergaenzt und hier nicht veraendert. */

  if (!sensors.dht[0].ok) {
    targets.heater = false;
    targets.circulation = true;
    targets.exhaust = true;
    ++commandSeq;
    return;
  }

  const float t = sensors.dht[0].tempC;
  const float rh = sensors.dht[0].rh;
  ActuatorState before = targets;

  if (t < TARGET_AIR_TEMP - TEMP_HYST) targets.heater = true;
  else if (t > TARGET_AIR_TEMP + TEMP_HYST) targets.heater = false;

  if (rh > TARGET_HUMIDITY + HUM_HYST) targets.circulation = true;
  else if (rh < TARGET_HUMIDITY - HUM_HYST) targets.circulation = false;

  if (t > TARGET_AIR_TEMP + 2.0f) targets.exhaust = true;
  else if (t < TARGET_AIR_TEMP + 0.5f) targets.exhaust = false;

  if (before.light != targets.light ||
      before.heater != targets.heater ||
      before.circulation != targets.circulation ||
      before.exhaust != targets.exhaust) {
    ++commandSeq;
  }
}

/* ---------- Lokale HTTP-API ---------- */

static void handleSummary() {
  if (!localAuthorized()) return sendUnauthorized();

  DynamicJsonDocument doc(3072);
  doc["ok"] = true;

  JsonObject control = doc.createNestedObject("control");
  control["control_mode"] = controlMode;
  JsonObject targetObj = control.createNestedObject("targets");
  writeActuators(targetObj, targets);

  JsonObject reading = doc.createNestedObject("reading");
  if (sensors.dht[0].ok) {
    reading["air_temperature"] = sensors.dht[0].tempC;
    reading["humidity"] = sensors.dht[0].rh;
  } else {
    reading["air_temperature"] = nullptr;
    reading["humidity"] = nullptr;
  }
  reading["vpd"] = isnan(sensors.vpd) ? nullptr : sensors.vpd;
  reading["ph"] = isnan(sensors.ph) ? nullptr : sensors.ph;
  reading["ph_voltage"] = isnan(sensors.phInputV) ? nullptr : sensors.phInputV;
  reading["ec"] = nullptr;
  reading["tds_ppm"] = isnan(sensors.tdsPpm) ? nullptr : sensors.tdsPpm;
  reading["tds_voltage"] = isnan(sensors.tdsInputV) ? nullptr : sensors.tdsInputV;
  reading["light_level"] = sensors.lux;
  reading["ds18b20_1_temperature"] = isnan(sensors.ds1C) ? nullptr : sensors.ds1C;
  reading["ds18b20_2_temperature"] = isnan(sensors.ds2C) ? nullptr : sensors.ds2C;
  reading["water_temperature"] = nullptr;

  for (int i = 0; i < 3; ++i) {
    String prefix = String("dht") + String(i + 1) + "_";
    reading[prefix + "temperature"] = sensors.dht[i].ok ? sensors.dht[i].tempC : nullptr;
    reading[prefix + "humidity"] = sensors.dht[i].ok ? sensors.dht[i].rh : nullptr;
  }

  JsonObject power = doc.createNestedObject("power_node");
  power["online"] = powerOnline();
  JsonObject actualObj = power.createNestedObject("actual");
  writeActuators(actualObj, powerActual);

  String body;
  serializeJson(doc, body);
  localServer.send(200, "application/json", body);
}

static void handleActuatorTargets() {
  if (!localAuthorized()) return sendUnauthorized();

  StaticJsonDocument<512> doc;
  doc["ok"] = true;
  doc["control_mode"] = controlMode;
  doc["seq"] = commandSeq;
  doc["source_ms"] = millis();
  JsonObject commands = doc.createNestedObject("commands");
  writeActuators(commands, targets);

  String body;
  serializeJson(doc, body);
  localServer.send(200, "application/json", body);
}

static void handleControlTargets() {
  if (!localAuthorized()) return sendUnauthorized();

  StaticJsonDocument<512> in;
  if (deserializeJson(in, localServer.arg("plain"))) {
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_json\"}");
  }

  bool changed = false;
  if (in.containsKey("light")) {
    targets.light = in["light"].as<bool>();
    changed = true;
  }
  if (in.containsKey("heater")) {
    targets.heater = in["heater"].as<bool>();
    changed = true;
  }
  if (in.containsKey("circulation_vent")) {
    targets.circulation = in["circulation_vent"].as<bool>();
    changed = true;
  }
  if (in.containsKey("exhaust_vent")) {
    targets.exhaust = in["exhaust_vent"].as<bool>();
    changed = true;
  }

  if (changed) {
    strncpy(controlMode, "manual", sizeof(controlMode));
    ++commandSeq;
  }

  localServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleControlMode() {
  if (!localAuthorized()) return sendUnauthorized();

  StaticJsonDocument<128> in;
  if (deserializeJson(in, localServer.arg("plain"))) {
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_json\"}");
  }

  const char *mode = in["mode"];
  if (!mode || (strcmp(mode, "auto") != 0 && strcmp(mode, "manual") != 0)) {
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_mode\"}");
  }

  strncpy(controlMode, mode, sizeof(controlMode) - 1);
  controlMode[sizeof(controlMode) - 1] = '\0';
  ++commandSeq;
  localServer.send(200, "application/json", "{\"ok\":true}");
}

static void handlePowerStatus() {
  if (!localAuthorized()) return sendUnauthorized();

  StaticJsonDocument<512> in;
  if (deserializeJson(in, localServer.arg("plain"))) {
    return localServer.send(400, "application/json", "{\"ok\":false,\"error\":\"bad_json\"}");
  }

  JsonObject s = in["actual"];
  if (!s.isNull()) {
    powerActual.light = s["light"] | false;
    powerActual.heater = s["heater"] | false;
    powerActual.circulation = s["circulation_vent"] | false;
    powerActual.exhaust = s["exhaust_vent"] | false;
    lastPowerStatusMs = millis();
  }

  localServer.send(200, "application/json", "{\"ok\":true}");
}

static void setupLocalApi() {
  localServer.collectHeaders(LOCAL_HEADER_KEYS, 1);
  localServer.on("/api/local/summary", HTTP_GET, handleSummary);
  localServer.on("/api/local/actuators", HTTP_GET, handleActuatorTargets);
  localServer.on("/api/local/control/targets", HTTP_POST, handleControlTargets);
  localServer.on("/api/local/control/mode", HTTP_POST, handleControlMode);
  localServer.on("/api/local/actuator-status", HTTP_POST, handlePowerStatus);
  localServer.begin();
}

/* ---------- Telemetrie zum Homeserver ---------- */

static bool sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) return false;

  DynamicJsonDocument doc(4096);

  doc["air_temperature"] = sensors.dht[0].ok ? sensors.dht[0].tempC : nullptr;
  doc["humidity"] = sensors.dht[0].ok ? sensors.dht[0].rh : nullptr;
  doc["water_temperature"] = nullptr;
  doc["case_temperature"] = nullptr;
  doc["light_level"] = sensors.lux;
  doc["ec"] = nullptr;
  doc["ph"] = isnan(sensors.ph) ? nullptr : sensors.ph;

  doc["vpd"] = isnan(sensors.vpd) ? nullptr : sensors.vpd;
  doc["ph_voltage"] = isnan(sensors.phInputV) ? nullptr : sensors.phInputV;
  doc["tds_voltage"] = isnan(sensors.tdsInputV) ? nullptr : sensors.tdsInputV;
  doc["tds_ppm"] = isnan(sensors.tdsPpm) ? nullptr : sensors.tdsPpm;
  doc["ds18b20_1_temperature"] = isnan(sensors.ds1C) ? nullptr : sensors.ds1C;
  doc["ds18b20_2_temperature"] = isnan(sensors.ds2C) ? nullptr : sensors.ds2C;

  for (int i = 0; i < 3; ++i) {
    String prefix = String("dht") + String(i + 1) + "_";
    doc[prefix + "temperature"] = sensors.dht[i].ok ? sensors.dht[i].tempC : nullptr;
    doc[prefix + "humidity"] = sensors.dht[i].ok ? sensors.dht[i].rh : nullptr;
  }

  const ActuatorState &reported = powerOnline() ? powerActual : targets;
  JsonObject relays = doc.createNestedObject("relays");
  writeActuators(relays, reported);

  JsonObject dev = doc.createNestedObject("device_info");
  dev["device_id"] = "grow_sensor_01";
  dev["firmware_version"] = "4.0.0-hw-v2";
  dev["control_mode"] = controlMode;
  dev["ads1115"] = sensors.adsOk;
  dev["bh1750"] = bh1750Present;
  dev["bh1750_address"] = bh1750Addr;
  dev["ds18b20_count"] = sensors.dsCount;
  dev["power_node_online"] = powerOnline();

  String body;
  serializeJson(doc, body);

  HTTPClient http;
  http.setTimeout(5000);
  if (!growHttpBegin(http, "/api/v1/telemetry")) return false;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  const int code = http.POST(body);
  http.end();
  return code == 200;
}

/* ---------- OLED ---------- */

static void drawOled() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  if (sensors.dht[0].ok) {
    display.printf("T %.1fC RH %.0f%%", sensors.dht[0].tempC, sensors.dht[0].rh);
  } else {
    display.print("DHT1 FEHLER");
  }

  display.setCursor(0, 16);
  display.print("VPD ");
  if (isnan(sensors.vpd)) display.print("--");
  else display.printf("%.2f", sensors.vpd);
  display.print("  Lux ");
  display.print(sensors.lux);

  display.setCursor(0, 32);
  display.print("pH ");
  if (isnan(sensors.ph)) display.print("--");
  else display.printf("%.2f", sensors.ph);
  display.print(" TDS ");
  if (isnan(sensors.tdsPpm)) display.print("--");
  else display.printf("%.0f", sensors.tdsPpm);

  display.setCursor(0, 48);
  display.printf("%s L%d H%d C%d E%d",
                 controlMode,
                 targets.light,
                 targets.heater,
                 targets.circulation,
                 targets.exhaust);

  display.display();
}

/* ---------- Setup / Loop ---------- */

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000);
  bhWire.begin(PIN_BH_SDA, PIN_BH_SCL, 50000);

  const bool oledOk = display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR);
  if (oledOk) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 24);
    display.print("Grow Sensor V4");
    display.display();
  }

  ads.setGain(GAIN_ONE);
  sensors.adsOk = ads.begin(0x48, &Wire);

  scanBh1750Bus();

  dht1.begin();
  dht2.begin();
  dht3.begin();
  ds18b20.begin();

  Serial.printf("ADS1115: %s\n", sensors.adsOk ? "OK" : "NICHT GEFUNDEN");
  Serial.printf("BH1750: %s 0x%02X\n", bh1750Present ? "OK" : "NICHT GEFUNDEN", bh1750Addr);
  Serial.printf("DS18B20 gefunden: %u\n", ds18b20.getDeviceCount());

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin("grow-sensor")) {
      MDNS.addService("http", "tcp", LOCAL_HTTP_PORT);
    }
  }

  setupLocalApi();
  readSensors();
}

void loop() {
  const unsigned long now = millis();

  localServer.handleClient();

  if (WiFi.status() != WL_CONNECTED && now - lastWifiReconnect >= WIFI_RECONNECT_MS) {
    lastWifiReconnect = now;
    WiFi.reconnect();
  }

  if (now - lastSensorRead >= SENSOR_READ_MS) {
    lastSensorRead = now;
    readSensors();
  }

  autoControl();

  if (now - lastOledRefresh >= OLED_REFRESH_MS) {
    lastOledRefresh = now;
    drawOled();
  }

  if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    sendTelemetry();
  }

  delay(2);
}
