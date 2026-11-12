/**
 * Portable Cheap Yellow Display mit LVGL v8:
 * Kompakte Anzeige/Steuerung direkt gegen den Sensor-ESP.
 *
 * GET /api/local/summary -> reading + control.targets
 * Relais umschalten: POST /api/local/control/targets
 *
 * Libs: lvgl (v8), TFT_eSPI, XPT2046_Touchscreen, ArduinoJson v6
 *
 * WICHTIG — TFT_eSPI Setup fuer CYD (ESP32-2432S028R):
 *   In User_Setup.h oder platformio.ini muss definiert sein:
 *     #define TFT_MISO 12
 *     #define TFT_MOSI 13
 *     #define TFT_SCLK 14
 *     #define TFT_CS   15
 *     #define TFT_DC    2
 *     #define TFT_RST  -1
 *     #define TFT_BL   21
 *     #define SPI_FREQUENCY  40000000
 *     #define LOAD_GLCD
 *     #define LOAD_FONT2
 *     #define LOAD_FONT4
 *     #define SMOOTH_FONT
 *   sowie: #define USE_HSPI_PORT  (fuer getrenntes Touch-SPI)
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ESPmDNS.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <math.h>
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

const char *SUMMARY_PATH = "/api/local/summary";
const char *CONTROL_PATH = "/api/local/control/targets";
const uint16_t SENSOR_HTTP_PORT = 8081;
IPAddress sensorIp;
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
static lv_color_t buf1[320 * 20];

// LVGL widgets
static lv_obj_t *lbl_mode;
static lv_obj_t *lbl_temp;
static lv_obj_t *lbl_hum;
static lv_obj_t *lbl_ecph;
static lv_obj_t *lbl_water;
static lv_obj_t *lbl_light;
static lv_obj_t *lbl_relay;
static lv_obj_t *lbl_alert;
static lv_obj_t *lbl_wifi;
static lv_obj_t *lbl_c3;
static lv_obj_t *lbl_pumps;
static lv_obj_t *btn_water;
static lv_obj_t *btn_fan;
static lv_obj_t *btn_mode;

static lv_obj_t *btn_heater;
static lv_obj_t *btn_light;
static lv_obj_t *btn_circ;
static lv_obj_t *btn_exh;

static bool relay_heater = false;
static bool relay_light = false;
static bool relay_circ = false;
static bool relay_exh = false;
static bool relay_water = false;
static bool relay_fan = false;
static bool c3_ready = false;
static bool manual_mode = false;
static uint8_t c3_active_pump = 0;
static constexpr uint8_t PUMP_COUNT = 6;
static lv_obj_t *pumpButtons[PUMP_COUNT] = {};
static const char *PUMP_IDS[PUMP_COUNT] = {"1", "2", "3", "4", "5", "6"};
static const char *PUMP_PATH = "/api/local/control/pump";
static const char *MODE_PATH = "/api/local/control/mode";
bool httpPostJson(const char *path, const String &jsonBody);

unsigned long lastFetch = 0;
unsigned long lastWifiReconnect = 0;
const unsigned long WIFI_RECONNECT_MS = 5000;

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

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

static void setBtnState(lv_obj_t *btn, bool on) {
  if (on) {
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E7D32), 0);
  } else {
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x424242), 0);
  }
}

static void btnToggleRelay(lv_event_t *e) {
  lv_obj_t *btn = lv_event_get_target(e);
  const char *key = (const char *)lv_obj_get_user_data(btn);
  bool current = false;
  if (strcmp(key, "light") == 0) current = relay_light;
  else if (strcmp(key, "heater") == 0) current = relay_heater;
  else if (strcmp(key, "circulation_vent") == 0) current = relay_circ;
  else if (strcmp(key, "exhaust_vent") == 0) current = relay_exh;
  else if (strcmp(key, "water_pump") == 0) current = relay_water;
  else if (strcmp(key, "lamp_cooling") == 0) current = relay_fan;
  else return;

  if (!c3_ready || !manual_mode) {
    lv_label_set_text_static(lbl_alert, "C3 gesperrt oder Modus AUTO");
    return;
  }
  lv_obj_set_style_bg_color(btn, lv_color_hex(0xFCA000), 0);

  StaticJsonDocument<64> doc;
  doc[key] = !current;
  String body;
  serializeJson(doc, body);
  if (!httpPostJson(CONTROL_PATH, body))
    lv_label_set_text_static(lbl_alert, "Relaisbefehl nicht angenommen");
}


static void btnToggleMode(lv_event_t *e) {
  (void)e;
  StaticJsonDocument<64> doc;
  doc["mode"] = manual_mode ? "auto" : "manual";
  String body;
  serializeJson(doc, body);
  if (!httpPostJson(MODE_PATH, body))
    lv_label_set_text_static(lbl_alert, "Moduswechsel fehlgeschlagen");
}
static void btnPumpPulse(lv_event_t *e) {
  if (!c3_ready || !manual_mode) {
    lv_label_set_text_static(lbl_alert, "Pumpen gesperrt");
    return;
  }
  const char *id = static_cast<const char *>(lv_event_get_user_data(e));
  const int index = atoi(id);
  if (index < 1 || index > PUMP_COUNT) return;
  StaticJsonDocument<96> doc;
  doc["pump"] = index;
  doc["duration_ms"] = 500; // begrenzter manueller Test, kein Dosierplan
  String body;
  serializeJson(doc, body);
  if (!httpPostJson(PUMP_PATH, body))
    lv_label_set_text_static(lbl_alert, "Pumpenimpuls abgelehnt");
  else
    lv_label_set_text_fmt(lbl_alert, "Pumpenimpuls P%d angefragt", index);
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
  http.setTimeout(5000);
  if (!sensorHttpBegin(http, path))
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

void updateLabels(const char *mode, float t, float rh, float tdsPpm, float ph,
                  float ec, bool ecValid,
                  float waterC, bool waterValid,
                  float ecMeasurementC, bool ecMeasurementValid,
                  float lux, bool luxValid, float areaLumens, bool areaLumensValid,
                  bool light, bool heat, bool circ, bool exh,
                  const char *alert, const char *alertSev) {
  relay_light = light;
  relay_heater = heat;
  relay_circ = circ;
  relay_exh = exh;

  lv_label_set_text_fmt(lbl_mode, "Modus: %s", mode ? mode : "?");
  lv_label_set_text_fmt(lbl_temp, "Temperatur: %.1f C", t);
  lv_label_set_text_fmt(lbl_hum, "Luftfeuchte: %.0f %%", rh);
  if (ecValid) lv_label_set_text_fmt(lbl_ecph, "EC %.3f mS/cm | pH %.2f", ec, ph);
  else lv_label_set_text_fmt(lbl_ecph, "EC -- | pH %.2f", ph);
  if (ecValid && ecMeasurementValid)
    lv_label_set_text_fmt(lbl_water, "EC bei DS18B20 %.2f C", ecMeasurementC);
  else if (waterValid && ecValid)
    lv_label_set_text_fmt(lbl_water, "Wasser %.2f C | EC@25C (alte FW)", waterC);
  else if (waterValid)
    lv_label_set_text_fmt(lbl_water, "Wasser %.2f C | EC nicht verfuegbar", waterC);
  else
    lv_label_set_text_static(lbl_water, "Wasser/EC-Temperatur: --");
  if (luxValid && areaLumensValid)
    lv_label_set_text_fmt(lbl_light, "BH %.0f lx | Beet ~%.0f lm (1 m2)", lux, areaLumens);
  else if (luxValid)
    lv_label_set_text_fmt(lbl_light, "BH %.0f lx | Lumen --", lux);
  else
    lv_label_set_text_static(lbl_light, "BH1750: --");
  lv_label_set_text_fmt(lbl_relay, "L:%s H:%s C:%s E:%s",
                        light ? "ON" : "off",
                        heat ? "ON" : "off",
                        circ ? "ON" : "off",
                        exh ? "ON" : "off");
  if (alert && strlen(alert) > 0) {
    lv_label_set_text_fmt(lbl_alert, "[%s] %s", alertSev ? alertSev : "?", alert);
    lv_obj_set_style_text_color(lbl_alert, lv_color_hex(0xFCA000), 0);
  } else {
    lv_label_set_text_static(lbl_alert, "Keine Alerts");
    lv_obj_set_style_text_color(lbl_alert, lv_color_hex(0x8D79), 0);
  }

  setBtnState(btn_light, light);
  setBtnState(btn_heater, heat);
  setBtnState(btn_circ, circ);
  setBtnState(btn_exh, exh);
}

void fetchSummaryAndUpdate() {
  HTTPClient http;
  http.setTimeout(2500);
  if (!sensorHttpBegin(http, SUMMARY_PATH)) {
    lv_label_set_text_static(lbl_wifi, "HTTP-Fehler");
    return;
  }
  http.setTimeout(5000);
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

  StaticJsonDocument<4096> doc;
  if (deserializeJson(doc, payload) || !doc["ok"].as<bool>()) {
    lv_label_set_text_static(lbl_wifi, "JSON-Fehler");
    return;
  }

  JsonObject c = doc["control"];
  const char *mode = c["control_mode"];

  float t = 0, rh = 0, tdsPpm = 0, ph = 0, ec = 0;
  float waterC = 0, ecMeasurementC = 0, lux = 0, areaLumens = 0;
  bool ecValid = false, waterValid = false, ecMeasurementValid = false;
  bool luxValid = false, areaLumensValid = false;
  JsonObject r = doc["reading"];
  if (!r.isNull()) {
    t = r["air_temperature"] | 0.0f;
    rh = r["humidity"] | 0.0f;
    tdsPpm = r["tds_ppm"] | 0.0f;
    ph = r["ph"] | 0.0f;
    ec = r["ec"] | 0.0f;
    ecValid = !r["ec"].isNull();
    JsonVariant waterReading = r["water_temperature"];
    if (!waterReading.isNull() && isfinite(waterReading.as<float>())) {
      waterC = waterReading.as<float>();
      waterValid = true;
    }
    JsonVariant ecTempReading = r["ec_measurement_temperature_c"];
    if (ecValid && !ecTempReading.isNull() && isfinite(ecTempReading.as<float>())) {
      ecMeasurementC = ecTempReading.as<float>();
      ecMeasurementValid = true;
    }
    JsonVariant luxReading = r["illuminance_lux"];
    if (!luxReading.isNull() && isfinite(luxReading.as<float>()) &&
        luxReading.as<float>() >= 0.0f) {
      lux = luxReading.as<float>();
      luxValid = true;
    } else if (!r.containsKey("illuminance_lux")) {
      // Aeltere Sensor-Firmware hatte nur light_level; dort 0 auch bei fehlendem BH.
      JsonVariant oldLuxReading = r["light_level"];
      if (!oldLuxReading.isNull() && isfinite(oldLuxReading.as<float>()) &&
          oldLuxReading.as<float>() > 0.0f) {
        lux = oldLuxReading.as<float>();
        luxValid = true;
      }
    }
    JsonVariant areaLumenReading = r["estimated_incident_lumens"];
    if (luxValid && !areaLumenReading.isNull() &&
        isfinite(areaLumenReading.as<float>()) &&
        areaLumenReading.as<float>() >= 0.0f) {
      areaLumens = areaLumenReading.as<float>();
      areaLumensValid = true;
    } else if (luxValid && !r.containsKey("estimated_incident_lumens")) {
      // Schaetzung fuer 1 m2, keine Lampen-Gesamt-Lumen.
      areaLumens = lux * 1.0f;
      areaLumensValid = true;
    }
  }

  bool light = false, heat = false, circ = false, exh = false;
  JsonObject targets = c["targets"];
  if (!targets.isNull()) {
    light = targets["light"] | false;
    heat = targets["heater"] | false;
    circ = targets["circulation_vent"] | false;
    exh = targets["exhaust_vent"] | false;
  }

  // pH-Alarm direkt aus der lokalen ESP32-Zusammenfassung lesen.
  const char *phAlarm = r["ph_alarm"] | "sensor_fault";
  const char *alertMsg = nullptr;
  const char *alertSev = nullptr;
  if (!strcmp(phAlarm, "low")) { alertMsg = "pH unter 5.5"; alertSev = "ALARM"; }
  else if (!strcmp(phAlarm, "high")) { alertMsg = "pH ueber 6.2"; alertSev = "ALARM"; }
  else if (!strcmp(phAlarm, "sensor_fault")) { alertMsg = "pH-Sensor/Kalibrierung"; alertSev = "FEHLER"; }

  updateLabels(mode, t, rh, tdsPpm, ph, ec, ecValid,
               waterC, waterValid, ecMeasurementC, ecMeasurementValid,
               lux, luxValid, areaLumens, areaLumensValid,
               light, heat, circ, exh, alertMsg, alertSev);
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
  lv_obj_set_style_pad_all(cont, 4, 0);
  lv_obj_set_style_pad_row(cont, 2, 0); // zwei Licht-/Wassertemperaturzeilen ohne UI-Ueberlauf
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

  lbl_water = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_water, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text_static(lbl_water, "Wasser/EC-Temperatur: --");

  lbl_light = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_light, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text_static(lbl_light, "BH1750: --");

  lbl_relay = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_relay, lv_color_hex(0xB7FF), 0);
  lv_label_set_text_static(lbl_relay, "Relais: --");

  lbl_alert = lv_label_create(cont);
  lv_obj_set_style_text_color(lbl_alert, lv_color_hex(0x8D79), 0);
  lv_label_set_long_mode(lbl_alert, LV_LABEL_LONG_DOT);
  lv_obj_set_width(lbl_alert, 300);
  lv_label_set_text_static(lbl_alert, "Keine Alerts");

  // Relay toggle buttons
  lv_obj_t *btn_cont = lv_obj_create(cont);
  lv_obj_set_size(btn_cont, 300, 44);
  lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(btn_cont, 2, 0);
  lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(btn_cont, 0, 0);

  btn_light = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_light, 68, 34);
  lv_obj_t *l = lv_label_create(btn_light);
  lv_label_set_text_static(l, "LIGHT");
  lv_obj_center(l);
  lv_obj_set_user_data(btn_light, (void *)"light");
  lv_obj_add_event_cb(btn_light, btnToggleRelay, LV_EVENT_CLICKED, NULL);

  btn_heater = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_heater, 68, 34);
  l = lv_label_create(btn_heater);
  lv_label_set_text_static(l, "HEAT");
  lv_obj_center(l);
  lv_obj_set_user_data(btn_heater, (void *)"heater");
  lv_obj_add_event_cb(btn_heater, btnToggleRelay, LV_EVENT_CLICKED, NULL);

  btn_circ = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_circ, 68, 34);
  l = lv_label_create(btn_circ);
  lv_label_set_text_static(l, "CIRC");
  lv_obj_center(l);
  lv_obj_set_user_data(btn_circ, (void *)"circulation_vent");
  lv_obj_add_event_cb(btn_circ, btnToggleRelay, LV_EVENT_CLICKED, NULL);

  btn_exh = lv_btn_create(btn_cont);
  lv_obj_set_size(btn_exh, 68, 34);
  l = lv_label_create(btn_exh);
  lv_label_set_text_static(l, "EXH");
  lv_obj_center(l);
  lv_obj_set_user_data(btn_exh, (void *)"exhaust_vent");
  lv_obj_add_event_cb(btn_exh, btnToggleRelay, LV_EVENT_CLICKED, NULL);
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
  lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 20);
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
  uint32_t ms = lv_timer_handler();

  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastWifiReconnect >= WIFI_RECONNECT_MS) {
      lastWifiReconnect = now;
      WiFi.reconnect();
    }
    lv_label_set_text_static(lbl_wifi, "WiFi verbinden...");
    delay(ms > 0 ? ms : 10);
    return;
  }

  static bool mdnsStarted = false;
  if (!mdnsStarted) {
    mdnsStarted = MDNS.begin("grow-cyd-lvgl");
    if (mdnsStarted) resolveSensor();
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

  delay(ms > 0 ? ms : 5);
}
