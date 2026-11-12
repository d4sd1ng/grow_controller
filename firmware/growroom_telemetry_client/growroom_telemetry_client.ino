/**
 * ESP32-WROOM Sensorcontroller mit passivem OLED.
 * I2C (OLED + ADS1115) GPIO21/22; BH1750 separater Bus GPIO13/14.
 * DHT22 an GPIO25/26/27. DS18B20 GPIO23 bleibt bis zur
 * Bibliotheks-/Kalibrierpruefung deaktiviert.
 * Keine lokalen SSR- oder Dosierpumpen-Ausgaenge.
 * Fuenf SSR-Funktionen werden ueber WLAN an den C3 weitergeleitet.
 * pH/EC sind erst nach Kalibrierung als Messwerte gueltig.
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Preferences.h>
#include <WebServer.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
const char *WIFI_SSID = "";
const char *WIFI_PASS = "";
const char *SERVER_HOST = "192.168.1.70";
const uint16_t SERVER_PORT = 8765;
const char *TELEMETRY_API_KEY = "";
const char *ACTUATOR_HOST = "";
const bool SERVER_USE_HTTPS = false;
const bool SERVER_TLS_INSECURE = false;
#endif

#include "grow_http_client.h"

/* ---------- Konfiguration ---------- */

// --- Sensor-GPIOs (keine Leistungs-GPIOs) ---
#define PIN_DHT_1 25
#define PIN_DHT_2 26
#define PIN_DHT_3 27
#define PIN_BH_SDA 13
#define PIN_BH_SCL 14
// Physische Eingaben und Status-LEDs, Festlegung vom 15.10.
#define PIN_BUTTON_PHASE 19
#define PIN_BUTTON_PUMP 18
#define PIN_LED_GREEN 4
#define PIN_LED_RED 33

// --- Intervale ---
const unsigned long TELEMETRY_INTERVAL_MS = 10000;
const unsigned long CONTROL_POLL_MS = 5000;
const unsigned long OLED_REFRESH_MS = 1000;
const unsigned long SENSOR_READ_MS = 2000;

// --- Sollwerte (Setpoints) ---
#define TARGET_AIR_TEMP 24.0f
#define TARGET_HUMIDITY 60.0f
#define TARGET_EC 1.5f
#define TARGET_PH 6.0f

// pH-/EC-Umrechnung bleibt bis zur Kalibrierung gesperrt.
// Rohspannungen werden nur als Diagnose uebertragen.
#define TEMP_HYST 0.5f
#define HUM_HYST 2.0f

/* ---------- OLED ---------- */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ---------- DHT22 ---------- */
DHT dht1(PIN_DHT_1, DHT22);
DHT dht2(PIN_DHT_2, DHT22);
DHT dht3(PIN_DHT_3, DHT22);
TwoWire lightWire = TwoWire(1);
float zoneTemp[3] = {NAN, NAN, NAN};
float zoneRh[3] = {NAN, NAN, NAN};
float phAdcVolts = NAN;
float ecAdcVolts = NAN;
bool adsAvailable = false;

/* ---------- I2C Sensor-Erkennung ---------- */
struct I2CDevice {
  uint8_t address;
  const char *name;
};
static const I2CDevice KNOWN_I2C_DEVICES[] = {
  { 0x48, "ADS1115" },
  { 0x3C, "OLED_SSD1306" },
  { 0x3D, "OLED_SSD1306_ALT" },
  { 0x23, "BH1750_ADDR_LOW" },
  { 0x5C, "BH1750_ADDR_HIGH" },
  { 0x76, "BME280_0x76" },
  { 0x77, "BME280_0x77" },
  { 0x44, "SHT30_0x44" },
  { 0x45, "SHT30_0x45" },
};
static uint8_t detectedI2C[16];
static char detectedNames[16][32];
static int detectedCount = 0;
static uint8_t i2cOledAddr = 0;
static bool hasBh1750 = false;
static uint8_t bh1750Addr = 0;

/* ---------- State ---------- */
struct SensorData {
  float airC, rh, ec, ph, caseC;
  uint32_t lux;
  bool dhtOk;
} sensors;

