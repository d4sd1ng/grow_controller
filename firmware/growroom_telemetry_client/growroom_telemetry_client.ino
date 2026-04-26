/**
 * Grow-room ESP32 (fest verbaut + optionales lokales Display):
 * - I2C-Bus-Scan beim Booten erkennt automatisch angeschlossene Sensoren
 * - regelmäßig GET /api/v1/control/state → Relais hardware
 * - POST /api/v1/telemetry → Messwerte + erkannte Sensoren + Relais
 *
 * Libraries: ArduinoJson v6, Wire (I2C).
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>

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

const char *TELEMETRY_PATH = "/api/v1/telemetry";
const char *CONTROL_PATH   = "/api/v1/control/state";

const unsigned long POST_INTERVAL_MS   = 10000;
const unsigned long CONTROL_POLL_MS    = 2000;

unsigned long lastPost        = 0;
unsigned long lastControlPoll = 0;

bool relayHeater      = false;
bool relayPump        = false;
bool relayCirculation = false;
bool relayExhaust     = false;

#define PIN_RELAY_HEATER      4
#define PIN_RELAY_PUMP        5
#define PIN_RELAY_CIRCULATION 16
#define PIN_RELAY_EXHAUST     17

/* ---------- I2C Sensor-Erkennung ---------- */

struct I2CDevice {
  uint8_t address;
  const char *name;
};

// Bekannte I2C-Adressen (häufigste Varianten)
static const I2CDevice KNOWN_I2C_DEVICES[] = {
  {0x3C, "OLED_SSD1306"},
  {0x3D, "OLED_SSD1306_ALT"},
  {0x23, "BH1750_ADDR_LOW"},
  {0x5C, "BH1750_ADDR_HIGH"},
  {0x76, "BME280_0x76"},
  {0x77, "BME280_0x77"},
  {0x44, "SHT30_0x44"},
  {0x45, "SHT30_0x45"},
  {0x48, "ADS1115_0x48"},
  {0x49, "ADS1115_0x49"},
};

static uint8_t detectedI2C[16];      // Adressen der erkannten Geräte
static char    detectedNames[16][32];// Namen der erkannten Geräte
static int     detectedCount = 0;    // Anzahl erkannter Geräte

static uint8_t i2cOledAddr   = 0;    // 0x3C oder 0x3D, 0 = nicht gefunden
static bool    hasBh1750     = false;
static uint8_t bh1750Addr    = 0;

/** Scannt den I2C-Bus und speichert erkannte Geräte. */
void scanI2CDevices() {
  Wire.begin(GPIO_NUM_21, GPIO_NUM_22); // SDA=GPIO21, SCL=GPIO22
  detectedCount = 0;
  i2cOledAddr   = 0;
  hasBh1750     = false;
  bh1750Addr    = 0;

  Serial.println("\n--- I2C-Scan ---");
  for (uint8_t addr = 1; addr < 127 && detectedCount < 16; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      // Gerät antwortet auf dieser Adresse
      detectedI2C[detectedCount] = addr;
      const char *name = "UNKNOWN";
      for (size_t i = 0; i < sizeof(KNOWN_I2C_DEVICES)/sizeof(KNOWN_I2C_DEVICES[0]); ++i) {
        if (KNOWN_I2C_DEVICES[i].address == addr) {
          name = KNOWN_I2C_DEVICES[i].name;
          // Spezifische Geräte merken
          if (addr == 0x3C || addr == 0x3D) i2cOledAddr = addr;
          if (addr == 0x23 || addr == 0x5C) { hasBh1750 = true; bh1750Addr = addr; }
          break;
        }
      }
      strncpy(detectedNames[detectedCount], name, 31);
      detectedNames[detectedCount][31] = '\0';
      Serial.printf("  I2C 0x%02X found: %s\n", addr, name);
      ++detectedCount;
    }
  }
  Serial.printf("--- %d I2C-Gerät(e) erkannt ---\n\n", detectedCount);
}

/* ---------- Sensor-Lesung (echt oder simuliert) ---------- */

// Liest BH1750 Lux-Wert (wenn erkannt)
uint32_t readBH1750() {
  if (!hasBh1750) return 0;
  // BH1750: 0x01 = Power ON, 0x10 = Continuous H-Res Mode
  Wire.beginTransmission(bh1750Addr);
  Wire.write(0x01);  // Power ON
  Wire.endTransmission();
  delay(10);
  Wire.beginTransmission(bh1750Addr);
  Wire.write(0x10);  // Continuous H-Res Mode (1 Lx)
  Wire.endTransmission();
  delay(180);        // Messzeit H-Res
  Wire.requestFrom((int)bh1750Addr, 2);
  if (Wire.available() >= 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return raw / 1.2; // Lux = raw / 1.2
  }
  return 0;
}

