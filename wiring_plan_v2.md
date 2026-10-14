# Grow Controller — Verdrahtung V4: zwei ESP32

**Stand:** 2026-10-14  
**Hardware:** (1) Sensor-ESP32 mit integriertem 0,96"-OLED auf **26 × 30 Lochraster**, (2) **ESP32-C3 SuperMini** als separater Leistungscontroller.  
**Status:** Sensor/pH-Kalibrierung funktioniert; die neue Zwei-ESP-Kommunikation und der C3-Leistungsteil sind noch NICHT implementiert bzw. getestet.

> Dieser Plan behandelt ausschliesslich die Kleinspannungs-Sensorplatine und die logische Rollenverteilung. Die 230-V-Seite ist nicht Gegenstand des Plans; Montage/Pruefung gehoeren in fachkundige Haende.

## Architektur

```text
                       WLAN
        +--------------------------------+
        |                                |
+-------v----------------+    +----------v--------------+
| Sensor-ESP32 + OLED    |    | ESP32-C3 SuperMini       |
| 26 x 30 Lochraster     |    | separates Leistungsgeh.  |
| - ADS1115 pH/EC        |    | - Aktorcontroller        |
| - 3 x DHT22            |    | - Relaisansteuerung      |
| - DS18B20 (nach Ersatz)|    |   noch NICHT integriert  |
| - BH1750 (geplant)     |    | - Fail-safe: zu planen   |
+----------+-------------+    +-----------+-------------+
           | WLAN                         | WLAN
           +----------> Raspberry Pi <----+
                            |
                     CYD / Dashboard
```

**Keine SSR-/Relaisleitungen auf der 26×30-Sensorplatine.** Der ESP32-C3 ist ein zweites Gerät, kein Aufsteckmodul dieser Platine. Die alte Einzel-ESP32-Zeichnung und die Zuordnung GPIO17/18/19/33 zu den Relais sind überholt. Auf dem tatsächlich abgelesenen Sensor-Board sind GPIO17/18 ohnehin nicht als Stiftkontakte aufgeführt.

## Sensor-ESP32: bestätigte Stiftkontakte und Signalzuordnung

Die Beschriftungen richten sich nach den **vom Nutzer direkt abgelesenen Board-Pins**, nicht nach einem generischen ESP32-DevKit-Pinout. Ein endgültiger Loch-für-Loch-Aufbau setzt den realen Lochabstand und die Modulmaße voraus.

| Sensor/Modul | Anschluss am Sensor-ESP32 | Zweck | Bauzustand |
|---|---|---|---|
| Internes OLED | GPIO21 SDA, GPIO22 SCL | vorhandenes Display (I2C) | vorhanden |
| ADS1115 | GPIO21 SDA, GPIO22 SCL, 3V3, GND | ADC, Adresse 0x48 (ADDR=GND) | bereits in Verwendung |
| PH4502C PO | ADS1115 **A0** | pH-Messung, vorhandene kalibrierte Signalkette **nicht umbauen** | kalibriert |
| CQRobot TDS/EC analog | ADS1115 **A1** | EC-Spannung, vorhandene Signalkette erhalten | bereits angeschlossen |
| DS18B20 | **GPIO23 (D23)** | Wasser-Temperatur, 1-Wire | Ersatzsensoren noch nicht geliefert |
| DHT22 #1 | **GPIO25 (D25)** | Pflanzenzone | vorgesehen |
| DHT22 #2 | **GPIO26 (D26)** | Zuluft-/Unterzone | vorgesehen |
| DHT22 #3 | **GPIO27 (D27)** | obere Zone | vorgesehen |
| BH1750 | **GPIO13 (D13) SDA**, **GPIO14 (D14) SCL** | separater I2C-Bus | vorgesehen |
| MLX90640 optional | GPIO21 SDA, GPIO22 SCL | mit OLED/ADS geteilter I2C-Bus; nur nach Modul-/Pegelpruefung | Reserve |

**Ausdrücklich nicht verdrahten:** PH4502C **TO -> ADS A2** (TO wurde mit 3,3–3,4 V gemessen, bei ADS-VDD 3,3 V nicht zulässig); PH4502C **DO -> GPIO32** (für die kalibrierten pH-Grenzalarme nicht erforderlich). GPIO17/18/19/33 als Relaisleitungen am Sensor-ESP entfallen. GPIO32/GPIO33 bleiben Reserve.

## 26 × 30 Lochraster — konkreter Bestückungsplan (Kleinspannung)

**Achtung:** 26×30 bezeichnet 26 × 30 **Bohrungen**, nicht Millimeter. Auf normalem 2,54-mm-Lochraster entspricht das ungefähr 66 × 76 mm im Raster; die Leiterplatte selbst kann etwas größer sein. Bestueckung erst trocken auflegen, da genaue OLED-ESP32-/ADS1115-Modulabmessungen und Reihenabstände nicht zuverlässig vorliegen.

