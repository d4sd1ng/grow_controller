/**
 * CYD als 3-Tafel-Fernbedienung (Landscape):
 *  Links: Relais-Touch
 *  Mitte: Live-Messwerte direkt vom Sensor-ESP + Benachrichtigungen vom Server
 *  Rechts: Kurven aus GET /api/v1/telemetry/series (Server-Historie), sonst Fallback Ringpuffer
 *
 * Live/Steuerung laufen direkt Sensor-ESP <-> CYD; Historie/Alerts kommen vom Server.
 *
 * Libs: TFT_eSPI, XPT2046_Touchscreen, ArduinoJson v6
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

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

const char *SUMMARY_PATH = "/api/local/summary";
const char *CONTROL_PATH = "/api/local/control/targets"; // derzeit immer 503 ohne C3
const char *PHASE_PATH = "/api/local/control/phase";
const char *WEEK_PATH = "/api/local/control/week";
const char *MODE_PATH = "/api/local/control/mode";
const uint16_t SENSOR_HTTP_PORT = 8081;
IPAddress sensorIp;

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

#define CAL_X_MIN 200
#define CAL_X_MAX 3700
#define CAL_Y_MIN 240
#define CAL_Y_MAX 3800

/* Layout 320x240 @ rotation 1 */
#define HDR_H 22
#define X_LEFT_END 100
#define X_MID_END 220

const unsigned long POLL_MS = 4000;
const unsigned long SERIES_POLL_MS = 25000;
const unsigned long TOUCH_DEBOUNCE_MS = 280;

#define HIST_CAP 120
float histT[HIST_CAP], histRh[HIST_CAP], histVpd[HIST_CAP];
uint8_t nHist = 0;

float seriesT[HIST_CAP], seriesRh[HIST_CAP], seriesVpd[HIST_CAP];
uint8_t seriesN = 0;
bool haveServerSeries = false;

TFT_eSPI tft;
unsigned long lastPoll = 0;
unsigned long lastSeriesPoll = 0;
unsigned long lastTouch = 0;

struct RemoteState {
  char mode[12];
  bool light, heat, circ, exh;
  bool haveReading;
  bool haveEc;
  float tempC, rh, ec, ph, tdsPpm, waterC;
  char phStatus[16]; // high/low/normal/sensor_fault
  bool haveWater;
  bool haveAir, havePh;
  char phase[12];
  uint8_t growWeek;
  bool havePhTarget, haveEcTarget;
  float phTarget, ecTarget;
  bool profileConfigured;
  float targetAirC, targetHumidity;
  bool c3Connected;
  // EC wird NICHT im CYD umgerechnet, sondern kommt fertig vom Sensor.
  // Nur die neue Sensor-Firmware weist den EC-Wert explizit der DS-Temperatur zu.
  bool haveEcMeasurementC;
  float ecMeasurementC;
  bool haveLux, haveAreaLumens;
  float lux, areaLumens; // ~lm ueber 1,00 m2, keine gemessenen Lampen-Lumen
  char alertLines[5][72];
  uint8_t nAlerts;
} state;

enum RelaySlot { REL_LIGHT = 0, REL_HEAT = 1, REL_CIRC = 2, REL_EXH = 3 };

struct RelayBtn {
  const char *label;
  int x, y, w, h;
  RelaySlot slot;
};

RelayBtn buttons[4]; // alte Relais-Geometrie bleibt ungenutzt, bis C3 wirklich verbunden ist
enum class ScreenPage : uint8_t { Live, Phase, Soll, Chart, Info };
ScreenPage screenPage = ScreenPage::Live;
bool lastSensorResponseOk = false;
char uiMessage[58] = "";
static constexpr int NAV_H = 37;
static constexpr int PAGE_BOTTOM = 240 - NAV_H;

static float computeVpd(float T, float rh) {
  if (T < -40.f || T > 60.f)
    return 0.f;
  float svp = 0.61078f * expf(17.27f * T / (T + 237.3f));
  float avp = svp * (rh / 100.0f);
  return svp - avp;
}

