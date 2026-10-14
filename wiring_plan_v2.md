# Grow Controller — Verdrahtung V5: zwei ESP32 und MLX90640

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
| - MLX90640 Kamera      |    |                          |
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
| Internes OLED | **GPIO21 SDA / GPIO22 SCL** | I2C-Bus A, Adresse 0x3C | vorhanden |
| ADS1115 | **GPIO13 SDA / GPIO14 SCL**, 3V3, GND | I2C-Bus B mit BH1750; Adresse 0x48 (ADDR=GND) | Buswechsel per Firmware vorgesehen |
| PH4502C PO | ADS1115 **A0** | pH-Messung, vorhandene kalibrierte Signalkette **nicht umbauen** | kalibriert |
| CQRobot TDS/EC analog | ADS1115 **A1** | EC-Spannung, vorhandene Signalkette erhalten | bereits angeschlossen |
| DS18B20 | **GPIO32 (D32)** | Wasser-Temperatur, 1-Wire | Ersatzsensoren noch nicht geliefert |
| DHT22 #1 | **GPIO25 (D25)** | Pflanzenzone | vorgesehen |
| DHT22 #2 | **GPIO26 (D26)** | Zuluft-/Unterzone | vorgesehen |
| DHT22 #3 | **GPIO27 (D27)** | obere Zone | vorgesehen |
| BH1750 | **GPIO13 (D13) SDA**, **GPIO14 (D14) SCL**, ADDR an GND | Bus B mit ADS1115, Adresse 0x23 | vorgesehen |
| MLX90640 Infrarotkamera | **GPIO21 (D21) SDA**, **GPIO22 (D22) SCL** | Bus A mit OLED, Adresse 0x33, Kamerastecker vorbereiten | Anschluss einplanen, Modul noch nicht geprüft |

**Ausdrücklich nicht verdrahten:** PH4502C **TO -> ADS A2** (TO wurde mit 3,3–3,4 V gemessen, bei ADS-VDD 3,3 V nicht zulässig); PH4502C **DO -> GPIO32** (GPIO32 ist für den DS18B20 reserviert, DO wird nicht benötigt). GPIO17/18/19/33 als Relaisleitungen am Sensor-ESP entfallen. GPIO33 bleibt Reserve.

## 26 × 30 Lochraster — konkreter Bestückungsplan (Kleinspannung)

**Achtung:** 26×30 bezeichnet 26 × 30 **Bohrungen**, nicht Millimeter. Bei 2,54-mm-Raster beträgt der **Abstand zwischen den äußersten Lochmittelpunkten** 63,50 × 73,66 mm; die Außenmaße der Leiterplatte können davon abweichen. Bestueckung erst trocken auflegen, da genaue OLED-ESP32-/ADS1115-Modulabmessungen und Reihenabstände nicht zuverlässig vorliegen.

| Bauteil / Anschluss | Auf der Sensorplatine vorbereiten | Bemerkung |
|---|---|---|
| Sensor-ESP32 mit OLED | **2 × 15-polige Buchsenleiste**, nur wenn der reale ESP32 die bestätigten 15 Kontakte je Seite und passendes Raster besitzt | USB und OLED müssen erreichbar bleiben |
| ADS1115-Modul | passende **Buchsenleisten / Stiftadapter** für die tatsächlich vorhandenen Modul-Pins | pH A0/EC A1; A2/A3 frei |
| DS18B20-Bus | **1 × 3-poliger Anschluss: 3V3, GND, DATA D32** | beide DS18B20 können denselben Bus nutzen |
| DS-Pull-up | **1 × 4,7 kOhm** zwischen DATA D32 und 3V3 | genau EIN gemeinsamer Pull-up am Bus |
| DHT22 #1–#3 | **3 × 3-poliger Anschluss: 3V3, GND, DATA** | DATA D25/D26/D27 |
| DHT-Pull-ups | **bis zu 3 × 4,7 kOhm**, jeweils DATA gegen 3V3 | nur falls das konkrete DHT22-Modul keinen eigenen Pull-up hat |
| BH1750 | **1 × 5-poliger Anschluss: 3V3, GND, SDA D13, SCL D14, ADDR** | ADDR an GND: Adresse 0x23; Board-Aufdruck ggf. „SCA“ für SDA prüfen |
| pH-/EC-Signale | vorhandene kalibrierte/verifizierte Verbindungen A0/A1 sauber übernehmen; geeignete Steckklemmen | keine neue, unbelegte 10k/20k-Teilerannahme |
| Spannungs-/Masseverteilung | getrennte **3V3- und GND-Verbindungen** auf Lochraster mit isoliertem Draht | Lochraster hat keine automatischen Stromschienen |
| **MLX90640 Infrarotkamera** | **1 × 4-poliger Anschluss: GND, SDA D21, SCL D22, VCC/VIN** | Kamera außerhalb des Controllergehäuses, Versorgung erst nach Abgleich der Modulvariante festlegen |

