/**
 * CYD als 3-Tab-Fernbedienung mit LVGL v8:
 *  Tab Relais: Touch-Buttons
 *  Tab Werte: Messwerte + Alerts
 *  Tab Charts: LVGL Charts (Temp, RH, VPD)
 *
 * Libs: lvgl (v8), TFT_eSPI, XPT2046_Touchscreen, ArduinoJson v6
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <math.h>
#include <string.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>

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

const unsigned long POLL_MS = 4000;
const unsigned long SERIES_POLL_MS = 25000;

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

#define HIST_CAP 120

// State
struct RemoteState {
  char mode[12];
  bool heat, pump, circ, exh;
  bool haveReading;
  float tempC, rh, ec, ph, waterC;
  bool haveWater;
  char alertLines[5][72];
  uint8_t nAlerts;
} state;

float histT[HIST_CAP], histRh[HIST_CAP], histVpd[HIST_CAP];
uint8_t nHist = 0;

float seriesT[HIST_CAP], seriesRh[HIST_CAP], seriesVpd[HIST_CAP];
uint8_t seriesN = 0;
bool haveServerSeries = false;

// LVGL display
TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[320 * 10];

// LVGL widgets
static lv_obj_t *tabview;
static lv_obj_t *btn_relay[4];
static lv_obj_t *lbl_temp, *lbl_rh, *lbl_ecph, *lbl_vpd, *lbl_water, *lbl_mode;
static lv_obj_t *lbl_alerts[5];
static lv_obj_t *chart;
static lv_chart_series_t *ser_temp, *ser_rh, *ser_vpd;

unsigned long lastPoll = 0;
unsigned long lastSeriesPoll = 0;

// Chart data buffers (LVGL needs lv_coord_t arrays)
static lv_coord_t chartBufT[HIST_CAP];
static lv_coord_t chartBufRh[HIST_CAP];
static lv_coord_t chartBufVpd[HIST_CAP];

static float computeVpd(float T, float rh) {
  if (T < -40.f || T > 60.f) return 0.f;
  float svp = 0.61078f * expf(17.27f * T / (T + 237.3f));
  float avp = svp * (rh / 100.0f);
  return svp - avp;
}

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
  if (touchscreen.touched()) {
    TS_Point p = touchscreen.getPoint();
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = map(constrain(p.x, CAL_X_MIN, CAL_X_MAX), CAL_X_MIN, CAL_X_MAX, 0, 319);
    data->point.y = map(constrain(p.y, CAL_Y_MIN, CAL_Y_MAX), CAL_Y_MIN, CAL_Y_MAX, 0, 239);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

bool httpPostJson(const char *path, const String &json) {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  if (!growHttpBegin(http, path)) return false;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.POST(json);
  http.end();
  return code == 200;
}

static void pushHistorySample(float t, float rh, float vpd) {
  if (nHist < HIST_CAP) {
    histT[nHist] = t; histRh[nHist] = rh; histVpd[nHist] = vpd;
    nHist++;
  } else {
    memmove(histT, histT + 1, (HIST_CAP - 1) * sizeof(float));
    memmove(histRh, histRh + 1, (HIST_CAP - 1) * sizeof(float));
    memmove(histVpd, histVpd + 1, (HIST_CAP - 1) * sizeof(float));
    histT[HIST_CAP - 1] = t; histRh[HIST_CAP - 1] = rh; histVpd[HIST_CAP - 1] = vpd;
  }
}

static void relayBtnEvent(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_target(e);
  int idx = (int)(intptr_t)lv_obj_get_user_data(btn);
  const char *key = nullptr;
  bool *statePtr = nullptr;
  switch (idx) {
    case 0: key = "heater"; statePtr = &state.heat; break;
    case 1: key = "water_pump"; statePtr = &state.pump; break;
    case 2: key = "circulation_vent"; statePtr = &state.circ; break;
    case 3: key = "exhaust_vent"; statePtr = &state.exh; break;
  }
  if (!key) return;
  *statePtr = !(*statePtr);
  StaticJsonDocument<64> doc;
  doc[key] = *statePtr;
  String body;
  serializeJson(doc, body);
  if (!httpPostJson(CONTROL_PATH, body)) {
    *statePtr = !(*statePtr);
  } else {
    lv_color_t col = *statePtr ? lv_color_hex(0x0340) : lv_color_hex(0x39C7);
    lv_obj_set_style_bg_color(btn, col, 0);
  }
}

void updateRelayButtons() {
  bool vals[] = {state.heat, state.pump, state.circ, state.exh};
  const char *labels[] = {"HEAT", "PUMP", "CIRC", "EXH"};
  for (int i = 0; i < 4; i++) {
    if (!btn_relay[i]) continue;
    lv_color_t col = vals[i] ? lv_color_hex(0x0340) : lv_color_hex(0x39C7);
    lv_obj_set_style_bg_color(btn_relay[i], col, 0);
    lv_obj_set_style_text_color(btn_relay[i], lv_color_hex(0x000000), 0);
    lv_label_set_text_fmt(lv_obj_get_child(btn_relay[i], 0), "%s %s", labels[i], vals[i] ? "ON" : "off");
  }
}

void updateValueLabels() {
  if (state.haveReading) {
    lv_label_set_text_fmt(lbl_temp, "Temp: %.1f C", state.tempC);
    lv_label_set_text_fmt(lbl_rh, "Luftfeuchte: %.0f %%", state.rh);
    lv_label_set_text_fmt(lbl_ecph, "EC %.2f | pH %.1f", state.ec, state.ph);
    lv_label_set_text_fmt(lbl_vpd, "VPD: %.2f kPa", computeVpd(state.tempC, state.rh));
    if (state.haveWater) {
      lv_label_set_text_fmt(lbl_water, "Wasser: %.1f C", state.waterC);
    } else {
      lv_label_set_text_static(lbl_water, "Wasser: --");
    }
  } else {
    lv_label_set_text_static(lbl_temp, "Temp: --");
    lv_label_set_text_static(lbl_rh, "Luftfeuchte: --");
    lv_label_set_text_static(lbl_ecph, "EC -- | pH --");
    lv_label_set_text_static(lbl_vpd, "VPD: --");
    lv_label_set_text_static(lbl_water, "Wasser: --");
  }
  lv_label_set_text_fmt(lbl_mode, "Modus: %s", state.mode);

  for (int i = 0; i < 5; i++) {
    if (i < state.nAlerts) {
      lv_label_set_text(lbl_alerts[i], state.alertLines[i]);
      lv_obj_set_style_text_color(lbl_alerts[i], lv_color_hex(0xFCA000), 0);
    } else if (i == 0 && state.nAlerts == 0) {
      lv_label_set_text_static(lbl_alerts[0], "Keine Alerts");
      lv_obj_set_style_text_color(lbl_alerts[0], lv_color_hex(0x8D79), 0);
    } else {
      lv_label_set_text_static(lbl_alerts[i], "");
    }
  }
}

void updateCharts() {
  const float *srcT = haveServerSeries ? seriesT : histT;
  const float *srcH = haveServerSeries ? seriesRh : histRh;
  const float *srcV = haveServerSeries ? seriesVpd : histVpd;
  uint8_t n = haveServerSeries ? seriesN : nHist;
  if (n < 2) return;

  float tMin = srcT[0], tMax = srcT[0];
  float hMin = srcH[0], hMax = srcH[0];
  float vMin = srcV[0], vMax = srcV[0];
  for (uint8_t i = 1; i < n; i++) {
    if (srcT[i] < tMin) tMin = srcT[i]; if (srcT[i] > tMax) tMax = srcT[i];
    if (srcH[i] < hMin) hMin = srcH[i]; if (srcH[i] > hMax) hMax = srcH[i];
    if (srcV[i] < vMin) vMin = srcV[i]; if (srcV[i] > vMax) vMax = srcV[i];
  }
  if (tMax - tMin < 1.0f) { tMin -= 0.5f; tMax += 0.5f; }
  if (hMax - hMin < 1.0f) { hMin -= 0.5f; hMax += 0.5f; }
  if (vMax - vMin < 0.1f) { vMin -= 0.05f; vMax += 0.05f; }

  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, (lv_coord_t)(tMin * 10), (lv_coord_t)(tMax * 10));
  lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, (lv_coord_t)(hMin), (lv_coord_t)(hMax));

  for (uint8_t i = 0; i < n; i++) {
    chartBufT[i] = (lv_coord_t)(srcT[i] * 10);
    chartBufRh[i] = (lv_coord_t)(srcH[i]);
    chartBufVpd[i] = (lv_coord_t)(srcV[i] * 100);
  }

  lv_chart_set_ext_y_array(chart, ser_temp, chartBufT);
  lv_chart_set_ext_y_array(chart, ser_rh, chartBufRh);
  lv_chart_set_ext_y_array(chart, ser_vpd, chartBufVpd);
  lv_chart_set_point_count(chart, n);
  lv_chart_refresh(chart);
}

bool fetchSummary() {
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  if (!growHttpBegin(http, SUMMARY_PATH)) return false;
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) { http.end(); return false; }
  String payload = http.getString();
  http.end();

  StaticJsonDocument<3072> doc;
  if (deserializeJson(doc, payload) || !doc["ok"].as<bool>()) return false;

  JsonObject c = doc["control"];
  {
    const char *m = c["control_mode"];
    snprintf(state.mode, sizeof(state.mode), "%s", m ? m : "?");
  }
  JsonObject t = c["targets"];
  state.heat = t["heater"] | false;
  state.pump = (t["pump"] | false) || (t["water_pump"] | false);
  state.circ = t["circulation_vent"] | false;
  state.exh = t["exhaust_vent"] | false;

  JsonObject r = doc["reading"];
  state.haveReading = !r.isNull();
  state.haveWater = false;
  if (state.haveReading) {
    state.tempC = r["air_temperature"] | 0.0f;
    state.rh = r["humidity"] | 0.0f;
    state.ec = r["ec"] | 0.0f;
    state.ph = r["ph"] | 0.0f;
    if (r.containsKey("water_temperature")) {
      state.waterC = r["water_temperature"] | 0.0f;
      state.haveWater = true;
    }
    pushHistorySample(state.tempC, state.rh, computeVpd(state.tempC, state.rh));
  }

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
  if (WiFi.status() != WL_CONNECTED) return false;
  HTTPClient http;
  String path = String("/api/v1/telemetry/series?hours=8&max_points=") + String((int)HIST_CAP);
  if (!growHttpBegin(http, path)) { haveServerSeries = false; return false; }
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) { http.end(); haveServerSeries = false; return false; }
  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(20480);
  if (deserializeJson(doc, payload)) { haveServerSeries = false; return false; }
  if (!doc["ok"].as<bool>()) { haveServerSeries = false; return false; }

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

void build_ui() {
  tabview = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 28);

  // --- Tab 1: Relais ---
  lv_obj_t *tab1 = lv_tabview_add_tab(tabview, "Relais");
  lv_obj_t *btn_cont = lv_obj_create(tab1);
  lv_obj_set_size(btn_cont, 300, 200);
  lv_obj_set_pos(btn_cont, 6, 2);
  lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_cont, 0, 0);

  const char *labels[] = {"HEAT", "PUMP", "CIRC", "EXH"};
  for (int i = 0; i < 4; i++) {
    btn_relay[i] = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_relay[i], 200, 38);
    lv_obj_set_user_data(btn_relay[i], (void *)(intptr_t)i);
    lv_obj_add_event_cb(btn_relay[i], relayBtnEvent, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn_relay[i]);
    lv_label_set_text_fmt(lbl, "%s off", labels[i]);
    lv_obj_center(lbl);
  }

  // --- Tab 2: Werte ---
  lv_obj_t *tab2 = lv_tabview_add_tab(tabview, "Werte");
  lv_obj_t *val_cont = lv_obj_create(tab2);
  lv_obj_set_size(val_cont, 308, 200);
  lv_obj_set_pos(val_cont, 2, 2);
  lv_obj_set_flex_flow(val_cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(val_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(val_cont, 4, 0);
  lv_obj_set_style_bg_opa(val_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(val_cont, 0, 0);

  lbl_mode = lv_label_create(val_cont);
  lv_label_set_text_static(lbl_mode, "Modus: ?");

  lbl_temp = lv_label_create(val_cont);
  lv_label_set_text_static(lbl_temp, "Temp: --");

  lbl_rh = lv_label_create(val_cont);
  lv_label_set_text_static(lbl_rh, "Luftfeuchte: --");

  lbl_ecph = lv_label_create(val_cont);
  lv_label_set_text_static(lbl_ecph, "EC -- | pH --");

  lbl_vpd = lv_label_create(val_cont);
  lv_label_set_text_static(lbl_vpd, "VPD: --");

  lbl_water = lv_label_create(val_cont);
  lv_label_set_text_static(lbl_water, "Wasser: --");

  lv_obj_t *sep = lv_label_create(val_cont);
  lv_label_set_text_static(sep, "--- Alerts ---");

  for (int i = 0; i < 5; i++) {
    lbl_alerts[i] = lv_label_create(val_cont);
    lv_obj_set_width(lbl_alerts[i], 300);
    lv_label_set_long_mode(lbl_alerts[i], LV_LABEL_LONG_DOT);
    lv_label_set_text_static(lbl_alerts[i], "");
  }

  // --- Tab 3: Charts ---
  lv_obj_t *tab3 = lv_tabview_add_tab(tabview, "Charts");

  chart = lv_chart_create(tab3);
  lv_obj_set_size(chart, 308, 180);
  lv_obj_set_pos(chart, 2, 2);
  lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
  lv_chart_set_update_mode(chart, LV_CHART_UPDATE_MODE_SHIFT);
  lv_chart_set_point_count(chart, HIST_CAP);
  lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 100, 400);
  lv_chart_set_range(chart, LV_CHART_AXIS_SECONDARY_Y, 0, 100);

  ser_temp = lv_chart_add_series(chart, lv_color_hex(0xFCA000), LV_CHART_AXIS_PRIMARY_Y);
  ser_rh = lv_chart_add_series(chart, lv_color_hex(0x00B7FF), LV_CHART_AXIS_SECONDARY_Y);
  ser_vpd = lv_chart_add_series(chart, lv_color_hex(0xF81F), LV_CHART_AXIS_PRIMARY_Y);

  lv_obj_t *chart_lbl = lv_label_create(tab3);
  lv_obj_set_pos(chart_lbl, 4, 186);
  lv_label_set_text_static(chart_lbl, "Orange=Temp Cyan=RH Magenta=VPD");
}

void setup() {
  Serial.begin(115200);
  memset(&state, 0, sizeof(state));
  nHist = 0;

  lv_init();

  tft.begin();
  tft.setRotation(1);

  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(tft.getRotation());

  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  build_ui();

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  lv_timer_handler();

  if (WiFi.status() != WL_CONNECTED) {
    delay(100);
    return;
  }

  unsigned long now = millis();
  bool updated = false;

  if (now - lastPoll >= POLL_MS) {
    lastPoll = now;
    if (fetchSummary()) {
      updated = true;
      updateRelayButtons();
      updateValueLabels();
    }
  }

  if (now - lastSeriesPoll >= SERIES_POLL_MS) {
    lastSeriesPoll = now;
    if (fetchSeriesFromServer()) {
      updated = true;
    }
  }

  if (updated) {
    updateCharts();
  }

  delay(5);
}
