# Grow Controller — Wiring Plan V2: Sensor-ESP32 und 5 Relais (Stand 15.10.2026)

**Stand:** 2026-10-15  
**Hardware:** (1) Sensor-ESP32 mit integriertem 0,96"-OLED auf **26 × 30 Lochraster**, (2) **ESP32-C3 SuperMini** im separaten Leistungsgehäuse für **genau fünf Relais**: zwei Lampen gemeinsam, Abluft, Umluft, Heizung und geplante Filter-/Umwälzpumpe. Der Pumpen-Wartungstaster kommt zusätzlich zum Phasentaster.  
**Status:** Die **pH-Kalibrierung funktioniert laut aktueller Nutzerbestätigung**. Die Funktionen der fünf Relais sind nun benannt, jedoch sind ihre C3-GPIO-Zuordnung, die Leistungsansteuerung, die WLAN-Kommunikation zwischen beiden ESP32 und die Pumpen-Wartungssperre noch nicht implementiert oder getestet. Die einzelnen Sensoren und der neue RGB-GPIO4 sind nicht allein durch die Pinliste als erfolgreich getestet anzusehen.

> Dieser Plan behandelt ausschliesslich die Kleinspannungs-Sensorplatine und die logische Rollenverteilung. Die 230-V-Seite ist nicht Gegenstand des Plans; Montage/Pruefung gehoeren in fachkundige Haende.

## Architektur

```text
                         WLAN
        +-------------------+-------------------+
        |                                       |
+-------v----------------+          +-----------v-----------------+
| Sensor-ESP32 + OLED    |          | ESP32-C3 SuperMini          |
| 26 x 30 Lochraster     |          | separates Leistungsgehaeuse |
| - ADS1115: pH / EC     |          | - 5 Relais (geplant):        |
| - DS18B20: Wasser      |          |   Licht fuer 2 Lampen       |
| - DHT22, BH1750        |          |   Abluft / Umluft           |
| - Kamera: geplant      |          |   Heizung / Pumpe           |
| - Phasentaster D19     |          | - extra Pumpen-Wartungstaster|
+-----------+------------+          +-------------+--------------+
            | WLAN                                | WLAN
            +--------------> Raspberry Pi <--------+
                                 |
                            CYD / Dashboard
```

**Keine der fünf Relaisleitungen gehört auf die 26×30-Sensorplatine.** Der ESP32-C3 ist ein eigenes Board im Leistungsgehäuse; frühere direkte SSR-Pinbelegungen des Sensor-ESP32 sind hinfällig. **D18 ist am tatsächlich abgelesenen OLED-ESP32 nicht herausgeführt.** Am Sensor-ESP32: bestehender Phasentaster D19, Buzzer-Signal D16 (in der Firmware noch deaktiviert). Ein **zweiter, unabhängiger Pumpen-Wartungstaster** ist vorgesehen; Platzierung und Anschluss am C3 sind noch festzulegen.

## Sensor-ESP32: bestätigte Stiftkontakte und Signalzuordnung

Die Beschriftungen richten sich nach den **vom Nutzer direkt abgelesenen Board-Pins**, nicht nach einem generischen ESP32-DevKit-Pinout. Ein endgültiger Loch-für-Loch-Aufbau setzt den realen Lochabstand und die Modulmaße voraus.

