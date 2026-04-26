/**
 * Portable Cheap Yellow Display mit LVGL v8:
 * Zentrale Mobilanzeige gegen den Server.
 *
 * GET /api/v1/mobile/summary → reading, control.targets, alerts
 * Relais umschalten: POST /api/v1/control/targets
 *
 * Libs: lvgl (v8), TFT_eSPI, XPT2046_Touchscreen, ArduinoJson v6
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
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
const unsigned long POLL_MS = 3000;

// Touch
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass touchscreenSPI = SPIClass(HSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS, XPT2046_IRQ);

// Display
TFT_eSPI tft;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[320 * 10];

// LVGL widgets
static lv_obj_t *lbl_mode;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_hum;
static lv_obj_t *lbl_ecph;
static lv_obj_t *lbl_relay;
static lv_obj_t *lbl_alert;
static lv_obj_t *lbl_wifi;

unsigned long lastFetch = 0;

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
    data->point.x = map(constrain(p.x, 200, 3700), 200, 3700, 0, 319);
    data->point.y = map(constrain(p.y, 240, 3800), 240, 3800, 0, 239);
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
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

void exampleToggleHeater(bool on) {
  StaticJsonDocument<64> doc;
  doc["heater"] = on;
  String body;
  serializeJson(doc, body);
  httpPostJson(CONTROL_PATH, body);
}

void updateLabels(const char *mode, float t, float rh, float ec, float ph,
                  bool heat, bool pump, bool circ, bool exh,
                  const char *alert, const char *alertSev) {
  lv_label_set_text_fmt(lbl_mode, "Modus: %s", mode ? mode : "?");
  lv_label_set_text_fmt(lbl_temp, "Temperatur: %.1f C", t);
  lv_label_set_text_fmt(lbl_hum, "Luftfeuchte: %.0f %%", rh);
  lv_label_set_text_fmt(lbl_ecph, "EC %.2f | pH %.1f", ec, ph);
  lv_label_set_text_fmt(lbl_relay, "H:%s P:%s C:%s E:%s",
                        heat ? "ON" : "off",
                        pump ? "ON" : "off",
                        circ ? "ON" : "off",
                        exh ? "ON" : "off");
  if (alert && strlen(alert) > 0) {
    lv_label_set_text_fmt(lbl_alert, "[%s] %s", alertSev ? alertSev : "?", alert);
    lv_obj_set_style_text_color(lbl_alert, lv_color_hex(0xFCA000), 0);
  } else {
    lv_label_set_text_static(lbl_alert, "Keine Alerts");
    lv_obj_set_style_text_color(lbl_alert, lv_color_hex(0x8D79), 0);
  }
}

void fetchSummaryAndUpdate() {
  HTTPClient http;
  if (!growHttpBegin(http, SUMMARY_PATH)) {
    lv_label_set_text_static(lbl_wifi, "HTTP-Fehler");
    return;
  }
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) {
    lv_label_set_text_fmt(lbl_wifi, "HTTP %d", code);
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, payload) || !doc["ok"].as<bool>()) {
    lv_label_set_text_static(lbl_wifi, "JSON-Fehler");
    return;
  }

  JsonObject c = doc["control"];
  const char *mode = c["control_mode"];

  float t = 0, rh = 0, ec = 0, ph = 0;
  JsonObject r = doc["reading"];
  if (!r.isNull()) {
    t = r["air_temperature"] | 0.0f;
    rh = r["humidity"] | 0.0f;
    ec = r["ec"] | 0.0f;
    ph = r["ph"] | 0.0f;
  }

  bool heat = false, pump = false, circ = false, exh = false;
  JsonObject targets = c["targets"];
  if (!targets.isNull()) {
    heat = targets["heater"] | false;
    pump = (targets["pump"] | false) || (targets["water_pump"] | false);
    circ = targets["circulation_vent"] | false;
    exh = targets["exhaust_vent"] | false;
  }

  const char *alertMsg = nullptr;
  const char *alertSev = nullptr;
  JsonArray alerts = doc["alerts"].as<JsonArray>();
  if (alerts.size() > 0) {
    JsonObject a0 = alerts[0];
    alertMsg = a0["message"];
    alertSev = a0["severity"];
  }

  updateLabels(mode, t, rh, ec, ph, heat, pump, circ, exh, alertMsg, alertSev);
  lv_label_set_text_fmt(lbl_wifi, "WiFi OK | %s", WiFi.localIP().toString().c_str());
}

void build_ui() {
  lv_obj_t *scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0);

  lv_obj_t *cont = lv_obj_create(scr);
  lv_obj_set_size(cont, 316, 236);
  lv_obj_set_pos(cont, 2, 2);
  lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_all(cont, 6, 0);
  lv_obj_set_style_bg_color(cont, lv_color_hex(0x0C11), 0);
  lv_obj_set_style_border_width(cont, 0, 0);

  lbl_wifi = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_wifi, lv_color_hex(0x9CF0), 0);
  lv_label_set_text_static(lbl_wifi, "Verbinde...");

  lbl_mode = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_mode, lv_color_hex(0xFFE0), 0);
  lv_label_set_text_static(lbl_mode, "Modus: ?");

  lbl_temp = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_temp, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text_static(lbl_temp, "Temperatur: --");

  lbl_hum = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_hum, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text_static(lbl_hum, "Luftfeuchte: --");

  lbl_ecph = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_ecph, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text_static(lbl_ecph, "EC -- | pH --");

  lbl_relay = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_relay, lv_color_hex(0xB7FF), 0);
  lv_label_set_text_static(lbl_relay, "Relais: --");

  lbl_alert = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_alert, lv_color_hex(0x8D79), 0);
  lv_label_set_long_mode(lbl_alert, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lbl_alert, 300);
  lv_label_set_text_static(lbl_alert, "Keine Alerts");
}

void setup() {
  Serial.begin(115200);

  // LVGL init
  lv_init();

  // TFT
  tft.begin();
  tft.setRotation(1);

  // Touch
  touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  touchscreen.begin(touchscreenSPI);
  touchscreen.setRotation(tft.getRotation());

  // Display buffer
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 10);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = 320;
  disp_drv.ver_res = 240;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Touch input
  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);

  build_ui();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  lv_timer_handler();

  if (WiFi.status() != WL_CONNECTED) {
    lv_label_set_text_static(lbl_wifi, "WiFi verbinden...");
    delay(100);
    return;
  }

  unsigned long now = millis();
  if (now - lastFetch >= POLL_MS) {
    lastFetch = now;
    fetchSummaryAndUpdate();
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "H1")
      exampleToggleHeater(true);
    else if (cmd == "H0")
      exampleToggleHeater(false);
  }

  delay(5);
}