static void layoutButtons() {
  int LX = 6;
  int y0 = HDR_H + 4;
  int bw = X_LEFT_END - 12;
  int bh = 44;
  int gap = 5;
  const char *labels[] = {"LIGHT", "HEAT", "CIRC", "EXH"};
  RelaySlot slots[] = {REL_LIGHT, REL_HEAT, REL_CIRC, REL_EXH};
  for (int i = 0; i < 4; i++) {
    buttons[i].label = labels[i];
    buttons[i].x = LX;
    buttons[i].y = y0 + i * (bh + gap);
    buttons[i].w = bw;
    buttons[i].h = bh;
    buttons[i].slot = slots[i];
  }
}

static bool resolveSensor() {
  IPAddress ip = MDNS.queryHost("grow-sensor", 1500);
  if (ip == IPAddress(0, 0, 0, 0))
    return false;
  sensorIp = ip;
  return true;
}

static bool sensorHttpBegin(HTTPClient &http, const char *path) {
  if (sensorIp == IPAddress(0, 0, 0, 0) && !resolveSensor())
    return false;
  String url = String("http://") + sensorIp.toString() + ":" +
               String((unsigned)SENSOR_HTTP_PORT) + path;
  return http.begin(url);
}

bool httpPostJson(const char *path, const String &json) {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  HTTPClient http;
  http.setTimeout(2500);
  if (!sensorHttpBegin(http, path))
    return false;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.POST(json);
  http.end();
  if (code < 0)
    sensorIp = IPAddress(0, 0, 0, 0);
  return code == 200;
}

static void pushHistorySample(float t, float rh, float vpd) {
  if (nHist < HIST_CAP) {
    histT[nHist] = t;
    histRh[nHist] = rh;
    histVpd[nHist] = vpd;
    nHist++;
  } else {
    memmove(histT, histT + 1, (HIST_CAP - 1) * sizeof(float));
    memmove(histRh, histRh + 1, (HIST_CAP - 1) * sizeof(float));
    memmove(histVpd, histVpd + 1, (HIST_CAP - 1) * sizeof(float));
    histT[HIST_CAP - 1] = t;
    histRh[HIST_CAP - 1] = rh;
    histVpd[HIST_CAP - 1] = vpd;
  }
}