**Mechanische Reihenfolge:** zuerst ESP32 und ADS1115 probeweise positionieren; USB-/OLED-Freiraum und Sensor-Steckverbinder prüfen; dann Buchsenleisten und Pull-up-Widerstände verlöten; anschließend Kleinspannungs-Signale und Masse einzeln durchmessen. Steckverbinder nach Signalname und Spannung beschriften. Die DHT22-/BH1750-/DS-Sensoren und die MLX90640-Kamera messen außerhalb des warmen Gehäuses; daher **nur ihre Anschlüsse**, nicht die Messköpfe selbst, auf der Lochrasterplatine anordnen. Für Kamera und Bus A die Leitung möglichst kurz halten; I2C-Betrieb mit 400 kHz ist mit längeren Kabeln nicht garantiert.

### Versorgung

ADS1115, DS18B20, DHT22 und BH1750: geplante **3,3-V-Logik**. **MLX90640: Versorgung gemäß Datenblatt des tatsächlich verbauten Breakout-Moduls; der VCC/VIN-Kontakt bleibt bis dahin unbeschaltet.** Versorgung von konkretem Breakout abhängig; prüfen, ob der 3,3-V-Regler des ESP32 den gesamten Strom liefern kann. PH4502C VCC wurde mit **4,7 V** gemessen; ihre bisher funktionierende Versorgung und A0-Messkette nicht ohne Grund verändern. Alle beteiligten Kleinspannungs-Sensoren benötigen eine passende gemeinsame Signalmasse.

## MLX90640-Kameraanschluss (jetzt reserviert)

**Für die 26×30-Lochrasterplatine einen 4-poligen Stecker vorsehen:**

| Kontakt | Verbindung | Bus |
|---|---|---|
| SDA | ESP32 D21 / GPIO21 | I²C A, gemeinsam mit OLED |
| SCL | ESP32 D22 / GPIO22 | I²C A, gemeinsam mit OLED |
| GND | Signalmasse | gemeinsame Kleinspannungsmasse |
| VCC/VIN | **noch offen – Modulversorgung prüfen** | nicht ohne Modulprüfung mit 3,3 V oder 5 V verbinden |

Die MLX90640 hat standardmäßig die I²C-Adresse **0x33**; das OLED **0x3C**. ADS1115 **0x48** und BH1750 **0x23** bleiben gemeinsam auf **Bus B D13/D14**, daher sind keine weiteren ESP32-GPIOs nötig. Falls das Kameramodul I²C-Pull-ups hat, deren Spannung und Gesamtwirkung mit dem OLED-Modul prüfen; alle Datenleitungen müssen **3,3-V-kompatibel** bleiben.

Die **Wärmebildkamera** benötigt freie Sicht auf die Blätter; nicht hinter gewöhnliches Glas/Acryl oder in das geschlossene Sensorgehäuse montieren. Kamera-Bildfeld und Ausrichtung erst am aufgebauten Salatbeet festlegen. Eine längere I²C-Leitung muss elektrisch validiert werden, bevor die Firmware mit 400 kHz arbeitet.

**Software:** MLX90640 ist in Diagnose- und Produktionssketch bereits vorbereitet (`GROW_ENABLE_MLX90640=0` bis Einbau und Bibliotheksinstallation). Daten werden zunächst als thermische ROI und möglicher Blatt-VPD-Kandidat erhoben, **ohne automatische Aktorregelung**.

## ESP32-C3 SuperMini — eigener Aufbau

Der C3 liegt **im separaten Leistungsgehäuse**. Die Vier-Kanal-Zuordnung bleibt funktional **Licht / Umluft / Abluft / Heizstrahler**, aber sie ist **nicht** auf dem Sensor-ESP32 zu verlöten. Konkrete C3-GPIOs, Eingangsstrom-/Pegel-Kompatibilität der eingesetzten Relais und die vollständige Fail-safe-Strategie sind noch zu verifizieren; darum enthält dieser Plan absichtlich **keine erfundene C3-Pinbelegung oder Lastverdrahtung**.

Geplante Kommunikation: über WLAN, mit getrennten Zuständen **angefordert**, **vom C3 bestätigt**, **Kommunikation verloren**. Bei Kommunikationsfehlern muss das System einen eindeutig definierten sicheren Zustand annehmen. Ohne überprüfte Umsetzung gibt es noch keine funktionsfähige Relaissteuerung.

## Softwarestand und nächste Schritte

- `firmware/ph_calibration_tool/ph_calibration_tool.ino`: Sensor-Hardwaretest, pH-Kalibrierung/Alarme; **keine Relais-GPIOs mehr**.
- `firmware/growroom_telemetry_client/growroom_telemetry_client.ino`: Sensor-Messwerte, WLAN, lokaler API-Endpunkt und Sollwertverwaltung; alte lokale Relais-GPIO-Schaltung **entfernt**. C3 ist noch nicht angebunden. API meldet für Aktoren `esp32_c3_not_connected`.
- `firmware/ds18b20_diagnostic/ds18b20_diagnostic.ino`: DS18B20-Prüfung auf GPIO32, wenn die Ersatzfühler eintreffen.
- Das separate C3-Programm sowie die WLAN-Anbindung und ihre Hardwaretests stehen noch aus.
- Sensorplatine erst vollständig mit Beschriftungen/Kontinuitätsmessungen prüfen; keine Netzspannungsseite während der Sensor-Diagnose.

**Nicht mehr verwenden:** alter Ein-ESP32-Wiringplan, alte direkte SSR-Pinbelegung am Sensorboard und `firmware/powerbox_actuator/` als vermeintliche fertige ESP32-C3-Firmware.