| Sensor/Modul | Anschluss am Sensor-ESP32 | Zweck | Bauzustand |
|---|---|---|---|
| Internes OLED | **GPIO21 SDA / GPIO22 SCL** | I2C-Bus A, Adresse 0x3C | vorhanden |
| ADS1115 | **GPIO21 (D21) SDA / GPIO22 (D22) SCL**, 3V3, GND | I²C-Bus A gemeinsam mit internem OLED, Adresse 0x48 (ADDR=GND) | ursprüngliche Zuordnung wiederhergestellt |
| PH4502C PO | ADS1115 **A0** | pH-Messung; bei pH-7-/pH-4-Erfassung wird die aktuelle DS18B20-Wassertemperatur gespeichert | **Kalibrierung funktioniert laut Nutzer** |
| CQRobot TDS/EC analog | ADS1115 **A1** | EC-Spannung, vorhandene Signalkette erhalten | bereits angeschlossen |
| DS18B20 | **GPIO23 (D23)** | Wasser-Temperatur, 1-Wire, ein Sensor | Erkennung vom Nutzer bestätigt (vor Pinrückverlegung) |
| DHT22 #1 | **GPIO25 (D25)** | Pflanzenzone | vorgesehen |
| DHT22 #2 | **GPIO26 (D26)** | Zuluft-/Unterzone | vorgesehen |
| DHT22 #3 | **GPIO27 (D27)** | obere Zone | vorgesehen |
| BH1750 | **GPIO13 (D13) SDA**, **GPIO14 (D14) SCL**, ADDR an GND | separater I²C-Bus B, Adresse 0x23 | vorgesehen |
| MLX90640 Infrarotkamera | **GPIO21 (D21) SDA**, **GPIO22 (D22) SCL** | Bus A mit OLED, Adresse 0x33, Kamerastecker vorbereiten | Anschluss einplanen, Modul noch nicht geprüft |
| Ein 4-Pin-Phasentaster | **GPIO19 (D19)** nach GND, `INPUT_PULLUP` | Je ein Pin aus den beiden Schaltpaaren; 50 ms Entprellung, jeder Tastendruck wechselt den Modus | vorgesehen |
| Eine 4-Pin-RGB-LED: Grün | **GPIO4 (D4)** über **440 Ω (2×220 Ω in Reihe)** | WUCHS = Grün, BLÜTE = Rot und Grün gleichzeitig | gemeinsame Kathode bestätigt |
| Dieselbe RGB-LED: Rot | **GPIO33 (D33)** über **440 Ω (2×220 Ω in Reihe)** | Blauer LED-Kanal bleibt frei | gemeinsame Kathode bestätigt |
| pH-Buzzer | **GPIO16 (D16)** als zugeordneter Steuersignalpin | 2-poliger Buzzer; Bauart/Strom/Spannung und benötigter Treiber noch prüfen; Firmware deaktiviert | Pinzuordnung vom Nutzer bestätigt, elektrischer Aufbau noch nicht verifiziert |

**Nicht verbinden:** PH4502C **TO → ADS A2** (TO wurde mit 3,3–3,4 V gemessen, bei ADS-VDD 3,3 V nicht zulässig); PH4502C **DO → GPIO23** (D23 ist für den DS18B20). Frühere Relaisleitungen am Sensor-ESP entfallen. **D4 Grün, D33 Rot, D19 Taster, D16 Buzzer-Signal (deaktiviert), D23 DS18B20.**

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
| pH-/EC-Signale | PO an ADS A0 (pH-Kalibrierung funktioniert); EC-Analogausgang an ADS A1 (EC-Kalibrierstatus separat prüfen) | keine neue, unbelegte 10k/20k-Teilerannahme |
| Spannungs-/Masseverteilung | getrennte **3V3- und GND-Verbindungen** auf Lochraster mit isoliertem Draht | Lochraster hat keine automatischen Stromschienen |
| **MLX90640 Infrarotkamera** | **1 × 4-poliger Anschluss: GND, SDA D21, SCL D22, VCC/VIN** | Kamera außerhalb des Controllergehäuses, Versorgung erst nach Abgleich der Modulvariante festlegen |
| **Ein 4-Pin-Taster** | **D19/GND:** je einen Pin aus zwei unterschiedlichen Kontaktpaaren verwenden, die nur beim Drücken verbunden werden | Kontaktpaare vor dem Verlöten mit Durchgangsprüfer identifizieren |
| **Eine 4-Pin-RGB-LED** | **Grün D4 über 440 Ω**, **Rot D33 über 440 Ω**, Blau frei, gemeinsame Kathode → GND | Jeder 440-Ω-Vorwiderstand besteht aus **2 × 220 Ω in Reihe** |
| **Buzzer-/Treiber-Reserve** | **D16 als Steuersignal, Platz für 2-poligen Buzzer-Anschluss und Transistor-Treiber vorsehen** | Kein unbekannter Buzzer direkt an GPIO; Versorgung und aktiver/passiver Typ prüfen |

**Mechanische Reihenfolge:** zuerst ESP32 und ADS1115 probeweise positionieren; USB-/OLED-Freiraum und Sensor-Steckverbinder prüfen; dann Buchsenleisten und Pull-up-Widerstände verlöten; anschließend Kleinspannungs-Signale und Masse einzeln durchmessen. Steckverbinder nach Signalname und Spannung beschriften. Die DHT22-/BH1750-/DS-Sensoren und die MLX90640-Kamera messen außerhalb des warmen Gehäuses; daher **nur ihre Anschlüsse**, nicht die Messköpfe selbst, auf der Lochrasterplatine anordnen. Für Kamera und Bus A die Leitung möglichst kurz halten; I2C-Betrieb mit 400 kHz ist mit längeren Kabeln nicht garantiert.