bool fetchSummary() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  HTTPClient http;
  http.setTimeout(2500);
  if (!sensorHttpBegin(http, SUMMARY_PATH)) {
    return false;
  }
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();

  StaticJsonDocument<4096> doc;
  if (deserializeJson(doc, payload) || !doc["ok"].as<bool>())
    return false;

  JsonObject c = doc["control"];
  {
    const char *m = c["control_mode"];
    snprintf(state.mode, sizeof(state.mode), "%s", m ? m : "?");
    const char *phase = c["phase"] | "?";
    snprintf(state.phase, sizeof(state.phase), "%s", phase);
  }
  state.growWeek = c["grow_week"] | 0;
  state.havePhTarget = !c["ph_target"].isNull();
  state.haveEcTarget = !c["ec_target_ms_cm"].isNull();
  state.phTarget = c["ph_target"] | 0.0f;
  state.ecTarget = c["ec_target_ms_cm"] | 0.0f;
  state.profileConfigured = c["phase_profile_configured"] | false;
  state.targetAirC = c["phase_air_target_c"] | 0.0f;
  state.targetHumidity = c["phase_humidity_target_pct"] | 0.0f;
  state.c3Connected = doc["actuators"]["connected"] | false;
  JsonObject t = c["targets"];
  state.light = t["light"] | false;
  state.heat = t["heater"] | false;
  state.circ = t["circulation_vent"] | false;
  state.exh = t["exhaust_vent"] | false;

  JsonObject r = doc["reading"];
  state.haveReading = !r.isNull();
  state.haveAir = false;
  state.havePh = false;
  state.haveWater = false;
  state.haveEc = false;
  state.haveEcMeasurementC = false;
  state.haveLux = false;
  state.haveAreaLumens = false;
  state.phStatus[0] = 0;
  if (state.haveReading) {
    state.haveAir = !r["air_temperature"].isNull() && !r["humidity"].isNull();
    state.tempC = r["air_temperature"] | 0.0f;
    state.rh = r["humidity"] | 0.0f;
    state.ec = r["ec"] | 0.0f;
    state.havePh = !r["ph"].isNull();
    state.haveEc = !r["ec"].isNull();
    state.ph = r["ph"] | 0.0f;
    const char *phAlarm = r["ph_alarm"] | "";
    snprintf(state.phStatus, sizeof(state.phStatus), "%s", phAlarm);
    state.tdsPpm = r["tds_ppm"] | 0.0f;
    JsonVariant water = r["water_temperature"];
    if (!water.isNull() && isfinite(water.as<float>())) {
      state.waterC = water.as<float>();
      state.haveWater = true;
    }
    JsonVariant ecMeasurement = r["ec_measurement_temperature_c"];
    if (state.haveEc && !ecMeasurement.isNull() &&
        isfinite(ecMeasurement.as<float>())) {
      state.ecMeasurementC = ecMeasurement.as<float>();
      state.haveEcMeasurementC = true;
    }
    // Neue Sensor-API: explizites null = BH1750 fehlt.
    // Alte Sensor-API: light_level (0 konnte auch "kein BH1750" bedeuten).
    JsonVariant luxReading = r["illuminance_lux"];
    if (!luxReading.isNull() && isfinite(luxReading.as<float>()) &&
        luxReading.as<float>() >= 0.0f) {
      state.lux = luxReading.as<float>();
      state.haveLux = true;
    } else if (!r.containsKey("illuminance_lux")) {
      JsonVariant legacyLux = r["light_level"];
      if (!legacyLux.isNull() && isfinite(legacyLux.as<float>()) &&
          legacyLux.as<float>() > 0.0f) {
        state.lux = legacyLux.as<float>();
        state.haveLux = true;
      }
    }
    JsonVariant lumenReading = r["estimated_incident_lumens"];
    if (state.haveLux && !lumenReading.isNull() &&
        isfinite(lumenReading.as<float>()) && lumenReading.as<float>() >= 0.0f) {
      state.areaLumens = lumenReading.as<float>();
      state.haveAreaLumens = true;
    } else if (state.haveLux && !r.containsKey("estimated_incident_lumens")) {
      // Altgeraet: lokaler Schaetzwert bei 1 m2, niemals Lampen-Gesamtleistung.
      state.areaLumens = state.lux * 1.0f;
      state.haveAreaLumens = true;
    }
    if (state.haveAir) {
      float vpd = computeVpd(state.tempC, state.rh);
      // Nur reale Klimamesswerte in die lokale Historie uebernehmen.
      pushHistorySample(state.tempC, state.rh, vpd);
    }
  }

  lastSensorResponseOk = true;
  return true;
}

bool fetchAlertsFromServer() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  HTTPClient http;
  if (!growHttpBegin(http, "/api/v1/alerts/recent?limit=5"))
    return false;
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }
  String payload = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, payload) || !doc["ok"].as<bool>())
    return false;

  state.nAlerts = 0;
  JsonArray al = doc["alerts"].as<JsonArray>();
  for (size_t i = 0; i < al.size() && state.nAlerts < 5; i++) {
    JsonObject a = al[i];
    const char *m = a["message"];
    const char *s = a["severity"];
    snprintf(state.alertLines[state.nAlerts], sizeof(state.alertLines[0]),
             "%s: %s", s ? s : "?", m ? m : "?");
    state.nAlerts++;
  }
  return true;
}

