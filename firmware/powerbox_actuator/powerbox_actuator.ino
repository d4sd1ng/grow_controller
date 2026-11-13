/**
 * Stromkasten-Aktor-Knoten V1
 *
 * Rolle:
 * - schaltet genau vier SSR-Eingaenge gemaess wiring_plan_v2.md
 * - holt Sollwerte direkt per WLAN vom Sensor-Knoten
 * - meldet die tatsaechlich gesetzten Ausgaenge an den Sensor-Knoten zurueck
 * - bei Kommunikationsverlust: alle vier Ausgaenge AUS
 *
 * GPIO17 -> SSR1 Licht
 * GPIO18 -> SSR2 Umluft links + rechts
 * GPIO19 -> SSR3 Abluft
 * GPIO23 -> SSR4 Heizstrahler
 *
 * Die 230-V-Lastseite ist nicht Bestandteil dieser Firmware.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
#error "Credentials missing: fill local .env and run python export_firmware_secrets_from_env.py"
const char *WIFI_SSID = "";
const char *WIFI_PASS = "";
const char *TELEMETRY_API_KEY = "";
#endif

static constexpr uint8_t PIN_LIGHT = 17;
static constexpr uint8_t PIN_CIRC = 18;
static constexpr uint8_t PIN_EXHAUST = 19;
static constexpr uint8_t PIN_HEATER = 23;

static constexpr uint16_t SENSOR_HTTP_PORT = 8081;
static constexpr unsigned long POLL_MS = 1000;
static constexpr unsigned long STATUS_POST_MS = 3000;
static constexpr unsigned long FAILSAFE_MS = 10000;
static constexpr unsigned long MDNS_RETRY_MS = 5000;
static constexpr unsigned long WIFI_RECONNECT_MS = 5000;

struct ActuatorState {
  bool light = false;
  bool heater = false;
  bool circulation = false;
  bool exhaust = false;
};

ActuatorState desired;
ActuatorState actual;

IPAddress sensorIp;
unsigned long lastGoodCommandMs = 0;
unsigned long lastPollMs = 0;
unsigned long lastStatusPostMs = 0;
unsigned long lastMdnsRetryMs = 0;
unsigned long lastWifiReconnectMs = 0;
bool failsafeActive = true;

static void applyState(const ActuatorState &s) {
  digitalWrite(PIN_LIGHT, s.light ? HIGH : LOW);
  digitalWrite(PIN_CIRC, s.circulation ? HIGH : LOW);
  digitalWrite(PIN_EXHAUST, s.exhaust ? HIGH : LOW);
  digitalWrite(PIN_HEATER, s.heater ? HIGH : LOW);
  actual = s;
}

static void allOff() {
  ActuatorState safe;
  applyState(safe);
}

static bool resolveSensor() {
  IPAddress ip = MDNS.queryHost("grow-sensor", 1500);
  if (ip == IPAddress(0, 0, 0, 0)) return false;
  sensorIp = ip;
  return true;
}

static String sensorUrl(const char *path) {
  return String("http://") + sensorIp.toString() + ":" +
         String(static_cast<unsigned>(SENSOR_HTTP_PORT)) + path;
}

static void addApiKey(HTTPClient &http) {
  if (strlen(TELEMETRY_API_KEY) > 0) {
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  }
}

static bool fetchTargets() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (sensorIp == IPAddress(0, 0, 0, 0) && !resolveSensor()) return false;

  HTTPClient http;
  http.setTimeout(2000);
  if (!http.begin(sensorUrl("/api/local/actuators"))) return false;
  addApiKey(http);

  const int code = http.GET();
  if (code != 200) {
    http.end();
    if (code < 0) sensorIp = IPAddress(0, 0, 0, 0);
    return false;
  }

  const String payload = http.getString();
  http.end();

  StaticJsonDocument<768> doc;
  if (deserializeJson(doc, payload) || !doc["ok"].as<bool>()) return false;

  JsonObject commands = doc["commands"];
  if (commands.isNull()) return false;

  ActuatorState next;
  next.light = commands["light"] | false;
  next.heater = commands["heater"] | false;
  next.circulation = commands["circulation_vent"] | false;
  next.exhaust = commands["exhaust_vent"] | false;

  desired = next;
  applyState(desired);
  lastGoodCommandMs = millis();
  failsafeActive = false;
  return true;
}

static bool postActualStatus() {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (sensorIp == IPAddress(0, 0, 0, 0) && !resolveSensor()) return false;

  StaticJsonDocument<384> doc;
  doc["device_id"] = "grow_power_01";
  doc["firmware_version"] = "1.0.0-hw-v2";
  doc["failsafe"] = failsafeActive;

  JsonObject s = doc.createNestedObject("actual");
  s["light"] = actual.light;
  s["heater"] = actual.heater;
  s["circulation_vent"] = actual.circulation;
  s["exhaust_vent"] = actual.exhaust;

  String body;
  serializeJson(doc, body);

  HTTPClient http;
  http.setTimeout(2000);
  if (!http.begin(sensorUrl("/api/local/actuator-status"))) return false;
  http.addHeader("Content-Type", "application/json");
  addApiKey(http);

  const int code = http.POST(body);
  http.end();
  if (code != 200 && code < 0) sensorIp = IPAddress(0, 0, 0, 0);
  return code == 200;
}

void setup() {
  Serial.begin(115200);

  pinMode(PIN_LIGHT, OUTPUT);
  pinMode(PIN_CIRC, OUTPUT);
  pinMode(PIN_EXHAUST, OUTPUT);
  pinMode(PIN_HEATER, OUTPUT);
  allOff();

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 20000) {
    delay(250);
  }

  if (WiFi.status() == WL_CONNECTED) {
    MDNS.begin("grow-power");
    resolveSensor();
  }
}

void loop() {
  const unsigned long now = millis();

  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiReconnectMs >= WIFI_RECONNECT_MS) {
      lastWifiReconnectMs = now;
      WiFi.reconnect();
    }
  } else if (sensorIp == IPAddress(0, 0, 0, 0) && now - lastMdnsRetryMs >= MDNS_RETRY_MS) {
    lastMdnsRetryMs = now;
    resolveSensor();
  }

  if (now - lastPollMs >= POLL_MS) {
    lastPollMs = now;
    fetchTargets();
  }

  if (lastGoodCommandMs == 0 || now - lastGoodCommandMs > FAILSAFE_MS) {
    if (!failsafeActive) {
      failsafeActive = true;
      allOff();
    }
  }

  if (now - lastStatusPostMs >= STATUS_POST_MS) {
    lastStatusPostMs = now;
    postActualStatus();
  }

  delay(5);
}