### Versorgung

ADS1115, DS18B20, DHT22 und BH1750: vorgesehene **3,3-V-Logik**. **MLX90640: Versorgung gemäß Datenblatt des tatsächlich verbauten Breakout-Moduls; VCC/VIN bis dahin unbeschaltet lassen.** Prüfen, ob die ESP32-3V3-Versorgung für die gesamte Sensorlast ausreicht. PH4502C VCC wurde mit **4,7 V** gemessen; die laut Nutzer funktionierende pH-Messkette nicht eigenmächtig umbauen. Alle Kleinspannungs-Sensoren benötigen eine gemeinsame Signalmasse.

## Sensor-Bedienfeld: Phasentaster, RGB-LED und Buzzer; weiterer Pumpentaster am Leistungsteil geplant

**Bestehender 4-Pin-Phasentaster (momentaner Schließer, NICHT der neue Pumpentaster):** Bei üblichen Tact-Switches sind zwei Kontakte je Seite intern dauerhaft verbunden. **Nicht nach Orientierung raten:** Mit dem Multimeter ein Kontaktpaar ermitteln, das ohne Druck **offen** und nur beim Drücken leitend ist. **Einen Kontakt an D19**, den anderen an **GND**. Die zwei übrigen Kontakte werden nicht verbunden. `INPUT_PULLUP`, softwareseitig 50 ms Entprellung. **Jeder Tastendruck wechselt WUCHS ↔ BLÜTE.** Auswahl im ESP32-NVS `growphase_v1`.

**4-Pin-RGB-LED:** Vier Kontakte entsprechen gemeinsamem Anschluss, Rot, Grün und Blau. Der Nutzer hat **gemeinsame Kathode** bestätigt: gemeinsamen Kontakt an GND. Rot- und Grünanschluss über die beiden Vorwiderstände; Blau bleibt frei.

- **D4 → 440 Ω (2 × 220 Ω in Reihe) → Grün**
- **D33 → 440 Ω (2 × 220 Ω in Reihe) → Rot**
- Blau bleibt unbenutzt.
- **Gemeinsame Kathode: gemeinsam an GND**, HIGH schaltet jeweilige Farbe ein.

Die Firmware ist auf **Grün = WUCHS** und **Rot + Grün = Gelb = BLÜTE** umgestellt. **Gemeinsame Kathode steht fest** (`GROW_RGB_COMMON_ANODE=0`); RGB-Ausgänge sind im jetzt aktualisierten Sketch aktiviert (`GROW_ENABLE_PHASE_RGB=1`). **Vor dem Flashen DS-DATA auf D23 und RGB-Grün vom nicht funktionierenden D32 auf D4 anschließen.**

**Zweipoliger pH-Buzzer:** Der Nutzer ordnet das Steuersignal **D16** zu. **Die Firmware hält die Buzzer-Ansteuerung mit `GROW_ENABLE_PH_BUZZER=0` deaktiviert.** Die elektrische Beschaltung, Versorgung, Stromaufnahme sowie aktiv/passiv müssen am konkreten Buzzer geprüft werden; eine erforderliche Treiberstufe darf nicht erraten werden. **D18 nicht verwenden:** am vorhandenen Board nicht herausgeführt.

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

## ESP32-C3 SuperMini — fünf Relais im separaten Leistungsgehäuse

**Endgültig als Funktionen zugeordnet: genau fünf Relais.** Die **beiden Lampen gehören gemeinsam an EINEN Lichtkanal** (nicht zwei oder drei Lichtrelais). Die untenstehenden Kanalnummern benennen Funktionen, **noch keine geprüften GPIOs, Klemmen oder elektrischen Anschlüsse**.

| Relais / Funktionskanal | Geschaltete Funktion | Steuerungsstatus |
|---|---|---|
| 1 – Licht | **Beide Pflanzenlampen gemeinsam** | Gemeinsames Ein/Aus; Leistungs- und Einschaltstrom beider Treiber vor Auswahl und Freigabe der Schaltstufe prüfen |
| 2 – Abluft | Abluftventilator | Eigenständiger Kanal |
| 3 – Umluft | Umluftventilation | Eigenständiger Kanal; konkrete Anzahl/Anordnung der Ventilatoren nicht aus der Relaisanzahl ableiten |
| 4 – Heizung | Heizstrahler | Eigenständiger Kanal; unabhängige Schutz-/Temperaturbegrenzung erforderlich |
| 5 – Pumpe | **Geplante Filter-/Umwälzpumpe** | Eigenständiger Kanal; Pumpe kommt neu hinzu, technische Daten und Anschluss noch offen |

