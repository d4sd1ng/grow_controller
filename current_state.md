# Current State — Grow Controller

**Stand:** 2026-10-14  
**Arbeitsbranch:** `feature/ph-calibration-service-esp`  
**Architektur:** **zwei ESP32**: (1) Sensor-ESP32 mit OLED auf **26×30-Lochraster**, (2) **ESP32-C3 SuperMini** als separater Leistungscontroller.

## Sensorplatine 26 × 30

| Sensorpin | Zweck |
|---|---|
| GPIO21/22 | I2C Bus A: OLED (0x3C) + MLX90640-Kamera (0x33) |
| ADS1115 A0 | PH4502C PO; kalibrierte, vorhandene Messkette beibehalten |
| ADS1115 A1 | TDS-/EC-Signaleingang; vorhandene Messkette beibehalten |
| GPIO32 | DS18B20, 1-Wire mit 4,7-kOhm-Pull-up gegen 3V3; Ersatzfühler ausstehend |
| GPIO25/26/27 | 3× DHT22 |
| GPIO13/14 | I2C Bus B: ADS1115 (0x48) + BH1750 (0x23; ADDR an GND) |
| GPIO16 | WUCHS-Taster gegen GND, interner Pull-up |
| GPIO19 | BLÜTE-Taster gegen GND, interner Pull-up |
| GPIO23 | WUCHS-LED über eigenen 470-Ohm-Vorwiderstand |
| GPIO33 | BLÜTE-LED über eigenen 470-Ohm-Vorwiderstand |
| GPIO18 | pH-Buzzer-Steuersignal reserviert; **deaktiviert bis Modulprüfung** |

**Nicht verwenden:** PH4502C TO und DO. Gemessene TO-Ausgangsspannung 3,3–3,4 V bei ADS-VDD 3,3 V. Wassertemperatur zukünftig über DS18B20.

**Bestückung:** ESP32 über passende, vorab mechanisch geprüfte 2×15-polige Buchsenleisten; ADS1115 steckbar; DS18B20 (3-polig), 3× DHT22 (je 3-polig), BH1750 (**5-polig inklusive ADDR**), MLX90640-Kamera (**4-poliger Stecker: SDA D21, SCL D22, GND, Versorgung vorerst offen**); separate 3V3-/GND-Verteilung; außerdem 2 Taster, 2 Status-LEDs und Buzzer-Reserve. Nur Sensor-/Kleinspannungsschaltungen auf dieser Lochrasterplatine. Vollständiger Plan: [wiring_plan_v2.md](wiring_plan_v2.md).

**Phasenauswahl:** In beiden Sensor-Sketches sind zwei entprellte physische Taster mit LED-Rückmeldung und Speicherung in NVS `growphase_v1` umgesetzt. WUCHS-Profil nutzt vorläufig die bisherigen Klimawerte 24 °C und 60 % relative Feuchte. BLÜTE-Sollwerte sind noch offen; bis zur Konfiguration ist die Phasenautomatik gesperrt. Der pH-Buzzer ist nur vorbereitet (`GROW_ENABLE_PH_BUZZER=0`), GPIO18 wird noch nicht angesteuert, solange Buzzer-Bauart und Pegel unklar sind.

## Zweites Gerät — ESP32-C3 SuperMini

Separate Installation im Leistungsgehäuse für die späteren vier Funktionskanäle Licht, Umluft, Abluft und Heizstrahler. **Die C3-Pinbelegung und Steuerungsfirmware sind noch nicht freigegeben, keine Relaisansteuerung funktionsfähig.** Kommunikationsausfall und Rückmeldungen müssen sicher behandelt werden. Keine 230-V-Anschlussanleitung in diesem Repository.

## Firmware

- `firmware/ph_calibration_tool`: Kalibrierung/Diagnose/pH-Alarmtests, **keine Relais-GPIO-Ausgänge**.
- `firmware/growroom_telemetry_client`: Messwerte, pH-Alarm, OLED, HTTP-API und Sollwertverwaltung; **alte direkte Relais-GPIO-Schaltung entfernt**. Aktorstatus ist `esp32_c3_not_connected`. WLAN-Verbindung zum C3 und tatsächliche Umsetzung von Steuerbefehlen stehen aus.
- `firmware/ds18b20_diagnostic`: 1-Wire-Test GPIO32, sobald Ersatzfühler da.
- `firmware/powerbox_actuator`: alte Firmware, **nicht** als fertige C3-Version verwenden.
- CYD und Raspberry Pi kommunizieren bislang weiterhin mit dem Sensor-ESP32.

## Verifizierter Stand

- PH4502C PO pH-4-/pH-7-Kalibrierung laut Nutzer erfolgt.
- pH-Alarmtests LOW / HIGH / NORMAL laut Nutzer funktionsfähig.
- PH4502C TO nicht verwendet. DS18B20 Ersatzsensoren noch nicht geliefert.
- Sonstige Sensor-Stecker, Steckleisten, Taster, LEDs und Lochrasteraufbau noch zu montieren. Buzzer-Typ und Versorgung noch nicht ermittelt.
- Die aktuell geänderten Sketche wurden hier **nicht** mit einer realen Arduino-ESP32-Toolchain kompiliert und nicht erneut auf Hardware getestet.

## Nächste Aufgaben

1. Lochrasterplatine 26×30 anhand echter Modulmaße trocken bestücken, USB/OLED und Sensoranschlüsse ausrichten.
2. Buchsenleisten, 3V3/GND-Verteilung und Sensorsteckverbinder auf der **Sensorplatine** löten; Durchgang und Versorgung prüfen.
3. DS18B20 nach Lieferung an GPIO32 prüfen und Wasserfühler per ROM zuordnen.
4. DHT22/BH1750 prüfen und die MLX90640 über den auf D21/D22 reservierten Anschluss mit der tatsächlichen Breakout-Versorgung verbinden, sobald ihr Datenblatt vorliegt.
5. Getrennten C3-Controller softwareseitig entwickeln, kommunikative und elektrische Sicherheitsprüfung vor jeder Leistungsfreigabe.
6. PostgreSQL-Migration von bisherigen Serverdaten separat durchführen.
