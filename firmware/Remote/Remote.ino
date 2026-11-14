/*
  Grow Controller / Remote v2.0 (CYD LVGL)
  Target: ESP32-2432S028R (ILI9341, 320x240 landscape)
  Libraries: TFT_eSPI 2.5.x, LVGL 8.4.x, XPT2046_Touchscreen, ArduinoJson 7.x.
  External local files preserved: device_secrets.h, grow_http_client.h, lv_conf.h.

  Important: five pump channels are DISPLAYED, but dosing remains LOCKED until
  calibration and hardware integration have been verified. No dosing commands.
  Touch coordinates are initial CYD defaults, NOT a confirmed calibration.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <math.h>
#include <string.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
#error "device_secrets.h missing; generate it locally from .env before compiling"
#endif
#include "grow_http_client.h"

#if LV_COLOR_DEPTH != 16
#error "The CYD remote requires LV_COLOR_DEPTH 16 in lv_conf.h"
#endif

namespace {
constexpr uint16_t SCREEN_W = 320;
constexpr uint16_t SCREEN_H = 240;
constexpr uint8_t PUMP_COUNT = 5;  // Current agreed controller configuration.
constexpr uint32_t SUMMARY_INTERVAL_MS = 5000;
constexpr uint32_t C3_INTERVAL_MS = 8000;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t STOP_CONFIRM_MS = 4000;
constexpr bool TOUCH_ACTUATION_VERIFIED = false; // Not calibrated; no remote actuation until tested.

constexpr int TOUCH_IRQ  = 36;
constexpr int TOUCH_MOSI = 32;
constexpr int TOUCH_MISO = 39;
constexpr int TOUCH_CLK  = 25;
constexpr int TOUCH_CS   = 33;

constexpr uint32_t BG = 0x071D1A;
constexpr uint32_t PANEL = 0x13302B;
constexpr uint32_t BORDER = 0x28524A;
constexpr uint32_t TEXT = 0xF1F6F3;
constexpr uint32_t MUTED = 0xACC7BE;
constexpr uint32_t ACCENT = 0x21D59A;
constexpr uint32_t WARN = 0xFFBD69;
constexpr uint32_t OFF = 0x435C55;

constexpr char SUMMARY_PATH[] = "/api/v1/mobile/summary";
constexpr char CONTROL_PATH[] = "/api/v1/control/targets";

TFT_eSPI tft;
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
static lv_disp_draw_buf_t drawBuf;
static lv_color_t drawPixels[SCREEN_W * 20];

static lv_obj_t *pages[4] = {};
static lv_obj_t *navButtons[4] = {};
static lv_obj_t *topStatus = nullptr;
static lv_obj_t *homeValues[4] = {};
static lv_obj_t *homeMode = nullptr;
static lv_obj_t *homeTds = nullptr;
static lv_obj_t *homeVpd = nullptr;
static lv_obj_t *homeAlert = nullptr;
static lv_obj_t *climateButtons[3] = {};
static lv_obj_t *climateValues[3] = {};
static lv_obj_t *climateReadonly = nullptr;
static lv_obj_t *fanButtons[2] = {};
static lv_obj_t *fanValues[2] = {};
static lv_obj_t *c3Info = nullptr;
static lv_obj_t *stopButton = nullptr;
static lv_obj_t *pumpButtons[PUMP_COUNT] = {};
static lv_obj_t *pumpInfo = nullptr;
static lv_obj_t *pumpActive = nullptr;

static uint8_t currentPage = 0;
static bool serverReady = false;
static bool c3Online = false;
static bool c3HardwareReady = false;
static bool climateStates[3] = {};
static bool fanStates[2] = {};
static bool lightState = false;
static bool waterPumpState = false;
static int activeDoser = 0;
static bool pumpServiceOff = true;
static bool pumpServiceOffConfirmed = false;
static uint32_t lastSummary = 0;
static uint32_t lastC3 = 0;
static uint32_t lastWifiTry = 0;
static uint32_t stopConfirmAt = 0;
static char networkStatus[48] = "WLAN verbindet...";

lv_color_t color(uint32_t rgb) { return lv_color_hex(rgb); }

void textIfChanged(lv_obj_t *obj, const char *value) {
  if (!obj || !value) return;
  const char *old = lv_label_get_text(obj);
  if (!old || strcmp(old, value) != 0) lv_label_set_text(obj, value);
}

void textFormat(lv_obj_t *obj, const char *fmt, const char *arg) {
  char tmp[104];
  snprintf(tmp, sizeof(tmp), fmt, arg);
  textIfChanged(obj, tmp);
}

void stylePanel(lv_obj_t *obj, uint32_t bg = PANEL) {
  lv_obj_set_style_bg_color(obj, color(bg), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 1, 0);
  lv_obj_set_style_border_color(obj, color(BORDER), 0);
  lv_obj_set_style_radius(obj, 5, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t *makeLabel(lv_obj_t *parent, int x, int y, int width, const char *initial,
                    uint32_t rgb = TEXT, const lv_font_t *font = &lv_font_montserrat_14) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_pos(label, x, y);
  lv_obj_set_width(label, width);
  lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color(rgb), 0);
  lv_label_set_text(label, initial);  // Always a mutable LVGL-owned copy.
  return label;
}

lv_obj_t *makeButton(lv_obj_t *parent, int x, int y, int width, int height,
                     const char *title, uint32_t bg = OFF,
                     const lv_font_t *font = &lv_font_montserrat_14) {
  lv_obj_t *btn = lv_btn_create(parent);
  lv_obj_set_size(btn, width, height);
  lv_obj_set_pos(btn, x, y);
  stylePanel(btn, bg);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_t *text = lv_label_create(btn);
  lv_obj_set_style_text_font(text, font, 0);
  lv_obj_set_style_text_color(text, color(TEXT), 0);
  lv_label_set_text(text, title);
  lv_obj_center(text);
  return btn;
}

void setButtonEnabled(lv_obj_t *btn, bool enabled) {
  if (!btn) return;
  if (enabled) lv_obj_clear_state(btn, LV_STATE_DISABLED);
  else lv_obj_add_state(btn, LV_STATE_DISABLED);
}

void setButtonActive(lv_obj_t *btn, bool active, bool enabled) {
  if (!btn) return;
  lv_obj_set_style_bg_color(btn, color(active ? 0x12724F : OFF), 0);
  setButtonEnabled(btn, enabled);
}

void showPage(uint8_t pageIndex) {
  if (pageIndex > 3) return;
  currentPage = pageIndex;
  for (uint8_t i = 0; i < 4; ++i) {
    if (i == pageIndex) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(navButtons[i], color(i == pageIndex ? 0x157B58 : PANEL), 0);
  }
}

void onNavigate(lv_event_t *event) {
  const uint32_t i = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
  showPage((uint8_t)i);
}

void setNetworkStatus(const char *message) {
  if (!message) return;
  if (strcmp(networkStatus, message) != 0) {
    snprintf(networkStatus, sizeof(networkStatus), "%s", message);
    textIfChanged(topStatus, networkStatus);
  }
}

bool serverPost(const char *path, const String &payload) {
  if (WiFi.status() != WL_CONNECTED || !serverReady) return false;
  HTTPClient http;
  http.setTimeout(1600);
  if (!growHttpBegin(http, path)) return false;
  http.addHeader("Content-Type", "application/json");
  if (TELEMETRY_API_KEY && TELEMETRY_API_KEY[0])
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.POST(payload);
  http.end();
  return code >= 200 && code < 300;
}

bool c3Request(const char *path, const String &payload) {
  if (WiFi.status() != WL_CONNECTED || !c3Online || !ACTUATOR_HOST ||
      !ACTUATOR_HOST[0] || !TELEMETRY_API_KEY || !TELEMETRY_API_KEY[0]) return false;
  HTTPClient http;
  http.setTimeout(1600);
  String url = String("http://") + ACTUATOR_HOST + path;
  if (!http.begin(url)) return false;
  http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  http.end();
  return code >= 200 && code < 300;
}

void updateControls() {
  for (int i = 0; i < 3; ++i) {
    setButtonActive(climateButtons[i], climateStates[i], serverReady && TOUCH_ACTUATION_VERIFIED);
    textIfChanged(climateValues[i], serverReady ? (climateStates[i] ? "EIN" : "AUS") : "--");
  }
  char status[90];
  snprintf(status, sizeof(status), "Licht: %s    Wasserpumpe: %s",
           c3Online ? (lightState ? "EIN" : "AUS") : "--",
           c3Online ? (waterPumpState ? "EIN" : "AUS") : "--");
  textIfChanged(climateReadonly, status);
  for (int i = 0; i < 2; ++i) {
    setButtonActive(fanButtons[i], fanStates[i], c3Online && c3HardwareReady && TOUCH_ACTUATION_VERIFIED);
    textIfChanged(fanValues[i], c3Online ? (fanStates[i] ? "EIN" : "AUS") : "--");
  }
  textIfChanged(c3Info, !c3Online ? "C3: offline / nicht konfiguriert" :
               (c3HardwareReady ? "C3: online / freigegeben" : "C3: online / gesperrt"));
  setButtonEnabled(stopButton, c3Online);
  for (int i = 0; i < PUMP_COUNT; ++i) {
    char label[32];
    snprintf(label, sizeof(label), "P%d  %s", i + 1,
             (c3Online && activeDoser == i + 1) ? "AKTIV" : "GESPERRT");
    lv_obj_t *child = lv_obj_get_child(pumpButtons[i], 0);
    textIfChanged(child, label);
    setButtonEnabled(pumpButtons[i], false);  // Locked until a verified calibration.
  }
  snprintf(status, sizeof(status), "Wartung: %s",
           !c3Online ? "C3 offline" :
           (pumpServiceOff ? (pumpServiceOffConfirmed ? "PUMPEN AUS bestaetigt" : "PUMPEN AUS angefordert") : "nicht aktiv"));
  textIfChanged(pumpActive, status);
  textIfChanged(pumpInfo, "Dosierung gesperrt: Kalibrierung offen");
}

void onClimate(lv_event_t *event) {
  uint32_t i = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
  static const char *keys[] = {"heater", "circulation_vent", "exhaust_vent"};
  if (i >= 3 || !serverReady || !TOUCH_ACTUATION_VERIFIED) return;
  JsonDocument doc;
  doc[keys[i]] = !climateStates[i];
  String body;
  serializeJson(doc, body);
  if (!serverPost(CONTROL_PATH, body)) setNetworkStatus("Steuerbefehl fehlgeschlagen");
  lastSummary = millis() - SUMMARY_INTERVAL_MS;
}

void onFan(lv_event_t *event) {
  uint32_t i = (uint32_t)(uintptr_t)lv_event_get_user_data(event);
  if (i >= 2 || !c3Online || !c3HardwareReady || !TOUCH_ACTUATION_VERIFIED) return;
  JsonDocument doc;
  if (i == 0) doc["fan_group_1"] = !fanStates[0];
  else doc["fan_group_2"] = !fanStates[1];
  String body;
  serializeJson(doc, body);
  if (!c3Request("/api/v1/actuator/fans", body)) setNetworkStatus("C3 Befehl fehlgeschlagen");
  lastC3 = millis() - C3_INTERVAL_MS;
}

void onStop(lv_event_t *) {
  if (!c3Online) return;
  const uint32_t now = millis();
  if (!stopConfirmAt || now - stopConfirmAt > STOP_CONFIRM_MS) {
    stopConfirmAt = now;
    setNetworkStatus("STOP zum Bestaetigen erneut");
    return;
  }
  stopConfirmAt = 0;
  if (c3Request("/api/v1/actuator/stop", "{}")) {
    c3HardwareReady = false;
    setNetworkStatus("C3 STOP gesendet");
    updateControls();
  } else setNetworkStatus("C3 STOP nicht bestaetigt");
}

void makeMetric(lv_obj_t *page, int x, int y, const char *name, int index) {
  lv_obj_t *panel = lv_obj_create(page);
  lv_obj_set_size(panel, 148, 60);
  lv_obj_set_pos(panel, x, y);
  stylePanel(panel);
  makeLabel(panel, 8, 5, 130, name, MUTED, &lv_font_montserrat_12);
  homeValues[index] = makeLabel(panel, 8, 27, 130, "--", TEXT, &lv_font_montserrat_16);
}

void makeClimateRow(lv_obj_t *page, uint8_t i, int y, const char *label) {
  climateButtons[i] = makeButton(page, 8, y, 130, 37, label);
  lv_obj_add_event_cb(climateButtons[i], onClimate, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)i);
  climateValues[i] = makeLabel(page, 159, y + 10, 128, "--", ACCENT);
}

void buildUi() {
  lv_obj_t *screen = lv_scr_act();
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_bg_color(screen, color(BG), 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(screen, 0, 0);
  lv_obj_set_style_border_width(screen, 0, 0);

  makeLabel(screen, 8, 6, 122, "GROW CONTROL", ACCENT, &lv_font_montserrat_14);
  topStatus = makeLabel(screen, 143, 6, 169, networkStatus, MUTED, &lv_font_montserrat_12);

  for (int i = 0; i < 4; ++i) {
    pages[i] = lv_obj_create(screen);
    lv_obj_set_pos(pages[i], 0, 30);
    lv_obj_set_size(pages[i], SCREEN_W, 171);
    stylePanel(pages[i], BG);
    lv_obj_set_style_border_width(pages[i], 0, 0);
  }

  // Page 0: four well-spaced measurements and system summary.
  makeMetric(pages[0], 7, 4, "TEMPERATUR", 0);
  makeMetric(pages[0], 165, 4, "LUFTFEUCHTE", 1);
  makeMetric(pages[0], 7, 69, "pH-WERT", 2);
  makeMetric(pages[0], 165, 69, "EC", 3);
  homeMode = makeLabel(pages[0], 8, 138, 88, "Modus: --", MUTED, &lv_font_montserrat_12);
  homeVpd = makeLabel(pages[0], 98, 138, 130, "VPD: --", ACCENT, &lv_font_montserrat_12);
  homeTds = makeLabel(pages[0], 230, 138, 82, "TDS: --", MUTED, &lv_font_montserrat_12);
  homeAlert = makeLabel(pages[0], 8, 154, 303, "Keine Messdaten", WARN,
                        &lv_font_montserrat_12);

  // Page 1: climate switches; light / water pump are display-only.
  makeClimateRow(pages[1], 0, 9, "HEIZUNG");
  makeClimateRow(pages[1], 1, 56, "UMLUFT");
  makeClimateRow(pages[1], 2, 103, "ABLUFT");
  climateReadonly = makeLabel(pages[1], 8, 151, 306, "Licht: --   Wasserpumpe: --",
                              MUTED, &lv_font_montserrat_12);

  // Page 2: both C3 fan groups, status and a two-step stop.
  for (int i = 0; i < 2; ++i) {
    fanButtons[i] = makeButton(pages[2], 8, 11 + i * 49, 130, 39,
                               i == 0 ? "LUEFTER 1" : "LUEFTER 2");
    lv_obj_add_event_cb(fanButtons[i], onFan, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
    fanValues[i] = makeLabel(pages[2], 160, 23 + i * 49, 110, "--", ACCENT);
  }
  c3Info = makeLabel(pages[2], 8, 113, 300, "C3: nicht verbunden", MUTED,
                     &lv_font_montserrat_12);
  stopButton = makeButton(pages[2], 8, 135, 304, 30, "C3 STOP (2x tippen)",
                          0x77392E, &lv_font_montserrat_12);
  lv_obj_add_event_cb(stopButton, onStop, LV_EVENT_CLICKED, nullptr);

  // Page 3: all five physically planned pumps, permanently locked for now.
  for (int i = 0; i < PUMP_COUNT; ++i) {
    const int x = 7 + (i % 3) * 104;
    const int y = 7 + (i / 3) * 49;
    char name[24];
    snprintf(name, sizeof(name), "P%d  GESPERRT", i + 1);
    pumpButtons[i] = makeButton(pages[3], x, y, 98, 42, name, PANEL,
                                &lv_font_montserrat_12);
    setButtonEnabled(pumpButtons[i], false);
  }
  pumpActive = makeLabel(pages[3], 8, 115, 305, "Aktiver Kanal: unbekannt", MUTED,
                          &lv_font_montserrat_12);
  pumpInfo = makeLabel(pages[3], 8, 143, 305, "Dosierung gesperrt: Kalibrierung offen",
                       WARN, &lv_font_montserrat_12);

  // Fixed tab bar. No vertical scroll, no overlapping UI elements.
  const char *tabs[] = {"WERTE", "KLIMA", "LUEFTER", "PUMPEN"};
  for (uint8_t i = 0; i < 4; ++i) {
    navButtons[i] = makeButton(screen, 1 + i * 80, 204, 78, 34, tabs[i], PANEL,
                               &lv_font_montserrat_12);
    lv_obj_add_event_cb(navButtons[i], onNavigate, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)i);
  }
  showPage(0);
  updateControls();
  lv_obj_invalidate(screen);
}

void updateReadingLabel(lv_obj_t *obj, JsonVariantConst value, const char *pattern) {
  if (value.isNull() || !(value.is<float>() || value.is<double>() ||
      value.is<int>() || value.is<long>())) {
    textIfChanged(obj, "--");
    return;
  }
  float number = value.as<float>();
  if (!isfinite(number)) { textIfChanged(obj, "--"); return; }
  char txt[34];
  snprintf(txt, sizeof(txt), pattern, number);
  textIfChanged(obj, txt);
}

void fetchSummary() {
  if (WiFi.status() != WL_CONNECTED || !SERVER_HOST || !SERVER_HOST[0]) {
    serverReady = false;
    updateControls();
    return;
  }
  HTTPClient http;
  http.setTimeout(1600);
  if (!growHttpBegin(http, SUMMARY_PATH)) {
    serverReady = false;
    setNetworkStatus("API nicht erreichbar");
    updateControls();
    return;
  }
  if (TELEMETRY_API_KEY && TELEMETRY_API_KEY[0])
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  String payload = (code == 200) ? http.getString() : "";
  http.end();
  if (code != 200) {
    char status[48];
    snprintf(status, sizeof(status), "API HTTP %d", code);
    setNetworkStatus(status);
    serverReady = false;
    updateControls();
    return;
  }
  JsonDocument doc;
  const auto error = deserializeJson(doc, payload);
  if (error || !doc["ok"].as<bool>()) {
    serverReady = false;
    setNetworkStatus("API Datenfehler");
    updateControls();
    return;
  }
  serverReady = doc["control"].is<JsonObject>() &&
                doc["control"]["targets"].is<JsonObject>();
  setNetworkStatus(serverReady ? "WLAN + API OK" : "API Steuerdaten fehlen");
  const JsonObjectConst reading = doc["reading"].as<JsonObjectConst>();
  updateReadingLabel(homeValues[0], reading["air_temperature"], "%.1f C");
  updateReadingLabel(homeValues[1], reading["humidity"], "%.0f %%");
  updateReadingLabel(homeValues[2], reading["ph"], "%.2f");
  updateReadingLabel(homeValues[3], reading["ec"], "%.2f mS");

  const char *mode = doc["control"]["control_mode"] | "--";
  char status[104];
  snprintf(status, sizeof(status), "Modus: %.7s", mode);
  textIfChanged(homeMode, status);
  // Die Telemetrie-API speichert zusaetzliche Sensorfelder in reading.extras.
  JsonVariantConst vpd = reading["vpd"];
  if (vpd.isNull()) vpd = reading["extras"]["vpd"];
  if (vpd.is<float>() || vpd.is<double>() || vpd.is<int>()) {
    snprintf(status, sizeof(status), "VPD %.2f kPa", vpd.as<float>());
    textIfChanged(homeVpd, status);
  } else textIfChanged(homeVpd, "VPD: --");
  JsonVariantConst tds = reading["tds_ppm"];
  if (tds.isNull()) tds = reading["tds"];
  if (tds.isNull()) textIfChanged(homeTds, "TDS: --");
  else if (tds.is<float>() || tds.is<double>() || tds.is<int>()) {
    snprintf(status, sizeof(status), "TDS: %.0f ppm", tds.as<float>());
    textIfChanged(homeTds, status);
  } else textIfChanged(homeTds, "TDS: --");

  JsonArrayConst alerts = doc["alerts"].as<JsonArrayConst>();
  if (!alerts.isNull() && alerts.size() > 0) {
    const char *msg = alerts[0]["message"] | "Alarm vorhanden";
    snprintf(status, sizeof(status), "ALARM: %.74s", msg);
    textIfChanged(homeAlert, status);
  } else textIfChanged(homeAlert, "Keine aktiven Meldungen");

  const JsonObjectConst target = doc["control"]["targets"].as<JsonObjectConst>();
  climateStates[0] = target["heater"] | false;
  climateStates[1] = target["circulation_vent"] | false;
  climateStates[2] = target["exhaust_vent"] | false;
  updateControls();
}

void fetchC3() {
  if (WiFi.status() != WL_CONNECTED || !ACTUATOR_HOST || !ACTUATOR_HOST[0] ||
      !TELEMETRY_API_KEY || !TELEMETRY_API_KEY[0]) {
    c3Online = false;
    c3HardwareReady = false;
    updateControls();
    return;
  }
  HTTPClient http;
  http.setTimeout(1600);
  String url = String("http://") + ACTUATOR_HOST + "/api/v1/actuator/state";
  if (!http.begin(url)) {
    c3Online = false;
    c3HardwareReady = false;
    updateControls();
    return;
  }
  http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  String body = (code == 200) ? http.getString() : "";
  http.end();
  JsonDocument doc;
  if (code != 200 || deserializeJson(doc, body) || !doc["ok"].as<bool>()) {
    c3Online = false;
    c3HardwareReady = false;
    updateControls();
    return;
  }
  c3Online = true;
  c3HardwareReady = (doc["hardware_ready"] | false) && doc["fans"].is<JsonObject>();
  lightState = doc["relays"]["light"] | false;
  waterPumpState = doc["relays"]["water_pump"] | false;
  fanStates[0] = doc["fans"]["fan_group_1"] | false;
  fanStates[1] = doc["fans"]["fan_group_2"] | false;
  activeDoser = doc["dosing"]["active_pump"] | 0;
  pumpServiceOff = doc["pump_service_off"] | true;
  pumpServiceOffConfirmed = doc["pump_service_off_confirmed"] | false;
  updateControls();
}

void touchRead(lv_indev_drv_t *, lv_indev_data_t *data) {
  if (!touch.touched()) {
    data->state = LV_INDEV_STATE_RELEASED;
    return;
  }
  TS_Point point = touch.getPoint();
  data->state = LV_INDEV_STATE_PRESSED;
  // Initial values inherited from the previous CYD firmware.
  // Calibrate before enabling physical actuator control through touch.
  data->point.x = map(constrain(point.x, 200, 3700), 200, 3700, 0, SCREEN_W - 1);
  data->point.y = map(constrain(point.y, 240, 3800), 240, 3800, 0, SCREEN_H - 1);
}

void displayFlush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *buffer) {
  const int32_t w = area->x2 - area->x1 + 1;
  const int32_t h = area->y2 - area->y1 + 1;
  if (w > 0 && h > 0) {
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(reinterpret_cast<uint16_t *>(buffer), w * h, true);
    tft.endWrite();
  }
  lv_disp_flush_ready(disp);
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Grow Remote v2.0 start");

  // IMPORTANT: TFT_eSPI uses its own User_Setup.h at LIBRARY compile time.
  // The matching CYD setup is included separately in the delivery ZIP.
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);  // Remove the boot/demo image completely.
  Serial.printf("Display resolution: %d x %d\n", tft.width(), tft.height());

  lv_init();
  lv_disp_draw_buf_init(&drawBuf, drawPixels, nullptr, SCREEN_W * 20);
  static lv_disp_drv_t displayDriver;
  lv_disp_drv_init(&displayDriver);
  displayDriver.hor_res = SCREEN_W;
  displayDriver.ver_res = SCREEN_H;
  displayDriver.flush_cb = displayFlush;
  displayDriver.draw_buf = &drawBuf;
  lv_disp_drv_register(&displayDriver);

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);
  touch.setRotation(1);
  static lv_indev_drv_t inputDriver;
  lv_indev_drv_init(&inputDriver);
  inputDriver.type = LV_INDEV_TYPE_POINTER;
  inputDriver.read_cb = touchRead;
  lv_indev_drv_register(&inputDriver);

  buildUi();
  if (tft.width() != SCREEN_W || tft.height() != SCREEN_H)
    setNetworkStatus("TFT Rotation/Setup pruefen");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println("UI ready; WLAN connecting");
}

void loop() {
  lv_timer_handler();
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (serverReady || c3Online) {
      serverReady = false;
      c3Online = false;
      c3HardwareReady = false;
      updateControls();
    }
    if (strcmp(networkStatus, "WLAN verbindet...") != 0)
      setNetworkStatus("WLAN verbindet...");
    if (now - lastWifiTry >= WIFI_RETRY_MS) {
      lastWifiTry = now;
      WiFi.reconnect();
    }
    delay(5);
    return;
  }
  if (now - lastSummary >= SUMMARY_INTERVAL_MS) {
    lastSummary = now;
    fetchSummary();
  }
  if (now - lastC3 >= C3_INTERVAL_MS) {
    lastC3 = now;
    fetchC3();
  }
  if (stopConfirmAt && now - stopConfirmAt > STOP_CONFIRM_MS) stopConfirmAt = 0;
  delay(5);
}