| Bauteil / Anschluss | Auf der Sensorplatine vorbereiten | Bemerkung |
|---|---|---|
| Sensor-ESP32 mit OLED | **2 × 15-polige Buchsenleiste**, nur wenn der reale ESP32 die bestätigten 15 Kontakte je Seite und passendes Raster besitzt | USB und OLED müssen erreichbar bleiben |
| ADS1115-Modul | passende **Buchsenleisten / Stiftadapter** für die tatsächlich vorhandenen Modul-Pins | pH A0/EC A1; A2/A3 frei |
| DS18B20-Bus | **1 × 3-poliger Anschluss: 3V3, GND, DATA D23** | beide DS18B20 können denselben Bus nutzen |
| DS-Pull-up | **1 × 4,7 kOhm** zwischen DATA D23 und 3V3 | genau EIN gemeinsamer Pull-up am Bus |
| DHT22 #1–#3 | **3 × 3-poliger Anschluss: 3V3, GND, DATA** | DATA D25/D26/D27 |
| DHT-Pull-ups | **bis zu 3 × 4,7 kOhm**, jeweils DATA gegen 3V3 | nur falls das konkrete DHT22-Modul keinen eigenen Pull-up hat |
| BH1750 | **1 × 4-poliger Anschluss: 3V3, GND, SDA D13, SCL D14** | ggf. ADDR-Pin am Modul beachten, keine doppelten Pull-ups ohne Prüfung |
| pH-/EC-Signale | vorhandene kalibrierte/verifizierte Verbindungen A0/A1 sauber übernehmen; geeignete Steckklemmen | keine neue, unbelegte 10k/20k-Teilerannahme |
| Spannungs-/Masseverteilung | getrennte **3V3- und GND-Verbindungen** auf Lochraster mit isoliertem Draht | Lochraster hat keine automatischen Stromschienen |
| Erweiterung, optional | 4-polige I2C-Reserve für MLX90640, sofern Platz/Modul passend | kein Pflichtteil für die jetzige Platine |

**Mechanische Reihenfolge:** zuerst ESP32 und ADS1115 probeweise positionieren; USB-/OLED-Freiraum und Sensor-Steckverbinder prüfen; dann Buchsenleisten und Pull-up-Widerstände verlöten; anschließend Kleinspannungs-Signale und Masse einzeln durchmessen. Steckverbinder nach Signalname und Spannung beschriften. Die DHT22-/BH1750-/DS-Sensoren messen außerhalb des warmen Gehäuses; daher **nur ihre Anschlüsse**, nicht die Messköpfe selbst, auf der Lochrasterplatine anordnen.

### Versorgung

ADS1115, DS18B20, DHT22 und BH1750: geplante **3,3-V-Logik**. Versorgung von konkretem Breakout abhängig; prüfen, ob der 3,3-V-Regler des ESP32 den gesamten Strom liefern kann. PH4502C VCC wurde mit **4,7 V** gemessen; ihre bisher funktionierende Versorgung und A0-Messkette nicht ohne Grund verändern. Alle beteiligten Kleinspannungs-Sensoren benötigen eine passende gemeinsame Signalmasse.

## ESP32-C3 SuperMini — eigener Aufbau

Der C3 liegt **im separaten Leistungsgehäuse**. Die Vier-Kanal-Zuordnung bleibt funktional **Licht / Umluft / Abluft / Heizstrahler**, aber sie ist **nicht** auf dem Sensor-ESP32 zu verlöten. Konkrete C3-GPIOs, Eingangsstrom-/Pegel-Kompatibilität der eingesetzten Relais und die vollständige Fail-safe-Strategie sind noch zu verifizieren; darum enthält dieser Plan absichtlich **keine erfundene C3-Pinbelegung oder Lastverdrahtung**.

Geplante Kommunikation: über WLAN, mit getrennten Zuständen **angefordert**, **vom C3 bestätigt**, **Kommunikation verloren**. Bei Kommunikationsfehlern muss das System einen eindeutig definierten sicheren Zustand annehmen. Ohne überprüfte Umsetzung gibt es noch keine funktionsfähige Relaissteuerung.

## Softwarestand und nächste Schritte

- `firmware/ph_calibration_tool/ph_calibration_tool.ino`: Sensor-Hardwaretest, pH-Kalibrierung/Alarme; **keine Relais-GPIOs mehr**.
- `firmware/growroom_telemetry_client/growroom_telemetry_client.ino`: Sensor-Messwerte, WLAN, lokaler API-Endpunkt und Sollwertverwaltung; alte lokale Relais-GPIO-Schaltung **entfernt**. C3 ist noch nicht angebunden. API meldet für Aktoren `esp32_c3_not_connected`.
- `firmware/ds18b20_diagnostic/ds18b20_diagnostic.ino`: DS18B20-Prüfung auf GPIO23, wenn die Ersatzfühler eintreffen.
- Das separate C3-Programm sowie die WLAN-Anbindung und ihre Hardwaretests stehen noch aus.
- Sensorplatine erst vollständig mit Beschriftungen/Kontinuitätsmessungen prüfen; keine Netzspannungsseite während der Sensor-Diagnose.

**Nicht mehr verwenden:** alter Ein-ESP32-Wiringplan, alte direkte SSR-Pinbelegung am Sensorboard und `firmware/powerbox_actuator/` als vermeintliche fertige ESP32-C3-Firmware.