bool fetchSeriesFromServer() {
  if (WiFi.status() != WL_CONNECTED)
    return false;
  HTTPClient http;
  String path = String("/api/v1/telemetry/series?hours=8&max_points=") + String((int)HIST_CAP);
  if (!growHttpBegin(http, path)) {
    haveServerSeries = false;
    return false;
  }
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) {
    http.end();
    haveServerSeries = false;
    return false;
  }
  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(20480);
  if (deserializeJson(doc, payload)) {
    haveServerSeries = false;
    return false;
  }
  if (!doc["ok"].as<bool>()) {
    haveServerSeries = false;
    return false;
  }
  JsonArray pts = doc["points"].as<JsonArray>();
  seriesN = 0;
  for (size_t i = 0; i < pts.size() && seriesN < HIST_CAP; i++) {
    JsonObject p = pts[i];
    seriesT[seriesN] = p["air_temperature"] | 0.0f;
    seriesRh[seriesN] = p["humidity"] | 0.0f;
    if (p.containsKey("vpd"))
      seriesVpd[seriesN] = p["vpd"] | 0.0f;
    else
      seriesVpd[seriesN] = computeVpd(seriesT[seriesN], seriesRh[seriesN]);
    seriesN++;
  }
  haveServerSeries = (seriesN >= 2);
  return haveServerSeries;
}

static void uiText(int x, int y, const char *label, uint16_t color = TFT_WHITE) {
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(color, 0x1082);
  tft.drawString(label, x, y);
}

static void uiButton(int x, int y, int w, int h, const char *label,
                     bool selected = false, bool available = true) {
  const uint16_t fill = !available ? 0x4208 : selected ? 0x3463 : 0x2104;
  tft.fillRoundRect(x, y, w, h, 5, fill);
  tft.drawRoundRect(x, y, w, h, 5, selected ? 0x07E0 : available ? 0xBDF7 : 0x632C);
  tft.setTextSize(1);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(available ? TFT_WHITE : 0x8410, fill);
  tft.drawString(label, x + w / 2, y + h / 2);
}

static void uiFeedback() {
  if (!uiMessage[0]) return;
  tft.fillRect(3, PAGE_BOTTOM - 14, 314, 12, 0x1082);
  uiText(7, PAGE_BOTTOM - 13, uiMessage, 0xFFE0);
}

static void drawHeader() {
  tft.fillRect(0, 0, 320, HDR_H, 0x0C11);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE, 0x0C11);
  tft.drawString("GROW CONTROLLER", 5, 6);
  if (WiFi.status() == WL_CONNECTED && lastSensorResponseOk)
    tft.drawString("SENSOR OK", 245, 6);
  else
    tft.drawString("OFFLINE", 257, 6);
}

static void drawNavigation() {
  static const char *tabs[] = {"LIVE", "PHASE", "SOLL", "KURVE", "INFO"};
  for (int i = 0; i < 5; ++i)
    uiButton(i * 64 + 1, PAGE_BOTTOM + 1, 62, NAV_H - 3,
             tabs[i], i == static_cast<int>(screenPage));
}

static void drawLivePage() {
  char ln[55];
  uiText(8, 29, "KLIMA", 0xFFE0);
  if (state.haveAir) {
    snprintf(ln, sizeof(ln), "Luft  %.1f C", state.tempC);
    uiText(8, 49, ln);
    snprintf(ln, sizeof(ln), "rH    %.0f %%", state.rh);
    uiText(8, 68, ln);
    snprintf(ln, sizeof(ln), "VPD   %.2f kPa", computeVpd(state.tempC, state.rh));
    uiText(8, 87, ln);
  } else {
    uiText(8, 49, "DHT: --", 0xFC00);
  }
  uiText(8, 119, "PHASE / WOCHE", 0xFFE0);
  snprintf(ln, sizeof(ln), "%s / W%u", state.phase, state.growWeek);
  uiText(8, 139, ln);
  uiText(166, 29, "WASSER / LICHT", 0xFFE0);
  if (state.haveWater) {
    snprintf(ln, sizeof(ln), "DS %.2f C", state.waterC);
    uiText(166, 47, ln);
  } else uiText(166, 47, "DS --");
  if (state.havePh) {
    snprintf(ln, sizeof(ln), "pH %.3f", state.ph);
    uiText(166, 64, ln);
  } else uiText(166, 64, "pH --");
  if (state.haveEc) {
    snprintf(ln, sizeof(ln), "EC %.3f mS/cm", state.ec);
    uiText(166, 81, ln);
    if (state.haveEcMeasurementC)
      snprintf(ln, sizeof(ln), "bei DS %.1f C", state.ecMeasurementC);
    else snprintf(ln, sizeof(ln), "Bezug: 25C (alte FW)");
    uiText(166, 98, ln, 0xBDF7);
  } else uiText(166, 81, "EC --");
  if (state.haveLux) {
    snprintf(ln, sizeof(ln), "BH %.0f lux", state.lux);
    uiText(166, 122, ln);
    if (state.haveAreaLumens) {
      snprintf(ln, sizeof(ln), "Beet ~%.0f lm", state.areaLumens);
      uiText(166, 139, ln);
    }
  } else uiText(166, 122, "BH1750 --");
  if (!strcmp(state.phStatus, "high")) uiText(8, 169, "pH ALARM HOCH", TFT_RED);
  else if (!strcmp(state.phStatus, "low")) uiText(8, 169, "pH ALARM NIEDRIG", TFT_RED);
  else if (!strcmp(state.phStatus, "sensor_fault")) uiText(8, 169, "pH SENSORFEHLER", TFT_RED);
}