### Zusätzlicher Pumpen-Taster für Filterreinigung

- **Zusätzlicher, separater Taster** zur Pumpe, unabhängig vom bereits vorhandenen **WUCHS/BLÜTE-Phasentaster D19**. Der Buzzer bleibt auf **D16**. Die Sensorplatine erhält dadurch keine neuen Relaisausgänge.
- **Soll-Funktion:** Betätigen des Pumpentasters versetzt den Pumpenkanal in einen deutlich erkennbaren Zustand **„Wartung / Pumpe gesperrt“**. Der Pumpenkanal muss aus bleiben, auch wenn Automatik, WLAN-Befehle oder Neustart ihn aktivieren wollen.
- **Wiedereinschalten:** Nicht automatisch nach Neustart oder Kommunikationsunterbrechung; erst nach bewusster Aufhebung der Sperre. Der konkrete Bedienablauf (erneuter Tastendruck oder getrennte Bestätigung) ist noch festzulegen.
- **Anschluss:** Als Kleinspannungs-Bedieneingang am separaten Leistungsteil/C3 geplant. **C3-GPIO, Tasterbauart und Position sind noch offen**; keine GPIO-Nummer wird ohne Prüfung zugewiesen.
- **Sicherheit:** Eine reine Software-Pausentaste oder Relaisanzeige ist **keine sichere elektrische Freischaltung** für Arbeiten am Filter oder an der Pumpe. Vor dem Reinigen muss die Energiezufuhr zur Pumpe fachgerecht und gegen unbeabsichtigten Wiederanlauf gesichert sein. Die Ausführung eines geeigneten, gegebenenfalls verriegelbaren Wartungs-AUS/Trennmittels sowie aller Netzspannungsarbeiten ist durch eine Elektrofachkraft zu planen und zu prüfen.

**Noch offen am C3:** GPIOs für fünf Relais plus Pumpen-Wartungstaster, aktive Relaispegel und Treiberkompatibilität, Größe/Typ der Pumpe, tatsächliche Lampentreiber-Last, Schutzkonzept und Rückmeldung des Pumpen-Wartungszustands.

Die zwei ESP32 sollen später über **WLAN** kommunizieren. Geplante Zustände: **angefordert**, **vom C3 bestätigt**, **Kommunikation verloren**. Die Umsetzung, sichere Fehlerzustände und Hardwaretests fehlen. Dieser Plan dokumentiert die Logik, nicht die 230-V-Verdrahtung.

## Softwarestand und nächste Schritte

- `firmware/ph_calibration_tool/ph_calibration_tool.ino`: Sensor-Hardwaretest, pH-Kalibrierung/Alarme; **keine Relais-GPIOs mehr**.
- `firmware/growroom_telemetry_client/growroom_telemetry_client.ino`: Sensor-Messwerte, WLAN, lokaler API-Endpunkt und Sollwertverwaltung; alte lokale Relais-GPIO-Schaltung **entfernt**. C3 ist noch nicht angebunden. API meldet für Aktoren `esp32_c3_not_connected`.
- `firmware/ds18b20_diagnostic/ds18b20_diagnostic.ino`: DS18B20-Prüfung auf GPIO23 (nach physischer Umverdrahtung erneut testen).
- Das separate C3-Programm, die WLAN-Anbindung, **fünf** Relaisausgänge und der **Pumpen-Wartungstaster samt Sperrlogik** sowie ihre Hardwaretests stehen noch aus.
- Sensorplatine vollständig anhand der obigen Liste auf Beschriftung/Kontinuität prüfen. **Die pH-Kalibrierung ist laut Nutzer funktionsfähig**; übrige Sensoren, RGB an D4, die fünf C3-Relais und die Pumpensperre müssen jeweils gesondert geprüft werden.

**Nicht mehr verwenden:** alter Ein-ESP32-Wiringplan, alte direkte SSR-Pinbelegung am Sensorboard und `firmware/powerbox_actuator/` als vermeintliche fertige ESP32-C3-Firmware.
