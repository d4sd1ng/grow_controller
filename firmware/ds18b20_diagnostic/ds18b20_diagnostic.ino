/*
 * Separater DS18B20 1-Wire Diagnosesketch
 * ESP32: DATA = GPIO23 (D23), 4.7 kOhm Pull-up DATA -> 3.3 V
 * Nur OneWire-Library, keine DallasTemperature-Library erforderlich.
 * Serial Monitor: 115200 Baud.
 */
#include <Arduino.h>
#include <OneWire.h>

static constexpr uint8_t PIN_ONEWIRE = 23;
OneWire bus(PIN_ONEWIRE);

void printAddress(const uint8_t *addr) {
  for (uint8_t i = 0; i < 8; ++i) {
    if (i) Serial.print(':');
    if (addr[i] < 16) Serial.print('0');
    Serial.print(addr[i], HEX);
  }
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n=== DS18B20 / 1-Wire Diagnose auf GPIO23 ===");
}

void loop() {
  uint8_t addr[8];
  uint8_t count = 0;
  bus.reset_search();
  while (bus.search(addr)) {
    ++count;
    Serial.print("Geraet ");
    Serial.print(count);
    Serial.print(" Adresse: ");
    printAddress(addr);
    Serial.println();

    if (OneWire::crc8(addr, 7) != addr[7]) {
      Serial.println("FEHLER: Adress-CRC ungueltig");
      continue;
    }
    if (addr[0] != 0x28) {
      Serial.println("Hinweis: Geraet ist kein DS18B20 (Familiencode ungleich 0x28)");
      continue;
    }

    if (!bus.reset()) {
      Serial.println("FEHLER: Kein Presence-Puls bei Temperaturstart");
      continue;
    }
    bus.select(addr);
    bus.write(0x44, 0);  // Convert T (extern versorgter Sensor)
    delay(800);         // maximal 750 ms bei 12 Bit

    if (!bus.reset()) {
      Serial.println("FEHLER: Kein Presence-Puls beim Auslesen");
      continue;
    }
    bus.select(addr);
    bus.write(0xBE);  // Scratchpad auslesen
    uint8_t data[9];
    for (uint8_t i = 0; i < 9; ++i) data[i] = bus.read();
    if (OneWire::crc8(data, 8) != data[8]) {
      Serial.println("FEHLER: Scratchpad-CRC ungueltig");
      continue;
    }
    const int16_t raw = static_cast<int16_t>((static_cast<uint16_t>(data[1]) << 8) | data[0]);
    Serial.printf("Temperatur: %.2f C\n", raw / 16.0f);
  }
  Serial.printf("1-Wire Suche abgeschlossen: %u Geraet(e)\n\n", count);
  delay(3000);
}