static void drawPhasePage() {
  char ln[64];
  uiText(8, 29, "PHASE WAEHLEN (NVS)", 0xFFE0);
  uiButton(9, 52, 146, 46, "WUCHS", !strcmp(state.phase, "wuchs"));
  uiButton(165, 52, 146, 46, "BLUETE", !strcmp(state.phase, "bluete"));
  snprintf(ln, sizeof(ln), "ANBAUWOCHE: %u", state.growWeek);
  uiText(103, 111, ln, 0xFFE0);
  uiButton(13, 132, 72, 46, "W -", state.growWeek == 1, state.growWeek > 1);
  uiButton(235, 132, 72, 46, "W +", state.growWeek == 12, state.growWeek < 12);
  uiText(104, 146, "1 BIS 12");
  uiText(105, 161, "SPEICHERN: AUTO");
}

static void drawSollPage() {
  char ln[64];
  uiText(8, 29, "WOCHEN-SOLLWERTE (NICHT RELAIS)", 0xFFE0);
  if (state.havePhTarget && state.haveEcTarget) {
    snprintf(ln, sizeof(ln), "W%u: pH %.2f  EC %.2f mS/cm",
             state.growWeek, state.phTarget, state.ecTarget);
    uiText(8, 47, ln);
  } else uiText(8, 47, "Woche nicht gewaehlt");
  if (state.profileConfigured) {
    snprintf(ln, sizeof(ln), "Klima: %.1f C  %.0f %% rH",
             state.targetAirC, state.targetHumidity);
    uiText(8, 64, ln);
  } else uiText(8, 64, "Klima-Profil: NICHT DEFINIERT", 0xFC00);
  uiText(8, 84, "Betriebsmodus (keine C3-Schaltung):", 0xFFE0);
  uiButton(9, 104, 146, 47, "AUTOMATIK", !strcmp(state.mode, "auto"));
  uiButton(165, 104, 146, 47, "MANUELL", !strcmp(state.mode, "manual"));
  uiText(8, 161, "pH/EC-Sollwerte: Wahl ueber WOCHE");
  uiText(8, 176, "Keine Dosierautomatik vorhanden");
}

static void drawInfoPage() {
  char ln[74];
  uiText(8, 29, "HARDWARE / MELDUNGEN", 0xFFE0);
  if (state.c3Connected) uiText(8, 47, "ESP32-C3: verbunden", 0x07E0);
  else uiText(8, 47, "ESP32-C3: NICHT VERBUNDEN", TFT_RED);
  uiText(8, 63, "Relais: KEINE SCHALTFUNKTION");
  if (!strcmp(state.phStatus, "high")) uiText(8, 82, "pH HIGH >6.2", TFT_RED);
  else if (!strcmp(state.phStatus, "low")) uiText(8, 82, "pH LOW <5.5", TFT_RED);
  else if (!strcmp(state.phStatus, "sensor_fault")) uiText(8, 82, "pH SENSORFEHLER", TFT_RED);
  else uiText(8, 82, "pH: kein aktueller Alarm");
  uiText(8, 103, "SERVER-ALARME", 0xFFE0);
  if (state.nAlerts == 0) {
    uiText(8, 121, "Keine Meldungen / Server offline");
  } else {
    for (uint8_t i = 0; i < state.nAlerts && i < 5; ++i) {
      // TFT_eSPI rendert ueberlange Server-Texte sonst ausserhalb des Schirms.
      snprintf(ln, sizeof(ln), "%.46s", state.alertLines[i]);
      uiText(8, 121 + i * 14, ln);
    }
  }
}

