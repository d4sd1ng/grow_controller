/**
 * Portable Cheap Yellow Display: zentrale Mobilanzeige + Steuerung gegen den Server.
 *
 * GET /api/v1/mobile/summary → reading (Sensoren), control.targets (Relais-Soll),
 * alerts (Fehler/Warnungen vom Dashboard oder ESP).
 *
 * Relais umschalten: POST /api/v1/control/targets mit z.B. {"heater":true} (setzt Manuell-Modus).
 * Optional später: Touch-Zonen für Buttons (hier Nur-Anzeige + ein Serial-Kommandobeispiel).
 *
 * TFT_eSPI + ArduinoJson v6 + passendes CYD-User_Setup.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

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

const char *SUMMARY_PATH = "/api/v1/mobile/summary";
const char *CONTROL_PATH = "/api/v1/control/targets";

const unsigned long POLL_MS = 3000;

TFT_eSPI tft;
unsigned long lastFetch = 0;

void drawPanel(const char *l1, const char *l2, const char *l3, const char *l4) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(6, 16);
  tft.println(l1);
  tft.setCursor(6, 44);
  tft.println(l2);
  tft.setCursor(6, 72);
  tft.println(l3);
  tft.setTextSize(1);
  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.setCursor(6, 110);
  tft.println(l4);
}

bool httpPostJson(const char *path, const String &jsonBody) {
  HTTPClient http;
  if (!growHttpBegin(http, path))
    return false;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.POST(jsonBody);
  http.end();
  return code == 200;
}

/** Beispiel: Relais togglen (z.B. aus Touch-Handler aufrufen) */
void exampleToggleHeater(bool on) {
  StaticJsonDocument<64> doc;
  doc["heater"] = on;
  String body;
  serializeJson(doc, body);
  httpPostJson(CONTROL_PATH, body);
}

void fetchSummaryAndDraw() {
  HTTPClient http;
  if (!growHttpBegin(http, SUMMARY_PATH)) {
    drawPanel("http begin", "failed", "", "");
    return;
  }
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) {
    char e[32];
    snprintf(e, sizeof(e), "HTTP %d", code);
    drawPanel("Summary", e, "", "");
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, payload)) {
    drawPanel("JSON error", "", "", "");
    return;
  }
  if (!doc["ok"].as<bool>()) {
    drawPanel("ok=false", "", "", "");
    return;
  }

  char l1[48], l2[48], l3[48], l4[96];
  l1[0] = l2[0] = l3[0] = l4[0] = 0;

  JsonObject c = doc["control"];
  const char *mode = c["control_mode"];
  snprintf(l1, sizeof(l1), "Mode: %s", mode ? mode : "?");

  JsonObject r = doc["reading"];
  if (!r.isNull()) {
    snprintf(l2, sizeof(l2), "T%.1f RH%.0f EC%.2f pH%.1f",
             r["air_temperature"].as<float>(),
             r["humidity"].as<float>(),
             r["ec"].as<float>(),
             r["ph"].as<float>());
  } else {
    snprintf(l2, sizeof(l2), "(no telemetry yet)");
  }

  JsonObject t = c["targets"];
  if (!t.isNull()) {
    snprintf(l3, sizeof(l3), "H%d P%d C%d E%d",
             t["heater"].as<bool>() ? 1 : 0,
             (t["pump"].as<bool>() || t["water_pump"].as<bool>()) ? 1 : 0,
             t["circulation_vent"].as<bool>() ? 1 : 0,
             t["exhaust_vent"].as<bool>() ? 1 : 0);
  }

  JsonArray alerts = doc["alerts"].as<JsonArray>();
  if (alerts.size() > 0) {
    JsonObject a0 = alerts[0];
    const char *msg = a0["message"];
    const char *sev = a0["severity"];
    snprintf(l4, sizeof(l4), "[%s] %s", sev ? sev : "?", msg ? msg : "");
  } else {
    snprintf(l4, sizeof(l4), "no alerts");
  }

  drawPanel(l1, l2, l3, l4);
}

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  drawPanel("WiFi...", "", "", "");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED)
    delay(400);
  drawPanel("Online", WiFi.localIP().toString().c_str(), "", "");
}

void loop() {
  unsigned long now = millis();
  if (now - lastFetch >= POLL_MS) {
    lastFetch = now;
    fetchSummaryAndDraw();
  }
  /* Serial: "H1" heater on, "H0" off — minimal ohne Touch */
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "H1")
      exampleToggleHeater(true);
    else if (cmd == "H0")
      exampleToggleHeater(false);
  }
}