bool relayHeater = false;
bool relayPump = false;
bool relayCirculation = false;
bool relayExhaust = false;

char controlMode[8] = "auto";  // "auto" oder "manual"

// Wachstum/Bluete und Woche werden auf dem Sensor-ESP gespeichert.
// Der Pumpentaster ist ein Wartungs-Override, kein Dosierpumpen-Taster.
Preferences preferences;
WebServer sensorApi(80);
bool bloomPhase = false;
uint8_t growthWeek = 1;
bool pumpPaused = false;
unsigned long lastValidServerControl = 0;
const unsigned long MANUAL_CONTROL_STALE_MS = 15000;
const unsigned long BUTTON_DEBOUNCE_MS = 45;
struct DebouncedButton {
  uint8_t pin;
  bool lastRawHigh = true;
  bool stableHigh = true;
  unsigned long lastTransition = 0;
};
DebouncedButton buttonPhase = {PIN_BUTTON_PHASE};
DebouncedButton buttonPump = {PIN_BUTTON_PUMP};

bool sensorApiAuthorized() {
  if (!TELEMETRY_API_KEY || TELEMETRY_API_KEY[0] == 0) {
    sensorApi.send(503, "application/json", "{\"ok\":false,\"error\":\"api_key_unconfigured\"}");
    return false;
  }
  if (!sensorApi.hasHeader("X-Telemetry-Key") ||
      sensorApi.header("X-Telemetry-Key") != String(TELEMETRY_API_KEY)) {
    sensorApi.send(401, "application/json", "{\"ok\":false,\"error\":\"unauthorized\"}");
    return false;
  }
  return true;
}
void updatePhaseLeds() {
  digitalWrite(PIN_LED_GREEN, HIGH);
  digitalWrite(PIN_LED_RED, bloomPhase ? HIGH : LOW);
}
void savePhase() {
  preferences.putBool("bloom", bloomPhase);
  preferences.putUChar("week", growthWeek);
  updatePhaseLeds();
}
void savePumpPause() {
  preferences.putBool("pumpPause", pumpPaused);
  if (pumpPaused) relayPump = false;
}
void sensorApiState() {
  if (!sensorApiAuthorized()) return;
  StaticJsonDocument<256> doc;
  doc["ok"] = true;
  doc["phase"] = bloomPhase ? "bloom" : "grow";
  doc["week"] = growthWeek;
  doc["pump_paused"] = pumpPaused;
  doc["pump_enabled"] = relayPump && !pumpPaused;
  String out;
  serializeJson(doc, out);
  sensorApi.send(200, "application/json", out);
}
void sensorApiSetState() {
  if (!sensorApiAuthorized()) return;
  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, sensorApi.arg("plain")) || !doc.is<JsonObject>()) {
    sensorApi.send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
    return;
  }
  bool nextBloom = bloomPhase;
  uint8_t nextWeek = growthWeek;
  bool nextPause = pumpPaused;
  for (JsonPair item : doc.as<JsonObject>()) {
    const char *key = item.key().c_str();
    JsonVariant value = item.value();
    if (strcmp(key, "phase") == 0 && value.is<const char *>()) {
      String phase = value.as<String>();
      if (phase != "grow" && phase != "bloom") {
        sensorApi.send(422, "application/json", "{\"ok\":false,\"error\":\"invalid_phase\"}");
        return;
      }
      nextBloom = (phase == "bloom");
    } else if (strcmp(key, "week") == 0 && value.is<int>() &&
               value.as<int>() >= 1 && value.as<int>() <= 12) {
      nextWeek = (uint8_t)value.as<int>();
    } else if (strcmp(key, "pump_paused") == 0 && value.is<bool>()) {
      nextPause = value.as<bool>();
    } else {
      sensorApi.send(422, "application/json", "{\"ok\":false,\"error\":\"unknown_or_invalid_field\"}");
      return;
    }
  }
  if (nextBloom != bloomPhase || nextWeek != growthWeek) {
    bloomPhase = nextBloom;
    growthWeek = nextWeek;
    savePhase();
  }
  if (nextPause != pumpPaused) {
    pumpPaused = nextPause;
    savePumpPause();
  }
  sensorApiState();
}
bool fellDebounced(DebouncedButton &button, unsigned long now) {
  bool rawHigh = digitalRead(button.pin) == HIGH;
  if (rawHigh != button.lastRawHigh) {
    button.lastTransition = now;
    button.lastRawHigh = rawHigh;
  }
  if (rawHigh != button.stableHigh &&
      now - button.lastTransition >= BUTTON_DEBOUNCE_MS) {
    button.stableHigh = rawHigh;
    return !rawHigh;
  }
  return false;
}
void pollButtons(unsigned long now) {
  if (fellDebounced(buttonPhase, now)) {
    bloomPhase = !bloomPhase;
    savePhase();
  }
  if (fellDebounced(buttonPump, now)) {
    pumpPaused = !pumpPaused;
    savePumpPause();
  }
}