static void drawSparkline(int x, int y, int gw, int gh, const float *buf, uint8_t n, uint16_t col) {
  tft.fillRect(x, y, gw, gh, 0x0000);
  tft.drawRect(x, y, gw, gh, 0x632C);
  if (n < 2)
    return;
  float vmin = buf[0], vmax = buf[0];
  for (uint8_t i = 1; i < n; i++) {
    if (buf[i] < vmin) vmin = buf[i];
    if (buf[i] > vmax) vmax = buf[i];
  }
  if (vmax - vmin < 1e-3f) {
    vmin -= 0.5f;
    vmax += 0.5f;
  }
  float pad = (vmax - vmin) * 0.1f;
  vmin -= pad;
  vmax += pad;
  int plotY = y + 2;
  int plotH = gh - 4;
  int plotW = gw - 4;
  int plotX = x + 2;
  for (uint8_t i = 1; i < n; i++) {
    float x0f = (float)(i - 1) / (float)(n - 1) * plotW;
    float x1f = (float)i / (float)(n - 1) * plotW;
    int y0 = plotY + plotH - 1 - (int)((buf[i - 1] - vmin) / (vmax - vmin) * (plotH - 1));
    int y1 = plotY + plotH - 1 - (int)((buf[i] - vmin) / (vmax - vmin) * (plotH - 1));
    tft.drawLine(plotX + (int)x0f, y0, plotX + (int)x1f, y1, col);
  }
}

static void drawChartPage() {
  const float *srcT = haveServerSeries ? seriesT : histT;
  const float *srcH = haveServerSeries ? seriesRh : histRh;
  const float *srcV = haveServerSeries ? seriesVpd : histVpd;
  const uint8_t n = haveServerSeries ? seriesN : nHist;
  uiText(8, 27, haveServerSeries ? "HISTORIE DB / 8h" : "HISTORIE LOKAL", 0xFFE0);
  uiText(8, 44, "Lufttemperatur C");
  drawSparkline(8, 55, 304, 32, srcT, n, TFT_ORANGE);
  uiText(8, 96, "Luftfeuchte %");
  drawSparkline(8, 107, 304, 32, srcH, n, TFT_CYAN);
  uiText(8, 148, "VPD kPa");
  drawSparkline(8, 159, 304, 32, srcV, n, TFT_MAGENTA);
}

static void redrawAll() {
  tft.fillScreen(0x1082);
  drawHeader();
  if (WiFi.status() != WL_CONNECTED) {
    uiText(8, 27, "WLAN NICHT VERBUNDEN", TFT_RED);
  } else if (!lastSensorResponseOk) {
    uiText(8, 27, "SENSOR-API NICHT ERREICHBAR", TFT_RED);
  }
  if (lastSensorResponseOk) {
    switch (screenPage) {
      case ScreenPage::Live: drawLivePage(); break;
      case ScreenPage::Phase: drawPhasePage(); break;
      case ScreenPage::Soll: drawSollPage(); break;
      case ScreenPage::Chart: drawChartPage(); break;
      case ScreenPage::Info: drawInfoPage(); break;
    }
  }
  uiFeedback();
  drawNavigation();
}

bool readTouchPixel(int *outX, int *outY) {
  if (!touchscreen.touched())
    return false;
  TS_Point p = touchscreen.getPoint();
  while (touchscreen.touched())
    delay(10);
  *outX = map(constrain(p.x, CAL_X_MIN, CAL_X_MAX), CAL_X_MIN, CAL_X_MAX, 0, tft.width() - 1);
  *outY = map(constrain(p.y, CAL_Y_MIN, CAL_Y_MAX), CAL_Y_MIN, CAL_Y_MAX, 0, tft.height() - 1);
  return true;
}

