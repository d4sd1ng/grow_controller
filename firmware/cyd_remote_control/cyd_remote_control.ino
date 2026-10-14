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
const char *CONTROL_PATH = "/api/local/control/targets";
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
const unsigned long TOUCH_DEBOUNCE_MS = 450;

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
  char alertLines[5][72];
  uint8_t nAlerts;
} state;

enum RelaySlot { REL_LIGHT = 0, REL_HEAT = 1, REL_CIRC = 2, REL_EXH = 3 };

struct RelayBtn {
  const char *label;
  int x, y, w, h;
  RelaySlot slot;
};

RelayBtn buttons[4];

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

  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, payload) || !doc["ok"].as<bool>())
    return false;

  JsonObject c = doc["control"];
  {
    const char *m = c["control_mode"];
    snprintf(state.mode, sizeof(state.mode), "%s", m ? m : "?");
  }
  JsonObject t = c["targets"];
  state.light = t["light"] | false;
  state.heat = t["heater"] | false;
  state.circ = t["circulation_vent"] | false;
  state.exh = t["exhaust_vent"] | false;

  JsonObject r = doc["reading"];
  state.haveReading = !r.isNull();
  state.haveWater = false;
  state.haveEc = false;
  state.phStatus[0] = 0;
  if (state.haveReading) {
    state.tempC = r["air_temperature"] | 0.0f;
    state.rh = r["humidity"] | 0.0f;
    state.ec = r["ec"] | 0.0f;
    state.haveEc = !r["ec"].isNull();
    state.ph = r["ph"] | 0.0f;
    const char *phAlarm = r["ph_alarm"] | "";
    snprintf(state.phStatus, sizeof(state.phStatus), "%s", phAlarm);
    state.tdsPpm = r["tds_ppm"] | 0.0f;
    JsonVariant water = r["water_temperature"];
    if (!water.isNull()) {
      state.waterC = water.as<float>();
      state.haveWater = true;
    }
    float vpd = computeVpd(state.tempC, state.rh);
    /* nur bei echten Messungen Historie füllen */
    pushHistorySample(state.tempC, state.rh, vpd);
  }

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

static void drawSeparator(int x) {
  tft.drawFastVLine(x, HDR_H, tft.height() - HDR_H, 0x5AEB);
  tft.drawFastVLine(x + 1, HDR_H, tft.height() - HDR_H, 0x1082);
}

static void drawHeader() {
  tft.fillRect(0, 0, tft.width(), HDR_H, 0x0C11);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, 0x0C11);
  tft.setTextSize(1);
  tft.drawString("GROW REMOTE", tft.width() / 2, HDR_H / 2 + 1);
}

static void drawRelayBtn(const RelayBtn &b, bool on) {
  uint16_t fill = on ? 0x0340 : 0x39C7;
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 5, fill);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 5, TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, fill);
  char line[16];
  snprintf(line, sizeof(line), "%s %s", b.label, on ? "ON" : "off");
  tft.drawString(line, b.x + b.w / 2, b.y + b.h / 2);
}

static void drawLeftPanel() {
  tft.fillRect(0, HDR_H, X_LEFT_END, tft.height() - HDR_H, 0x2104);
  for (int i = 0; i < 4; i++)
    drawRelayBtn(buttons[i],
                 (buttons[i].slot == REL_LIGHT)  ? state.light
                 : (buttons[i].slot == REL_HEAT) ? state.heat
                 : (buttons[i].slot == REL_CIRC) ? state.circ
                                                 : state.exh);
}