unsigned long lastTelemetry = 0;
unsigned long lastControl = 0;
unsigned long lastOled = 0;
unsigned long lastSensor = 0;
unsigned long lastActuatorSend = 0;

/* ---------- Prototypen ---------- */
void scanI2CDevices();
void readSensors();
void applyRelays();
void controlLogic();
void drawOled();
bool sendTelemetry();
void fetchControlState();
uint32_t readBH1750();

bool readAdsVolts(uint8_t channel, float &out) {
  if (!adsAvailable || channel > 3) return false;
  // ADS1115: single ended, +/-4.096 V, single shot, 128 SPS.
  uint16_t config = 0x8000 | ((uint16_t)(4 + channel) << 12) |
                    0x0200 | 0x0100 | 0x0080 | 0x0003;
  Wire.beginTransmission(0x48);
  Wire.write(0x01);
  Wire.write((uint8_t)(config >> 8));
  Wire.write((uint8_t)config);
  if (Wire.endTransmission() != 0) return false;
  delay(12);
  Wire.beginTransmission(0x48);
  Wire.write(0x00);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)0x48, (uint8_t)2) != 2) return false;
  int16_t raw = (int16_t)(((uint16_t)Wire.read() << 8) | Wire.read());
  out = (float)raw * (4.096f / 32768.0f);
  return true;
}

bool sendActuatorSnapshot() {
  if (!ACTUATOR_HOST || !ACTUATOR_HOST[0] || WiFi.status() != WL_CONNECTED ||
      !TELEMETRY_API_KEY || !TELEMETRY_API_KEY[0]) return false;
  StaticJsonDocument<192> doc;
  doc["heater"] = relayHeater;
  doc["water_pump"] = relayPump;
  doc["circulation_vent"] = relayCirculation;
  doc["exhaust_vent"] = relayExhaust;
  String payload;
  serializeJson(doc, payload);
  HTTPClient http;
  http.setTimeout(2000);
  String url = String("http://") + ACTUATOR_HOST + "/api/v1/actuator/relays";
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int status = http.POST(payload);
  http.end();
  return status == 200;
}

void scanI2CDevices() {
  Wire.begin(GPIO_NUM_21, GPIO_NUM_22);
  detectedCount = 0;
  i2cOledAddr = 0;
  hasBh1750 = false;
  bh1750Addr = 0;
  adsAvailable = false;
  Serial.println("\n--- I2C-Scan ---");
  for (uint8_t addr = 1; addr < 127 && detectedCount < 16; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      detectedI2C[detectedCount] = addr;
      const char *name = "UNKNOWN";
      for (size_t i = 0; i < sizeof(KNOWN_I2C_DEVICES) / sizeof(KNOWN_I2C_DEVICES[0]); ++i) {
        if (KNOWN_I2C_DEVICES[i].address == addr) {
          name = KNOWN_I2C_DEVICES[i].name;
          if (addr == 0x3C || addr == 0x3D) i2cOledAddr = addr;
          if (addr == 0x48) adsAvailable = true;

          break;
        }
      }
      strncpy(detectedNames[detectedCount], name, 31);
      detectedNames[detectedCount][31] = '\0';
      Serial.printf("  I2C 0x%02X found: %s\n", addr, name);
      ++detectedCount;
    }
  }
  // BH1750 liegt auf einem anderen I2C-Bus.
  lightWire.begin(PIN_BH_SDA, PIN_BH_SCL, 50000);
  for (uint8_t addr : { (uint8_t)0x23, (uint8_t)0x5C }) {
    lightWire.beginTransmission(addr);
    if (lightWire.endTransmission() == 0) {
      hasBh1750 = true;
      bh1750Addr = addr;
      break;
    }
  }
  Serial.printf("--- %d I2C-Hauptbusgeraete erkannt ---\n\n", detectedCount);
}