// TODO: Ersetze durch echte DHT22-Lesung mit Adafruit/DHT-Bibliothek
float readDHT22Temperature(int sensorNum) {
  // Platzhalter — ersetze durch echte Bibliothek:
  // dht1.readTemperature() / dht2.readTemperature()
  (void)sensorNum;
  return 24.0f + random(-10, 10) / 10.0f;
}

float readDHT22Humidity(int sensorNum) {
  (void)sensorNum;
  return 55.0f + random(-50, 50) / 10.0f;
}

// TODO: Ersetze durch echte EC/pH-Lesung (ADC oder I2C-ADC-Modul)
float readEC() {
  // Platzhalter — ersetze durch analogRead(GPIO34) + Kalibrierung
  return 1.35f + random(-10, 10) / 100.0f;
}

float readPH() {
  // Platzhalter — ersetze durch analogRead(GPIO35) + Kalibrierung
  return 6.2f + random(-20, 20) / 100.0f;
}

void applyRelayHardware() {
  digitalWrite(PIN_RELAY_HEATER,      relayHeater      ? HIGH : LOW);
  digitalWrite(PIN_RELAY_PUMP,        relayPump        ? HIGH : LOW);
  digitalWrite(PIN_RELAY_CIRCULATION, relayCirculation ? HIGH : LOW);
  digitalWrite(PIN_RELAY_EXHAUST,     relayExhaust     ? HIGH : LOW);
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
  int code = http.GET();
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
  if (!doc["ok"].as<bool>()) return;
  JsonObject t = doc["targets"];
  relayHeater      = t["heater"]            | false;
  relayCirculation = t["circulation_vent"]  | false;
  relayExhaust     = t["exhaust_vent"]      | false;
  relayPump        = (t["pump"] | false) || (t["water_pump"] | false);
  applyRelayHardware();
}

void printLocalPanel(float airC, float rh, float ec, float ph, uint32_t lux) {
  Serial.printf("LOCAL | T=%.1fC RH=%.0f%% EC=%.2f pH=%.1f L=%lu | R H=%d P=%d C=%d E=%d | I2C:%d\n",
                airC, rh, ec, ph, lux,
                relayHeater, relayPump, relayCirculation, relayExhaust,
                detectedCount);
}

void sendTelemetry() {
  // Echte (oder simulierte) Sensorwerte lesen
  float airC   = readDHT22Temperature(1);
  float waterC = 22.0f;  // TODO: Wassertemperatur-Sensor
  float rh     = readDHT22Humidity(1);
  float ec     = readEC();
  float ph     = readPH();
  uint32_t lux = readBH1750();

  printLocalPanel(airC, rh, ec, ph, lux);

  StaticJsonDocument<1024> doc;
  doc["air_temperature"]   = airC;
  doc["water_temperature"] = waterC;
  doc["humidity"]          = rh;
  doc["ec"]                = ec;
  doc["ph"]                = ph;
  doc["light_level"]       = lux;
  doc["case_temperature"]  = airC;

  // Relais-Status
  JsonObject relays = doc.createNestedObject("relays");
  relays["heater"]           = relayHeater;
  relays["pump"]             = relayPump;
  relays["circulation_vent"] = relayCirculation;
  relays["exhaust_vent"]     = relayExhaust;

  // Geräte-Info (I2C-Scan-Ergebnisse)
  JsonObject devInfo = doc.createNestedObject("device_info");
  devInfo["device_id"]      = "growroom_esp32_01";
  devInfo["firmware_version"] = "2.1.0";
  devInfo["i2c_device_count"] = detectedCount;
  devInfo["has_oled"]         = (i2cOledAddr != 0);
  devInfo["has_bh1750"]       = hasBh1750;
  devInfo["bh1750_address"]   = bh1750Addr;

  JsonArray i2cArr = devInfo.createNestedArray("i2c_devices");
  for (int i = 0; i < detectedCount; ++i) {
    JsonObject d = i2cArr.createNestedObject();
    d["address"] = detectedI2C[i];
    d["name"]    = detectedNames[i];
  }

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
  delay(500);

  // Relais-Pins als Ausgang
  pinMode(PIN_RELAY_HEATER,      OUTPUT);
  pinMode(PIN_RELAY_PUMP,        OUTPUT);
  pinMode(PIN_RELAY_CIRCULATION, OUTPUT);
  pinMode(PIN_RELAY_EXHAUST,     OUTPUT);
  applyRelayHardware();

  // I2C-Bus scannen — erkennt automatisch angeschlossene Sensoren
  scanI2CDevices();

  // WiFi verbinden
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
