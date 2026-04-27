/**
 * Growroom-ESP32 Steuerzentrale (ESP32-WROOM-32D + SSD1306 OLED)
 *
 * Autonome Regelung mit Hysterese. Server dient nur Logging + Dashboard.
 * OLED zeigt passive Anzeige (keine Touch-Steuerung).
 *
 * Hardware (festgelegt):
 *   GPIO36 -> DHT22 (Luft-Temperatur + Luftfeuchte)
 *   GPIO34 -> EC-Sonde  (ADC, Verstaerker-Modul)
 *   GPIO35 -> pH-Sonde   (ADC, Verstaerker-Modul)
 *   GPIO21 -> I2C SDA    (SSD1306 OLED + BH1750)
 *   GPIO22 -> I2C SCL
 *   GPIO4  -> Relais Heizung
 *   GPIO5  -> Relais Wasserpumpe
 *   GPIO16 -> Relais Zirkulationsluefter
 *   GPIO17 -> Relais Abluftluefter
 */
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#if __has_include("device_secrets.h")
#include "device_secrets.h"
#else
const char *WIFI_SSID = "M3thlab";
const char *WIFI_PASS = "!Butch*20052023";
const char *SERVER_HOST = "192.168.1.70";
const uint16_t SERVER_PORT = 8765;
const char *TELEMETRY_API_KEY = "";
const bool SERVER_USE_HTTPS = false;
const bool SERVER_TLS_INSECURE = false;
#endif

#include "grow_http_client.h"

/* ---------- Konfiguration ---------- */

// --- Pins ---
#define PIN_DHT 36
#define PIN_ADC_EC 34
#define PIN_ADC_PH 35
#define PIN_RELAY_HEATER 4
#define PIN_RELAY_PUMP 5
#define PIN_RELAY_CIRC 16
#define PIN_RELAY_EXHAUST 17

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

// --- Kalibrierung EC / pH (ADC 12bit: 0-4095 @ 3.3V) ---
// Passe diese Werte an dein Verstaerker-Modul an!
#define EC_ADC_MIN 0
#define EC_ADC_MAX 4095
#define EC_VALUE_MIN 0.0f
#define EC_VALUE_MAX 5.0f
#define PH_ADC_MIN 0
#define PH_ADC_MAX 4095
#define PH_VALUE_MIN 0.0f
#define PH_VALUE_MAX 14.0f

// --- Hysterese ---
#define TEMP_HYST 0.5f
#define HUM_HYST 2.0f
#define EC_HYST 0.1f

/* ---------- OLED ---------- */
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

/* ---------- DHT22 ---------- */
DHT dht(PIN_DHT, DHT22);

/* ---------- I2C Sensor-Erkennung ---------- */
struct I2CDevice {
  uint8_t address;
  const char *name;
};
static const I2CDevice KNOWN_I2C_DEVICES[] = {
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

unsigned long lastTelemetry = 0;
unsigned long lastControl = 0;
unsigned long lastOled = 0;
unsigned long lastSensor = 0;

/* ---------- Prototypen ---------- */
void scanI2CDevices();
void readSensors();
void applyRelays();
void controlLogic();
void drawOled();
bool sendTelemetry();
void fetchControlState();
float mapADC(int raw, int adcMin, int adcMax, float valMin, float valMax);
uint32_t readBH1750();

/* ---------- Hilfsfunktionen ---------- */

float mapADC(int raw, int adcMin, int adcMax, float valMin, float valMax) {
  if (raw < adcMin) raw = adcMin;
  if (raw > adcMax) raw = adcMax;
  return valMin + (valMax - valMin) * (float)(raw - adcMin) / (float)(adcMax - adcMin);
}

void scanI2CDevices() {
  Wire.begin(GPIO_NUM_21, GPIO_NUM_22);
  detectedCount = 0;
  i2cOledAddr = 0;
  hasBh1750 = false;
  bh1750Addr = 0;
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
          if (addr == 0x23 || addr == 0x5C) {
            hasBh1750 = true;
            bh1750Addr = addr;
          }
          break;
        }
      }
      strncpy(detectedNames[detectedCount], name, 31);
      detectedNames[detectedCount][31] = '\0';
      Serial.printf("  I2C 0x%02X found: %s\n", addr, name);
      ++detectedCount;
    }
  }
  Serial.printf("--- %d I2C-Geraete erkannt ---\n\n", detectedCount);
}