uint32_t readBH1750() {
  if (!hasBh1750) return 0;
  lightWire.beginTransmission(bh1750Addr);
  lightWire.write(0x01);
  if (lightWire.endTransmission() != 0) return 0;
  delay(10);
  lightWire.beginTransmission(bh1750Addr);
  lightWire.write(0x10);
  if (lightWire.endTransmission() != 0) return 0;
  delay(180);
  if (lightWire.requestFrom(bh1750Addr, (uint8_t)2) != 2) return 0;
  uint16_t raw = ((uint16_t)lightWire.read() << 8) | lightWire.read();
  return (uint32_t)(raw / 1.2f);
}

void readSensors() {
  zoneTemp[0] = dht1.readTemperature();
  zoneRh[0] = dht1.readHumidity();
  zoneTemp[1] = dht2.readTemperature();
  zoneRh[1] = dht2.readHumidity();
  zoneTemp[2] = dht3.readTemperature();
  zoneRh[2] = dht3.readHumidity();
  sensors.dhtOk = !isnan(zoneTemp[0]) && !isnan(zoneRh[0]);
  sensors.airC = sensors.dhtOk ? zoneTemp[0] : NAN;
  sensors.rh = sensors.dhtOk ? zoneRh[0] : NAN;
  phAdcVolts = NAN;
  ecAdcVolts = NAN;
  readAdsVolts(0, phAdcVolts);
  readAdsVolts(1, ecAdcVolts);
  sensors.ph = NAN;  // pH7/pH4-Kalibrierung noch nicht freigegeben
  sensors.ec = NAN;  // temperaturkompensierte TDS-Kalibrierung noch offen
  sensors.lux = readBH1750();
  sensors.caseC = temperatureRead();
}

void applyRelays() {
  // Keine lokalen Ausgaenge am ESP Pro! C3 bekommt nur vier
  // bestaetigte Server-Steuerfelder; "light" bleibt eigener C3-Kanal.
  unsigned long now = millis();
  if (now - lastActuatorSend < 2000) return;
  lastActuatorSend = now;
  sendActuatorSnapshot();
}

void controlLogic() {
  if (pumpPaused) relayPump = false;
  // Sicherheitsabschaltung hat Vorrang auch vor manuellen Heizanfragen.
  if (!sensors.dhtOk) {
    relayHeater = false;
    if (strcmp(controlMode, "auto") == 0) {
      relayCirculation = false;
      relayExhaust = false;
      relayPump = false;
    }
    return;
  }
  if (strcmp(controlMode, "auto") != 0) return;
  if (sensors.airC < TARGET_AIR_TEMP - TEMP_HYST) relayHeater = true;
  else if (sensors.airC > TARGET_AIR_TEMP + TEMP_HYST) relayHeater = false;

  if (sensors.rh > TARGET_HUMIDITY + HUM_HYST) relayCirculation = true;
  else if (sensors.rh < TARGET_HUMIDITY - HUM_HYST) relayCirculation = false;

  if (sensors.airC > TARGET_AIR_TEMP + 2.0f) relayExhaust = true;
  else if (sensors.airC < TARGET_AIR_TEMP + 0.5f) relayExhaust = false;

  // Wasserpumpe ist KEINE Dosierpumpe: Betrieb, ausser Filterwechselpause.
  relayPump = !pumpPaused;
}