static bool postPhase(const char *requested) {
  StaticJsonDocument<64> doc;
  doc["phase"] = requested;
  String body;
  serializeJson(doc, body);
  const bool ok = httpPostJson(PHASE_PATH, body);
  if (ok) fetchSummary();
  snprintf(uiMessage, sizeof(uiMessage), "%s",
           ok ? "Phase dauerhaft gespeichert"
              : "Phase NICHT gespeichert (FW/API pruefen)");
  return ok;
}

static bool postWeek(uint8_t requested) {
  if (requested < 1 || requested > 12) return false;
  StaticJsonDocument<64> doc;
  doc["week"] = requested;
  String body;
  serializeJson(doc, body);
  const bool ok = httpPostJson(WEEK_PATH, body);
  if (ok) fetchSummary();
  snprintf(uiMessage, sizeof(uiMessage), "%s",
           ok ? "Woche dauerhaft gespeichert"
              : "Woche NICHT gespeichert (API pruefen)");
  return ok;
}

static bool postMode(const char *requested) {
  StaticJsonDocument<64> doc;
  doc["mode"] = requested;
  String body;
  serializeJson(doc, body);
  const bool ok = httpPostJson(MODE_PATH, body);
  if (ok) fetchSummary();
  snprintf(uiMessage, sizeof(uiMessage), "%s",
           ok ? "Modus gesetzt; C3 weiter ohne Relais"
              : "Modus nicht gespeichert (API pruefen)");
  return ok;
}

static void handleTouch(int x, int y) {
  if (y >= PAGE_BOTTOM) {
    int next = constrain(x / 64, 0, 4);
    screenPage = static_cast<ScreenPage>(next);
    uiMessage[0] = '\0';
    redrawAll();
    return;
  }
  if (!lastSensorResponseOk) return;
  if (screenPage == ScreenPage::Phase) {
    if (y >= 52 && y < 99) {
      if (x < 160) postPhase("wuchs");
      else postPhase("bluete");
    } else if (y >= 132 && y < 179) {
      if (x < 90 && state.growWeek > 1)
        postWeek(static_cast<uint8_t>(state.growWeek - 1));
      else if (x > 230 && state.growWeek < 12)
        postWeek(static_cast<uint8_t>(state.growWeek + 1));
    }
    redrawAll();
  } else if (screenPage == ScreenPage::Soll && y >= 104 && y < 152) {
    postMode(x < 160 ? "auto" : "manual");
    redrawAll();
  }
}

void setup() {
  Serial.begin(115200);
  memset(&state, 0, sizeof(state));
  nHist = 0;
  snprintf(state.phase, sizeof(state.phase), "?");
  snprintf(state.mode, sizeof(state.mode), "?");
  layoutButtons();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(tft.getRotation());

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // Bedienoberflaeche auch bei nicht erreichbarem WLAN verfuegbar lassen.
  const unsigned long startWifiMs = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startWifiMs < 10000UL)
    delay(250);
  if (WiFi.status() == WL_CONNECTED) {
    MDNS.begin("grow-cyd");
    resolveSensor();
    lastSensorResponseOk = fetchSummary();
    fetchAlertsFromServer();
    fetchSeriesFromServer();
  }
  redrawAll();
}

void loop() {
  const unsigned long now = millis();
  bool needRedraw = false;
  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    if (WiFi.status() != WL_CONNECTED) {
      WiFi.reconnect();
      lastSensorResponseOk = false;
      needRedraw = true;
    } else {
      lastSensorResponseOk = fetchSummary();
      fetchAlertsFromServer();
      needRedraw = true;
    }
  }
  if (now - lastSeriesPoll >= SERIES_POLL_MS && WiFi.status() == WL_CONNECTED) {
    lastSeriesPoll = now;
    fetchSeriesFromServer();
    needRedraw = true;
  }
  if (needRedraw) redrawAll();

  if (now - lastTouch >= TOUCH_DEBOUNCE_MS) {
    int tx = 0, ty = 0;
    if (readTouchPixel(&tx, &ty)) {
      lastTouch = millis();
      handleTouch(tx, ty);
    }
  }
}