static void drawCenterPanel() {
  char ln[48];
  int x0 = X_LEFT_END + 2;
  int y = HDR_H + 4;
  tft.fillRect(X_LEFT_END, HDR_H, X_MID_END - X_LEFT_END, tft.height() - HDR_H, 0x2965);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(0xFFE0, 0x2965);
  tft.drawString("MESSWERTE", x0, y);
  y += 12;
  tft.setTextColor(TFT_WHITE, 0x2965);
  if (state.haveReading) {
    snprintf(ln, sizeof(ln), "T %.1f C   rH %.0f%%", state.tempC, state.rh);
    tft.drawString(ln, x0, y);
    y += 12;
    if (state.haveEc)
      snprintf(ln, sizeof(ln), "EC %.3f   pH %.2f", state.ec, state.ph);
    else
      snprintf(ln, sizeof(ln), "EC --   pH %.2f", state.ph);
    tft.drawString(ln, x0, y);
    y += 12;
    snprintf(ln, sizeof(ln), "VPD %.2f kPa", computeVpd(state.tempC, state.rh));
    tft.drawString(ln, x0, y);
    y += 12;
    if (state.haveWater) {
      snprintf(ln, sizeof(ln), "H2O %.1f C", state.waterC);
      tft.drawString(ln, x0, y);
      y += 12;
    }
  } else {
    tft.drawString("(keine Daten)", x0, y);
    y += 14;
  }
  snprintf(ln, sizeof(ln), "Modus: %s", state.mode);
  tft.setTextColor(0x7BEF, 0x2965);
  tft.drawString(ln, x0, y);
  y += 16;

  tft.setTextColor(0xFC9F, 0x2965);
  tft.drawString("BENACHRICHTIGUNGEN", x0, y);
  y += 11;
  tft.setTextColor(TFT_WHITE, 0x2965);
  // Live-Alarm direkt vom ESP, nicht auf verzögerte Server-Alerts warten.
  if (!strcmp(state.phStatus, "low") || !strcmp(state.phStatus, "high") ||
      !strcmp(state.phStatus, "sensor_fault")) {
    tft.setTextColor(TFT_RED, 0x2965);
    const char *alarmText = !strcmp(state.phStatus, "low") ? "pH ALARM <5.5" :
                            !strcmp(state.phStatus, "high") ? "pH ALARM >6.2" :
                            "pH SENSOR/KAL FEHLER";
    tft.drawString(alarmText, x0, y);
    y += 11;
    tft.setTextColor(TFT_WHITE, 0x2965);
  }
  if (state.nAlerts == 0) {
    tft.setTextColor(0x8D79, 0x2965);
    tft.drawString("— keine —", x0, y);
  } else {
    for (uint8_t i = 0; i < state.nAlerts; i++) {
      tft.drawString(state.alertLines[i], x0, y);
      y += 11;
      if (y > tft.height() - 8)
        break;
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

static void drawRightPanel() {
  int x0 = X_MID_END + 2;
  int pw = tft.width() - x0 - 4;
  tft.fillRect(X_MID_END, HDR_H, tft.width() - X_MID_END, tft.height() - HDR_H, 0x1082);

  const float *srcT = haveServerSeries ? seriesT : histT;
  const float *srcH = haveServerSeries ? seriesRh : histRh;
  const float *srcV = haveServerSeries ? seriesVpd : histVpd;
  uint8_t nT = haveServerSeries ? seriesN : nHist;
  uint8_t nH = haveServerSeries ? seriesN : nHist;
  uint8_t nV = haveServerSeries ? seriesN : nHist;

  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(1);
  tft.setTextColor(0x9CF0, 0x1082);
  tft.drawString(haveServerSeries ? "Charts: DB ~8h" : "Charts: local", x0, HDR_H + 2);

  int usableH = tft.height() - HDR_H - 14;
  int ch = usableH / 3;
  int yBase = HDR_H + 10;

  tft.setTextColor(0xFCA0, 0x1082);
  tft.drawString("Temp C", x0, yBase);
  yBase += 9;
  drawSparkline(x0, yBase, pw, ch - 18, srcT, nT, TFT_ORANGE);
  yBase += ch;

  tft.setTextColor(0xB7FF, 0x1082);
  tft.drawString("rH %", x0, yBase);
  yBase += 9;
  drawSparkline(x0, yBase, pw, ch - 18, srcH, nH, TFT_CYAN);
  yBase += ch;

  tft.setTextColor(0xF81F, 0x1082);
  tft.drawString("VPD kPa", x0, yBase);
  yBase += 9;
  drawSparkline(x0, yBase, pw, tft.height() - yBase - 3, srcV, nV, TFT_MAGENTA);
}

static void redrawAll() {
  tft.fillScreen(0x1082);
  drawHeader();
  drawSeparator(X_LEFT_END);
  drawSeparator(X_MID_END);
  drawLeftPanel();
  drawCenterPanel();
  drawRightPanel();
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

bool relayState(RelaySlot s) {
  switch (s) {
    case REL_LIGHT:
      return state.light;
    case REL_HEAT:
      return state.heat;
    case REL_CIRC:
      return state.circ;
    case REL_EXH:
      return state.exh;
  }
  return false;
}

void toggleRelayPost(RelaySlot s) {
  const char *key = nullptr;
  bool newVal = !relayState(s);
  switch (s) {
    case REL_LIGHT:
      key = "light";
      state.light = newVal;
      break;
    case REL_HEAT:
      key = "heater";
      state.heat = newVal;
      break;
    case REL_CIRC:
      key = "circulation_vent";
      state.circ = newVal;
      break;
    case REL_EXH:
      key = "exhaust_vent";
      state.exh = newVal;
      break;
  }
  StaticJsonDocument<64> doc;
  doc[key] = newVal;
  String body;
  serializeJson(doc, body);
  if (!httpPostJson(CONTROL_PATH, body))
    fetchSummary();
}

void setup() {
  Serial.begin(115200);
  memset(&state, 0, sizeof(state));
  nHist = 0;
  layoutButtons();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(tft.getRotation());

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("WiFi...", tft.width() / 2, tft.height() / 2);
  while (WiFi.status() != WL_CONNECTED)
    delay(300);
  MDNS.begin("grow-cyd");
  resolveSensor();
  fetchSummary();
  fetchAlertsFromServer();
  fetchSeriesFromServer();
  redrawAll();
}

void loop() {
  unsigned long now = millis();

  bool needRedraw = false;
  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    needRedraw |= fetchSummary();
    fetchAlertsFromServer();
  }
  if (now - lastSeriesPoll >= SERIES_POLL_MS) {
    lastSeriesPoll = now;
    needRedraw |= fetchSeriesFromServer();
  }
  if (needRedraw)
    redrawAll();

  if (now - lastTouch >= TOUCH_DEBOUNCE_MS) {
    int tx, ty;
    if (readTouchPixel(&tx, &ty)) {
      lastTouch = now;
      if (tx < X_LEFT_END) {
        for (unsigned i = 0; i < 4; i++) {
          const RelayBtn &b = buttons[i];
          if (tx >= b.x && tx <= b.x + b.w && ty >= b.y && ty <= b.y + b.h) {
            toggleRelayPost(b.slot);
            redrawAll();
            break;
          }
        }
      }
    }
  }
}