void drawOled() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Zeile 1: Temp + RH
  display.setCursor(0, 0);
  if (sensors.dhtOk) {
    display.print("T:");
    display.print(sensors.airC, 1);
    display.print("C H:");
    display.print(sensors.rh, 0);
    display.print("%");
  } else {
    display.print("DHT Fehler");
  }

  // Zeile 2: Case-Temp
  display.setCursor(0, 16);
  display.print("CASE:");
  display.print(sensors.caseC, 1);
  display.print("C");

  // Zeile 3: EC + pH
  display.setCursor(0, 32);
  display.print("EC:-- pH:--");

  // Zeile 4: Relais + Modus
  display.setCursor(0, 48);
  display.print(controlMode);
  display.print(" H");
  display.print(relayHeater);
  display.print(" P");
  display.print(relayPump);
  display.print(" C");
  display.print(relayCirculation);
  display.print(" E");
  display.print(relayExhaust);
  display.setCursor(75, 56);
  display.print(bloomPhase ? "BL" : "WU");
  display.print(growthWeek);
  if (pumpPaused) display.print(" P!");

  display.display();
}

bool sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) return false;

  DynamicJsonDocument doc(3072);
  doc["air_temperature"] = sensors.airC;
  doc["water_temperature"] = nullptr;  // DS18B20 noch nicht verifiziert
  doc["case_temperature"] = sensors.caseC;
  doc["humidity"] = sensors.rh;
  doc["ec"] = sensors.ec;
  doc["ph"] = sensors.ph;
  doc["light_level"] = sensors.lux;

  JsonObject relays = doc.createNestedObject("relays");
  relays["heater"] = relayHeater;
  relays["pump"] = relayPump;
  relays["circulation_vent"] = relayCirculation;
  relays["exhaust_vent"] = relayExhaust;

  JsonObject devInfo = doc.createNestedObject("device_info");
  devInfo["device_id"] = "growroom_esp32_01";
  devInfo["firmware_version"] = "3.1.0-c3";
  devInfo["ads1115_available"] = adsAvailable;
  devInfo["ph_voltage_raw"] = phAdcVolts;
  devInfo["tds_voltage_raw"] = ecAdcVolts;
  devInfo["dht_control_ok"] = sensors.dhtOk;
  devInfo["phase"] = bloomPhase ? "bloom" : "grow";
  devInfo["week"] = growthWeek;
  devInfo["pump_paused"] = pumpPaused;
  devInfo["ph_calibrated"] = false;
  devInfo["tds_calibrated"] = false;
  JsonArray zones = devInfo.createNestedArray("dht_zones");
  for (int i = 0; i < 3; ++i) {
    JsonObject z = zones.createNestedObject();
    z["zone"] = i + 1;
    z["temperature"] = zoneTemp[i];
    z["humidity"] = zoneRh[i];
  }
  devInfo["control_mode"] = controlMode;
  devInfo["i2c_device_count"] = detectedCount;
  devInfo["has_oled"] = (i2cOledAddr != 0);
  devInfo["has_bh1750"] = hasBh1750;
  devInfo["bh1750_address"] = bh1750Addr;

  JsonArray i2cArr = devInfo.createNestedArray("i2c_devices");
  for (int i = 0; i < detectedCount; ++i) {
    JsonObject d = i2cArr.createNestedObject();
    d["address"] = detectedI2C[i];
    d["name"] = detectedNames[i];
  }

  String body;
  serializeJson(doc, body);

  HTTPClient http;
  if (!growHttpBegin(http, "/api/v1/telemetry")) return false;
  http.addHeader("Content-Type", "application/json");
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.POST(body);
  http.end();
  return code == 200;
}

