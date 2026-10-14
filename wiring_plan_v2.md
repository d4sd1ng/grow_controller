# Grow Controller — Verdrahtung V8: DS18B20 D23, RGB-Grün D32

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
| - 1× DS18B20 (Wasser)|    |   noch NICHT integriert  |
| - BH1750 (geplant)     |    | - Fail-safe: zu planen   |
| - MLX90640 Kamera      |    |                          |
+----------+-------------+    +-----------+-------------+
           | WLAN                         | WLAN
           +----------> Raspberry Pi <----+
                            |
                     CYD / Dashboard
```

**Keine SSR-/Relaisleitungen auf der 26×30-Sensorplatine.** Der ESP32-C3 ist ein zweites Gerät, kein Aufsteckmodul dieser Platine. Die alte Einzel-ESP32-Zeichnung und die Zuordnung GPIO17/18/19/33 zu den Relais sind überholt. Die bisherige direkte GPIO17/18/19/33-Relaiszuordnung ist hinfällig. **D18 ist am tatsächlich abgelesenen OLED-ESP32 nicht herausgeführt; der optionale, deaktivierte pH-Buzzer wird daher auf D19 reserviert.**

## Sensor-ESP32: bestätigte Stiftkontakte und Signalzuordnung

Die Beschriftungen richten sich nach den **vom Nutzer direkt abgelesenen Board-Pins**, nicht nach einem generischen ESP32-DevKit-Pinout. Ein endgültiger Loch-für-Loch-Aufbau setzt den realen Lochabstand und die Modulmaße voraus.

| Sensor/Modul | Anschluss am Sensor-ESP32 | Zweck | Bauzustand |
|---|---|---|---|
| Internes OLED | **GPIO21 SDA / GPIO22 SCL** | I2C-Bus A, Adresse 0x3C | vorhanden |
| ADS1115 | **GPIO21 (D21) SDA / GPIO22 (D22) SCL**, 3V3, GND | I²C-Bus A gemeinsam mit internem OLED, Adresse 0x48 (ADDR=GND) | ursprüngliche Zuordnung wiederhergestellt |
| PH4502C PO | ADS1115 **A0** | pH-Messung, vorhandene kalibrierte Signalkette **nicht umbauen** | kalibriert |
| CQRobot TDS/EC analog | ADS1115 **A1** | EC-Spannung, vorhandene Signalkette erhalten | bereits angeschlossen |
| DS18B20 | **GPIO23 (D23)** | Wasser-Temperatur, 1-Wire, ein Sensor | Erkennung vom Nutzer bestätigt (vor Pinrückverlegung) |
| DHT22 #1 | **GPIO25 (D25)** | Pflanzenzone | vorgesehen |
| DHT22 #2 | **GPIO26 (D26)** | Zuluft-/Unterzone | vorgesehen |
| DHT22 #3 | **GPIO27 (D27)** | obere Zone | vorgesehen |
| BH1750 | **GPIO13 (D13) SDA**, **GPIO14 (D14) SCL**, ADDR an GND | separater I²C-Bus B, Adresse 0x23 | vorgesehen |
| MLX90640 Infrarotkamera | **GPIO21 (D21) SDA**, **GPIO22 (D22) SCL** | Bus A mit OLED, Adresse 0x33, Kamerastecker vorbereiten | Anschluss einplanen, Modul noch nicht geprüft |
| Ein 4-Pin-Phasentaster | **GPIO16 (D16)** nach GND, `INPUT_PULLUP` | Je ein Pin aus den beiden Schaltpaaren; 50 ms Entprellung, jeder Tastendruck wechselt den Modus | vorgesehen |
| Eine 4-Pin-RGB-LED: Grün | **GPIO32 (D32)** über **440 Ω (2×220 Ω in Reihe)** | WUCHS = Grün, BLÜTE = Rot und Grün gleichzeitig | gemeinsame Kathode bestätigt |
| Dieselbe RGB-LED: Rot | **GPIO33 (D33)** über **440 Ω (2×220 Ω in Reihe)** | Blauer LED-Kanal bleibt frei | gemeinsame Kathode bestätigt |
| pH-Buzzer | **GPIO19 (D19)** als reservierter Steuersignalpin | 2-poliger Buzzer; Bauart/Strom/Spannung unbekannt; über Treiber vorsehen, Firmware deaktiviert | noch nicht anschließen |

**Ausdrücklich nicht verdrahten:** PH4502C **TO -> ADS A2** (TO wurde mit 3,3–3,4 V gemessen, bei ADS-VDD 3,3 V nicht zulässig); PH4502C **DO -> GPIO23** (GPIO23 ist für den DS18B20 reserviert, DO wird nicht benötigt). GPIO17/18/19/33 als Relaisleitungen am Sensor-ESP entfallen. GPIO33 wird nun für den RGB-Rotkanal, GPIO19 als Buzzer-Reserve genutzt.

## 26 × 30 Lochraster — konkreter Bestückungsplan (Kleinspannung)

**Achtung:** 26×30 bezeichnet 26 × 30 **Bohrungen**, nicht Millimeter. Bei 2,54-mm-Raster beträgt der **Abstand zwischen den äußersten Lochmittelpunkten** 63,50 × 73,66 mm; die Außenmaße der Leiterplatte können davon abweichen. Bestueckung erst trocken auflegen, da genaue OLED-ESP32-/ADS1115-Modulabmessungen und Reihenabstände nicht zuverlässig vorliegen.

| Bauteil / Anschluss | Auf der Sensorplatine vorbereiten | Bemerkung |
|---|---|---|
| Sensor-ESP32 mit OLED | **2 × 15-polige Buchsenleiste**, nur wenn der reale ESP32 die bestätigten 15 Kontakte je Seite und passendes Raster besitzt | USB und OLED müssen erreichbar bleiben |
| ADS1115-Modul | passende **Buchsenleisten / Stiftadapter** für die tatsächlich vorhandenen Modul-Pins | pH A0/EC A1; A2/A3 frei |
| DS18B20-Bus | **1 × 3-poliger Anschluss: 3V3, GND, DATA D23** | genau ein Wassertemperatursensor |
| DS-Pull-up | **1 × 4,7 kOhm** zwischen DATA D23 und 3V3 | genau EIN Pull-up am Bus |
| DHT22 #1–#3 | **3 × 3-poliger Anschluss: 3V3, GND, DATA** | DATA D25/D26/D27 |
| DHT-Pull-ups | **bis zu 3 × 4,7 kOhm**, jeweils DATA gegen 3V3 | nur falls das konkrete DHT22-Modul keinen eigenen Pull-up hat |
| BH1750 | **1 × 5-poliger Anschluss: 3V3, GND, SDA D13, SCL D14, ADDR** | ADDR an GND: Adresse 0x23; Board-Aufdruck ggf. „SCA“ für SDA prüfen |
| pH-/EC-Signale | vorhandene kalibrierte/verifizierte Verbindungen A0/A1 sauber übernehmen; geeignete Steckklemmen | keine neue, unbelegte 10k/20k-Teilerannahme |
| Spannungs-/Masseverteilung | getrennte **3V3- und GND-Verbindungen** auf Lochraster mit isoliertem Draht | Lochraster hat keine automatischen Stromschienen |
| **MLX90640 Infrarotkamera** | **1 × 4-poliger Anschluss: GND, SDA D21, SCL D22, VCC/VIN** | Kamera außerhalb des Controllergehäuses, Versorgung erst nach Abgleich der Modulvariante festlegen |
| **Ein 4-Pin-Taster** | **D16/GND:** je einen Pin aus zwei unterschiedlichen Kontaktpaaren verwenden, die nur beim Drücken verbunden werden | Kontaktpaare vor dem Verlöten mit Durchgangsprüfer identifizieren |
| **Eine 4-Pin-RGB-LED** | **Grün D32 über 440 Ω**, **Rot D33 über 440 Ω**, Blau frei, gemeinsame Kathode → GND | Jeder 440-Ω-Vorwiderstand besteht aus **2 × 220 Ω in Reihe** |
| **Buzzer-/Treiber-Reserve** | **D19 als Steuersignal, Platz für 2-poligen Buzzer-Anschluss und Transistor-Treiber vorsehen** | Kein unbekannter Buzzer direkt an GPIO; Versorgung und aktiver/passiver Typ prüfen |

**Mechanische Reihenfolge:** zuerst ESP32 und ADS1115 probeweise positionieren; USB-/OLED-Freiraum und Sensor-Steckverbinder prüfen; dann Buchsenleisten und Pull-up-Widerstände verlöten; anschließend Kleinspannungs-Signale und Masse einzeln durchmessen. Steckverbinder nach Signalname und Spannung beschriften. Die DHT22-/BH1750-/DS-Sensoren und die MLX90640-Kamera messen außerhalb des warmen Gehäuses; daher **nur ihre Anschlüsse**, nicht die Messköpfe selbst, auf der Lochrasterplatine anordnen. Für Kamera und Bus A die Leitung möglichst kurz halten; I2C-Betrieb mit 400 kHz ist mit längeren Kabeln nicht garantiert.

### Versorgung

ADS1115, DS18B20, DHT22 und BH1750: geplante **3,3-V-Logik**. **MLX90640: Versorgung gemäß Datenblatt des tatsächlich verbauten Breakout-Moduls; der VCC/VIN-Kontakt bleibt bis dahin unbeschaltet.** Versorgung von konkretem Breakout abhängig; prüfen, ob der 3,3-V-Regler des ESP32 den gesamten Strom liefern kann. PH4502C VCC wurde mit **4,7 V** gemessen; ihre bisher funktionierende Versorgung und A0-Messkette nicht ohne Grund verändern. Alle beteiligten Kleinspannungs-Sensoren benötigen eine passende gemeinsame Signalmasse.

## Bedienfeld: EIN 4-Pin-Taster, EINE 4-Pin-RGB-LED und 2-Pin-Buzzer

**4-Pin-Taster (momentaner Schließer):** Bei üblichen Tact-Switches sind zwei Kontakte je Seite intern dauerhaft verbunden. **Nicht nach Orientierung raten:** Mit dem Multimeter ein Kontaktpaar ermitteln, das ohne Druck **offen** und nur beim Drücken leitend ist. **Einen Kontakt an D16**, den anderen an **GND**. Die zwei übrigen Kontakte werden nicht verbunden. `INPUT_PULLUP`, softwareseitig 50 ms Entprellung. **Jeder Tastendruck wechselt WUCHS ↔ BLÜTE.** Auswahl im ESP32-NVS `growphase_v1`.

**4-Pin-RGB-LED:** Vier Kontakte entsprechen gemeinsamem Anschluss, Rot, Grün und Blau. Der Nutzer hat **gemeinsame Kathode** bestätigt: gemeinsamen Kontakt an GND. Rot- und Grünanschluss über die beiden Vorwiderstände; Blau bleibt frei.

- **D32 → 440 Ω (2 × 220 Ω in Reihe) → Grün**
- **D33 → 440 Ω (2 × 220 Ω in Reihe) → Rot**
- Blau bleibt unbenutzt.
- **Gemeinsame Kathode: gemeinsam an GND**, HIGH schaltet jeweilige Farbe ein.

Die Firmware ist auf **Grün = WUCHS** und **Rot + Grün = Gelb = BLÜTE** umgestellt. **Gemeinsame Kathode steht fest** (`GROW_RGB_COMMON_ANODE=0`); RGB-Ausgänge sind im jetzt aktualisierten Sketch aktiviert (`GROW_ENABLE_PHASE_RGB=1`). **Vor dem Flashen zuerst DS-DATA auf D23 und RGB-Grün auf D32 umverdrahten.**

**Zweipoliger pH-Buzzer aus Sensor-Pack:** Auf der Platine **D19 als Buzzer-Steuersignal und Platz für einen 2-poligen Buzzer-Anschluss sowie einen geeigneten Transistortreiber** vorsehen. Bei unbekanntem Typ/Strom/Spannung ist **kein direkter GPIO-Anschluss freigegeben**. Auch ein 2-Pin-Bauteil kann aktiv (Gleichspannung) oder passiv (Tonerzeugung via PWM erforderlich) sein. Die Versorgung und der Treiber werden **erst nach Identifizierung** des Bauteils festgelegt; der aktuell implementierte Buzzer-Code ist für ein aktives, logikgesteuertes Modul und bleibt deaktiviert (`GROW_ENABLE_PH_BUZZER=0`). **D18 nicht verwenden:** am konkret vorhandenen ESP32-Board nicht herausgeführt.

**pH-Alarm:** LOW bei pH unter **5,5**; HIGH bei pH über **6,2**; Normalbereich 5,5–6,2 inklusive (bei Statuswechseln weiterhin 0,1 pH Hysterese und drei Messungen). Vorbereitete Tonsignale für aktiven Buzzer: LOW zweimal kurz alle 5 s, HIGH dreimal kurz alle 5 s, Sensorfehler einmal alle 10 s.

**Software/Phasensteuerung:** Die Phase wird weiterhin auf OLED, API und Telemetrie ausgegeben. BLÜTE-Klimasollwerte sind noch nicht abgestimmt; die automatische Klimaregelung für BLÜTE bleibt gesperrt. Die pH-/EC-Wochensollwerte und pH-Alarmgrenzen werden durch den Tasterwechsel nicht verändert.

## MLX90640-Kameraanschluss (jetzt reserviert)

**Für die 26×30-Lochrasterplatine einen 4-poligen Stecker vorsehen:**

| Kontakt | Verbindung | Bus |
|---|---|---|
| SDA | ESP32 D21 / GPIO21 | I²C A, gemeinsam mit OLED |
| SCL | ESP32 D22 / GPIO22 | I²C A, gemeinsam mit OLED |
| GND | Signalmasse | gemeinsame Kleinspannungsmasse |
| VCC/VIN | **noch offen – Modulversorgung prüfen** | nicht ohne Modulprüfung mit 3,3 V oder 5 V verbinden |

Die MLX90640 hat standardmäßig die I²C-Adresse **0x33**; das OLED **0x3C**. Der ADS1115 (**0x48**) teilt sich den Bus A auf **D21/D22** mit OLED und geplanter Kamera. Der BH1750 (**0x23**) bleibt allein auf **Bus B D13/D14**. Dafür sind keine weiteren ESP32-GPIOs nötig. Falls das Kameramodul I²C-Pull-ups hat, deren Spannung und Gesamtwirkung mit dem OLED-Modul prüfen; alle Datenleitungen müssen **3,3-V-kompatibel** bleiben.

Die **Wärmebildkamera** benötigt freie Sicht auf die Blätter; nicht hinter gewöhnliches Glas/Acryl oder in das geschlossene Sensorgehäuse montieren. Kamera-Bildfeld und Ausrichtung erst am aufgebauten Salatbeet festlegen. Eine längere I²C-Leitung muss elektrisch validiert werden, bevor die Firmware mit 400 kHz arbeitet.

**Software:** MLX90640 ist in Diagnose- und Produktionssketch bereits vorbereitet (`GROW_ENABLE_MLX90640=0` bis Einbau und Bibliotheksinstallation). Daten werden zunächst als thermische ROI und möglicher Blatt-VPD-Kandidat erhoben, **ohne automatische Aktorregelung**.

## Festgelegte pH-/EC-Wochenprofile (Firmware, keine Zusatzverkabelung)

pH-Alarm ist fest **unter 5,5** bzw. **über 6,2** (inklusive Grenzen als Normalbereich, bestehende Hysterese). EC steht für **elektrische Leitfähigkeit in mS/cm** und ist als Wochen-Sollwert vorgesehen, noch nicht als EC-Alarm.

| Anbauwoche | pH-Soll | EC-Soll in mS/cm |
|---|---:|---:|
| 1 | 6,0 | 1,6 |
| 2 | 5,8 | 1,8 |
| 3–7 | 5,5 | 2,0 |
| 8 | 5,5 | 2,5 |
| 9–10 | 5,5 | 1,8 |
| 11–12 | 5,5 | 0,2 |

Die Wochenzahl wird auf dem Sensor-ESP32 getrennt von der Phasentaster-Auswahl gespeichert. Ohne festgelegtes Startdatum wird die Woche **nicht automatisch hochgezählt**. Manuell `k1` bis `k12` über seriell oder per `POST /api/local/control/week`. Für diese Werte sind **keine weiteren Pins oder Lötstellen** erforderlich.

## ESP32-C3 SuperMini — eigener Aufbau

Der C3 liegt **im separaten Leistungsgehäuse**. Die Vier-Kanal-Zuordnung bleibt funktional **Licht / Umluft / Abluft / Heizstrahler**, aber sie ist **nicht** auf dem Sensor-ESP32 zu verlöten. Konkrete C3-GPIOs, Eingangsstrom-/Pegel-Kompatibilität der eingesetzten Relais und die vollständige Fail-safe-Strategie sind noch zu verifizieren; darum enthält dieser Plan absichtlich **keine erfundene C3-Pinbelegung oder Lastverdrahtung**.

Geplante Kommunikation: über WLAN, mit getrennten Zuständen **angefordert**, **vom C3 bestätigt**, **Kommunikation verloren**. Bei Kommunikationsfehlern muss das System einen eindeutig definierten sicheren Zustand annehmen. Ohne überprüfte Umsetzung gibt es noch keine funktionsfähige Relaissteuerung.

## Softwarestand und nächste Schritte

- `firmware/ph_calibration_tool/ph_calibration_tool.ino`: Sensor-Hardwaretest, pH-Kalibrierung/Alarme; **keine Relais-GPIOs mehr**.
- `firmware/growroom_telemetry_client/growroom_telemetry_client.ino`: Sensor-Messwerte, WLAN, lokaler API-Endpunkt und Sollwertverwaltung; alte lokale Relais-GPIO-Schaltung **entfernt**. C3 ist noch nicht angebunden. API meldet für Aktoren `esp32_c3_not_connected`.
- `firmware/ds18b20_diagnostic/ds18b20_diagnostic.ino`: DS18B20-Prüfung auf GPIO23 (nach physischer Umverdrahtung erneut testen).
- Das separate C3-Programm sowie die WLAN-Anbindung und ihre Hardwaretests stehen noch aus.
- Sensorplatine erst vollständig mit Beschriftungen/Kontinuitätsmessungen prüfen; keine Netzspannungsseite während der Sensor-Diagnose.

**Nicht mehr verwenden:** alter Ein-ESP32-Wiringplan, alte direkte SSR-Pinbelegung am Sensorboard und `firmware/powerbox_actuator/` als vermeintliche fertige ESP32-C3-Firmware.
