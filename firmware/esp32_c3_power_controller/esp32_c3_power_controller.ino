/**
 * Grow Controller - ESP32-C3 SuperMini Leistungscontroller.
 * 5 SSR-Steuersignale + exakt 6 unidirektionale 12-V-Dosierpumpen (MOSFET).
 * Keine Anzeige, kein OLED, keine Umpolung.
 *
 * Sicherheitszustand: Alle Ausgaenge AUS beim Start, WLAN-Ausfall,
 * Kommandotimeout und fehlender Hardwarefreigabe.
 *
 * WICHTIG: Die 11 physischen Ausgangszuordnungen sind noch NICHT freigegeben.
 * Ohne Board-/Treiberabgleich bleiben Hardwareausgaenge gesperrt.
 * Es wird keine Netzspannungsverdrahtung in dieser Firmware definiert.
 *
 * Arduino Libraries: ESP32 core (WiFi, WebServer), ArduinoJson v6.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
const char *WIFI_SSID = "";
const char *WIFI_PASS = "";
const char *SERVER_HOST = "";
const uint16_t SERVER_PORT = 8765;
const char *TELEMETRY_API_KEY = "";
const bool SERVER_USE_HTTPS = false;
const bool SERVER_TLS_INSECURE = false;
#endif

// Erst nach Verifikation des konkreten Boards, GPIO-Layouts, der MOSFET-Treiber
// und der SSR-Eingangslogik auf true setzen.
static constexpr bool OUTPUTS_APPROVED = false;
static constexpr uint8_t SSR_COUNT = 5;
static constexpr uint8_t DOSER_COUNT = 6;

// GPIO-Mapping absichtlich UNBELEGT: -1 ist kein physischer GPIO.
// Reihenfolge SSR: Licht, Abluft, Umluft, Heizung, Wasserpumpe.
static constexpr int8_t SSR_GPIO[SSR_COUNT] = {-1, -1, -1, -1, -1};
static constexpr int8_t DOSER_GPIO[DOSER_COUNT] = {-1, -1, -1, -1, -1, -1};

// Logikpegel nur nach Ueberpruefung der Treiber festlegen.
static constexpr uint8_t OUTPUT_ACTIVE_LEVEL = HIGH;
static constexpr uint8_t OUTPUT_INACTIVE_LEVEL = LOW;

// Vor Kalibrierung der Pumpen kein Dosierimpuls freigegeben.
// Beim Freigeben ist ein technisch begruendetes Limit festzulegen.
static constexpr uint32_t MAX_DOSING_MS = 0;
static constexpr uint32_t CONTROL_TIMEOUT_MS = 15000;
static constexpr uint32_t WIFI_RECONNECT_MS = 5000;

static_assert(SSR_COUNT == 5, "Genau fuenf SSR-Funktionen erforderlich");
static_assert(DOSER_COUNT == 6, "Genau sechs Dosierpumpen erforderlich");

const char *SSR_NAMES[SSR_COUNT] = {
  "light", "exhaust_vent", "circulation_vent", "heater", "water_pump"
};

WebServer server(80);
bool ssrState[SSR_COUNT] = {false, false, false, false, false};
int8_t activeDoser = -1;
uint32_t doseStartedAt = 0;
uint32_t doseDurationMs = 0;
uint32_t lastControlAt = 0;
uint32_t lastReconnectAt = 0;
bool layoutValid = false;

bool pinAllowed(int8_t pin) {
  // GPIO18/19 haeufig USB, GPIO8/9 Boot-Funktionen des C3.
  // Keine dieser Leitungen ohne gesonderte Hardwarepruefung verwenden.
  // Nur auf den ueblichen C3-Pad-Gruppen, nicht im Flash-/USB-Bereich.
  // GPIO2 ist ein Strapping-Pin und bleibt OHNE Hardwarefreigabe gesperrt.
  return ((pin >= 0 && pin <= 10 && pin != 2 && pin != 8 && pin != 9) ||
          pin == 20 || pin == 21);
}

bool verifyLayout() {
  // Doppelte / ungueltige GPIOs sind auch bei Softwarefreigabe verboten.
  for (uint8_t i = 0; i < SSR_COUNT + DOSER_COUNT; ++i) {
    int8_t p = i < SSR_COUNT ? SSR_GPIO[i] : DOSER_GPIO[i - SSR_COUNT];
    if (!pinAllowed(p)) return false;
    for (uint8_t j = 0; j < i; ++j) {
      int8_t q = j < SSR_COUNT ? SSR_GPIO[j] : DOSER_GPIO[j - SSR_COUNT];
      if (p == q) return false;
    }
  }
  return true;
}

void setPhysical(int8_t pin, bool on) {
  if (!layoutValid || !OUTPUTS_APPROVED) return;
  digitalWrite(pin, on ? OUTPUT_ACTIVE_LEVEL : OUTPUT_INACTIVE_LEVEL);
}

void stopDosers() {
  if (activeDoser >= 0 && activeDoser < DOSER_COUNT)
    setPhysical(DOSER_GPIO[activeDoser], false);
  activeDoser = -1;
  doseDurationMs = 0;
}

void stopAll() {
  stopDosers();
  for (uint8_t i = 0; i < SSR_COUNT; ++i) {
    ssrState[i] = false;
    setPhysical(SSR_GPIO[i], false);
  }
}

bool authorized() {
  if (!TELEMETRY_API_KEY || !TELEMETRY_API_KEY[0]) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"api_key_unconfigured\"}");
    return false;
  }
  if (!server.hasHeader("X-Telemetry-Key") ||
      server.header("X-Telemetry-Key") != String(TELEMETRY_API_KEY)) {
    server.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
    return false;
  }
  return true;
}

bool outputReady() {
  if (!OUTPUTS_APPROVED || !layoutValid) {
    server.send(503, "application/json", "{\"ok\":false,\"error\":\"hardware_unconfigured\"}");
    return false;
  }
  return true;
}

void sendState() {
  if (!authorized()) return;
  StaticJsonDocument<768> doc;
  doc["ok"] = true;
  doc["firmware"] = "c3-power-0.1.0";
  doc["hardware_ready"] = OUTPUTS_APPROVED && layoutValid;
  doc["wifi_connected"] = WiFi.status() == WL_CONNECTED;
  JsonObject relays = doc.createNestedObject("relays");
  for (uint8_t i = 0; i < SSR_COUNT; ++i) relays[SSR_NAMES[i]] = ssrState[i];
  JsonObject dosing = doc.createNestedObject("dosing");
  dosing["channel_count"] = DOSER_COUNT;
  dosing["active_channel"] = activeDoser >= 0 ? activeDoser + 1 : 0;
  dosing["max_duration_ms"] = MAX_DOSING_MS;
  String payload;
  serializeJson(doc, payload);
  server.send(200, "application/json", payload);
}

void setRelays() {
  if (!authorized() || !outputReady()) return;
  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
    return;
  }
  // Nur explizite boolesche Schaltbefehle. Keine stillen Konvertierungen.
  bool next[SSR_COUNT];
  for (uint8_t i = 0; i < SSR_COUNT; ++i) next[i] = ssrState[i];
  for (JsonPair item : doc.as<JsonObject>()) {
    int8_t channel = -1;
    for (uint8_t i = 0; i < SSR_COUNT; ++i)
      if (strcmp(item.key().c_str(), SSR_NAMES[i]) == 0) channel = i;
    if (channel < 0 || !item.value().is<bool>()) {
      server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_relay_field\"}");
      return;
    }
    next[channel] = item.value().as<bool>();
  }
  for (uint8_t i = 0; i < SSR_COUNT; ++i) {
    ssrState[i] = next[i];
    setPhysical(SSR_GPIO[i], next[i]);
  }
  lastControlAt = millis();
  server.send(200, "application/json", "{\"ok\":true}");
}

void startDose() {
  if (!authorized() || !outputReady()) return;
  StaticJsonDocument<192> doc;
  if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>() ||
      !doc["pump"].is<int>() || !doc["duration_ms"].is<uint32_t>()) {
    server.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_dose\"}");
    return;
  }
  int channel = doc["pump"].as<int>();
  uint32_t duration = doc["duration_ms"].as<uint32_t>();
  if (channel < 1 || channel > DOSER_COUNT || MAX_DOSING_MS == 0 ||
      duration == 0 || duration > MAX_DOSING_MS) {
    server.send(422, "application/json", "{\"ok\":false,\"error\":\"dose_limit_or_channel\"}");
    return;
  }
  if (activeDoser >= 0) {
    server.send(409, "application/json", "{\"ok\":false,\"error\":\"doser_busy\"}");
    return;
  }
  activeDoser = static_cast<int8_t>(channel - 1);
  doseStartedAt = millis();
  doseDurationMs = duration;
  setPhysical(DOSER_GPIO[activeDoser], true);
  server.send(200, "application/json", "{\"ok\":true}");
}

void emergencyStop() {
  if (!authorized()) return;
  stopAll();
  server.send(200, "application/json", "{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  layoutValid = verifyLayout();
  if (OUTPUTS_APPROVED && layoutValid) {
    for (uint8_t i = 0; i < SSR_COUNT; ++i) {
      digitalWrite(SSR_GPIO[i], OUTPUT_INACTIVE_LEVEL);
      pinMode(SSR_GPIO[i], OUTPUT);
    }
    for (uint8_t i = 0; i < DOSER_COUNT; ++i) {
      digitalWrite(DOSER_GPIO[i], OUTPUT_INACTIVE_LEVEL);
      pinMode(DOSER_GPIO[i], OUTPUT);
    }
  }
  stopAll();

  const char *headers[] = {"X-Telemetry-Key"};
  server.collectHeaders(headers, 1);
  server.on("/api/v1/actuator/state", HTTP_GET, sendState);
  server.on("/api/v1/actuator/relays", HTTP_POST, setRelays);
  server.on("/api/v1/actuator/dose", HTTP_POST, startDose);
  server.on("/api/v1/actuator/stop", HTTP_POST, emergencyStop);
  server.onNotFound([]() {
    server.send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
  });
  WiFi.mode(WIFI_STA);
  if (WIFI_SSID && WIFI_SSID[0]) WiFi.begin(WIFI_SSID, WIFI_PASS);
  server.begin();
}

void loop() {
  uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    stopAll();
    if (now - lastReconnectAt >= WIFI_RECONNECT_MS) {
      lastReconnectAt = now;
      WiFi.reconnect();
    }
  } else {
    server.handleClient();
    if (lastControlAt && now - lastControlAt > CONTROL_TIMEOUT_MS) {
      stopAll();
      lastControlAt = 0;
    }
  }
  if (activeDoser >= 0 && now - doseStartedAt >= doseDurationMs) stopDosers();
  delay(2);
}
