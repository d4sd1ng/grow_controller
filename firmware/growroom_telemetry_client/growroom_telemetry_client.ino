/**
 * Grow-room ESP32 (fest verbaut + optionales lokales Display):
 * - regelmäßig GET /api/v1/control/state → Relais hardware (Ziel kommt von Server: Dashboard oder portables CYD)
 * - POST /api/v1/telemetry → Messwerte + aktuelle Relais in SQLite (Streamlit-Charts)
 *
 * Lokale Anzeige T/RH/pH/EC: hier über Serial; echtes OLED/TFT parallel nach gleichen Variablen füttern.
 *
 * Libraries: ArduinoJson v6.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
/* Fallback nur für den ersten Build — produktiv: `python export_firmware_secrets_from_env.py` */
const char *WIFI_SSID = "YOUR_SSID";
const char *WIFI_PASS = "YOUR_PASSWORD";
const char *SERVER_HOST = "192.168.1.50";
const uint16_t SERVER_PORT = 8765;
const char *TELEMETRY_API_KEY = "";
const bool SERVER_USE_HTTPS = false;
const bool SERVER_TLS_INSECURE = false;
#endif

#include "grow_http_client.h"

const char *TELEMETRY_PATH = "/api/v1/telemetry";
const char *CONTROL_PATH = "/api/v1/control/state";

const unsigned long POST_INTERVAL_MS = 10000;
const unsigned long CONTROL_POLL_MS = 2000;

unsigned long lastPost = 0;
unsigned long lastControlPoll = 0;

bool relayHeater = false;
bool relayPump = false;
bool relayCirculation = false;
bool relayExhaust = false;

/* TODO: GPIO an deine Relais-Module anpassen */
#define PIN_RELAY_HEATER 4
#define PIN_RELAY_PUMP 5
#define PIN_RELAY_CIRCULATION 16
#define PIN_RELAY_EXHAUST 17

void applyRelayHardware() {
  digitalWrite(PIN_RELAY_HEATER, relayHeater ? HIGH : LOW);
  digitalWrite(PIN_RELAY_PUMP, relayPump ? HIGH : LOW);
  digitalWrite(PIN_RELAY_CIRCULATION, relayCirculation ? HIGH : LOW);
  digitalWrite(PIN_RELAY_EXHAUST, relayExhaust ? HIGH : LOW);
}

void fetchControlStateFromServer() {
  HTTPClient http;
  if (!growHttpBegin(http, CONTROL_PATH)) {
    Serial.println("control/state http.begin failed");
    return;
  }
  if (strlen(TELEMETRY_API_KEY) > 0) {
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  }
  int code = http.get();
  if (code != 200) {
    Serial.printf("control/state HTTP %d\n", code);
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("control JSON parse error");
    return;
  }
  if (!doc["ok"].as<bool>())
    return;
  JsonObject t = doc["targets"];
  relayHeater = t["heater"] | false;
  relayCirculation = t["circulation_vent"] | false;
  relayExhaust = t["exhaust_vent"] | false;
  relayPump = (t["pump"] | false) || (t["water_pump"] | false);
  applyRelayHardware();
}

void printLocalPanel(float airC, float rh, float ec, float ph) {
  Serial.printf("LOCAL | T=%.1fC RH=%.0f%% EC=%.2f pH=%.1f | R H=%d P=%d C=%d E=%d\n",
                airC, rh, ec, ph,
                relayHeater, relayPump, relayCirculation, relayExhaust);
}

void sendTelemetry() {
  float airC = 24.0f;
  float waterC = 22.0f;
  float rh = 55.0f;
  float ec = 1.35f;
  float ph = 6.2f;
  int lightLux = 400;
  // TODO: echte Sensorwerte

  printLocalPanel(airC, rh, ec, ph);

  StaticJsonDocument<512> doc;
  doc["air_temperature"] = airC;
  doc["water_temperature"] = waterC;
  doc["humidity"] = rh;
  doc["ec"] = ec;
  doc["ph"] = ph;
  doc["light_level"] = lightLux;
  doc["case_temperature"] = airC;

  JsonObject relays = doc.createNestedObject("relays");
  relays["heater"] = relayHeater;
  relays["pump"] = relayPump;
  relays["circulation_vent"] = relayCirculation;
  relays["exhaust_vent"] = relayExhaust;

  String body;
  serializeJson(doc, body);

  HTTPClient http;
  if (!growHttpBegin(http, TELEMETRY_PATH)) {
    Serial.println("POST telemetry http.begin failed");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0) {
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  }
  int code = http.POST(body);
  Serial.printf("POST telemetry -> %d\n", code);
  http.end();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_RELAY_HEATER, OUTPUT);
  pinMode(PIN_RELAY_PUMP, OUTPUT);
  pinMode(PIN_RELAY_CIRCULATION, OUTPUT);
  pinMode(PIN_RELAY_EXHAUST, OUTPUT);
  applyRelayHardware();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected: " + WiFi.localIP().toString());
}

void loop() {
  unsigned long now = millis();
  if (now - lastControlPoll >= CONTROL_POLL_MS) {
    lastControlPoll = now;
    if (WiFi.status() == WL_CONNECTED)
      fetchControlStateFromServer();
  }
  if (now - lastPost >= POST_INTERVAL_MS) {
    lastPost = now;
    if (WiFi.status() == WL_CONNECTED)
      sendTelemetry();
  }
}