void fetchControlState() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  if (!growHttpBegin(http, "/api/v1/control/state")) return;
  if (strlen(TELEMETRY_API_KEY) > 0)
    http.addHeader("X-Telemetry-Key", TELEMETRY_API_KEY);
  int code = http.GET();
  if (code != 200) {
    http.end();
    return;
  }
  String payload = http.getString();
  http.end();

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload)) return;
  if (!doc["ok"].as<bool>()) return;
  const char *requestedMode = doc["control_mode"] | "";
  if (strcmp(requestedMode, "auto") != 0 && strcmp(requestedMode, "manual") != 0) return;
  if (!doc["targets"].is<JsonObject>()) return;

  const char *m = doc["control_mode"];
  if (m) {
    strncpy(controlMode, m, sizeof(controlMode) - 1);
    controlMode[sizeof(controlMode) - 1] = '\0';
  }

  JsonObject t = doc["targets"];
  if (strcmp(controlMode, "manual") == 0) {
    relayHeater = t["heater"] | relayHeater;
    relayCirculation = t["circulation_vent"] | relayCirculation;
    relayExhaust = t["exhaust_vent"] | relayExhaust;
    if (t.containsKey("pump")) relayPump = t["pump"].as<bool>();
    else if (t.containsKey("water_pump")) relayPump = t["water_pump"].as<bool>();
    if (pumpPaused) relayPump = false;
  }
  lastValidServerControl = millis();
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Taster + Status-LEDs: ausschliesslich Kleinspannungs-Signale.
  pinMode(PIN_BUTTON_PHASE, INPUT_PULLUP);
  pinMode(PIN_BUTTON_PUMP, INPUT_PULLUP);
  pinMode(PIN_LED_GREEN, OUTPUT);
  pinMode(PIN_LED_RED, OUTPUT);
  preferences.begin("grow-phase", false);
  bloomPhase = preferences.getBool("bloom", false);
  growthWeek = preferences.getUChar("week", 1);
  if (growthWeek < 1 || growthWeek > 12) growthWeek = 1;
  pumpPaused = preferences.getBool("pumpPause", false);
  updatePhaseLeds();
  const char *headers[] = {"X-Telemetry-Key"};
  sensorApi.collectHeaders(headers, 1);
  sensorApi.on("/api/v1/sensor/state", HTTP_GET, sensorApiState);
  sensorApi.on("/api/v1/sensor/state", HTTP_POST, sensorApiSetState);
  sensorApi.begin();
  // Der ESP Pro besitzt keine Leistungs-GPIOs.
  // I2C + OLED-Adresse ermitteln
  scanI2CDevices();

  // OLED init (falls gefunden)
  if (i2cOledAddr != 0) {
    if (display.begin(SSD1306_SWITCHCAPVCC, i2cOledAddr)) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 24);
      display.print("WiFi verbinden...");
      display.display();
    }
  }

  // DHT init
  dht1.begin();
  dht2.begin();
  dht3.begin();

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("WiFi");
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 15000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected: " + WiFi.localIP().toString());
}

void loop() {
  unsigned long now = millis();
  pollButtons(now);
  if (WiFi.status() == WL_CONNECTED) sensorApi.handleClient();
  else if (now % 5000 < 10) WiFi.reconnect();

  if (strcmp(controlMode, "manual") == 0 &&
      (lastValidServerControl == 0 ||
       now - lastValidServerControl > MANUAL_CONTROL_STALE_MS)) {
    relayHeater = relayPump = relayCirculation = relayExhaust = false;
  }

  // Sensoren lesen
  if (now - lastSensor >= SENSOR_READ_MS) {
    lastSensor = now;
    readSensors();
  }

  // Autonome Regelung (nur im AUTO-Modus).
  controlLogic();
  if (pumpPaused) relayPump = false;
  applyRelays();

  // OLED aktualisieren
  if (now - lastOled >= OLED_REFRESH_MS) {
    lastOled = now;
    if (i2cOledAddr != 0) drawOled();
  }

  // Control-State vom Server holen (fuer MANUAL-Modus oder Setpoint-Updates)
  if (now - lastControl >= CONTROL_POLL_MS) {
    lastControl = now;
    fetchControlState();
  }

  // Telemetry senden
  if (now - lastTelemetry >= TELEMETRY_INTERVAL_MS) {
    lastTelemetry = now;
    sendTelemetry();
  }
}