uint32_t readBH1750() {
  if (!hasBh1750) return 0;
  Wire.beginTransmission(bh1750Addr);
  Wire.write(0x01);
  Wire.endTransmission();
  delay(10);
  Wire.beginTransmission(bh1750Addr);
  Wire.write(0x10);
  Wire.endTransmission();
  delay(180);
  Wire.requestFrom((int)bh1750Addr, 2);
  if (Wire.available() >= 2) {
    uint16_t raw = (Wire.read() << 8) | Wire.read();
    return raw / 1.2;
  }
  return 0;
}

void readSensors() {
  sensors.dhtOk = false;
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (!isnan(t) && !isnan(h)) {
    sensors.airC = t;
    sensors.rh = h;
    sensors.dhtOk = true;
  }
  sensors.ec = mapADC(analogRead(PIN_ADC_EC), EC_ADC_MIN, EC_ADC_MAX, EC_VALUE_MIN, EC_VALUE_MAX);
  sensors.ph = mapADC(analogRead(PIN_ADC_PH), PH_ADC_MIN, PH_ADC_MAX, PH_VALUE_MIN, PH_VALUE_MAX);
  sensors.lux = readBH1750();
  sensors.caseC = temperatureRead();  // ESP32 interner Temperatursensor
}

void applyRelays() {
  digitalWrite(PIN_RELAY_HEATER, relayHeater ? HIGH : LOW);
  digitalWrite(PIN_RELAY_PUMP, relayPump ? HIGH : LOW);
  digitalWrite(PIN_RELAY_CIRC, relayCirculation ? HIGH : LOW);
  digitalWrite(PIN_RELAY_EXHAUST, relayExhaust ? HIGH : LOW);
}

void controlLogic() {
  if (strcmp(controlMode, "auto") != 0) return;

  // Heizung
  if (sensors.airC < TARGET_AIR_TEMP - TEMP_HYST) relayHeater = true;
  else if (sensors.airC > TARGET_AIR_TEMP + TEMP_HYST) relayHeater = false;

  // Zirkulationsluefter (Luftfeuchte)
  if (sensors.rh > TARGET_HUMIDITY + HUM_HYST) relayCirculation = true;
  else if (sensors.rh < TARGET_HUMIDITY - HUM_HYST) relayCirculation = false;

  // Abluftluefter (Notfall-Kuehlung)
  if (sensors.airC > TARGET_AIR_TEMP + 2.0f) relayExhaust = true;
  // Abluft bleibt an bis Temp wieder im Zielbereich
  else if (sensors.airC < TARGET_AIR_TEMP + 0.5f) relayExhaust = false;

  // Wasserpumpe (EC)
  if (sensors.ec < TARGET_EC - EC_HYST) relayPump = true;
  else if (sensors.ec > TARGET_EC + EC_HYST) relayPump = false;
}

void drawOled() {
  if (!display.begin(SSD1306_SWITCHCAPVCC, i2cOledAddr ? i2cOledAddr : OLED_ADDR)) return;
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
  display.print("EC:");
  display.print(sensors.ec, 2);
  display.print(" pH:");
  display.print(sensors.ph, 1);

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

  display.display();
}

bool sendTelemetry() {
  if (WiFi.status() != WL_CONNECTED) return false;

  StaticJsonDocument<1024> doc;
  doc["air_temperature"] = sensors.airC;
  doc["water_temperature"] = 0.0f;  // optional
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
  devInfo["firmware_version"] = "3.0.0";
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
    relayPump = (t["pump"] | false) || (t["water_pump"] | false) || relayPump;
    applyRelays();
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  // Relais-Pins
  pinMode(PIN_RELAY_HEATER, OUTPUT);
  pinMode(PIN_RELAY_PUMP, OUTPUT);
  pinMode(PIN_RELAY_CIRC, OUTPUT);
  pinMode(PIN_RELAY_EXHAUST, OUTPUT);
  applyRelays();

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
  dht.begin();

  // WiFi
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

  // Sensoren lesen
  if (now - lastSensor >= SENSOR_READ_MS) {
    lastSensor = now;
    readSensors();
  }

  // Autonome Regelung (nur im AUTO-Modus)
  controlLogic();
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
