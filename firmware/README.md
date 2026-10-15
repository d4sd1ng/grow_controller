# Grow Controller — Sensor-Firmware und geplanter ESP32-C3-Leistungscontroller

## Aktive Geraete

| Sketch | Geraet | Funktion |
|---|---|---|
| `growroom_telemetry_client` | Sensor-ESP32 mit OLED, **26×30-Lochraster** | 3x DHT22, DS18B20, ADS1115 pH/EC, BH1750; **keine Relais-GPIOs**, HTTP-API, Telemetrie |
| `ph_calibration_tool` | derselbe ESP32, temporär geflasht | Hardwarediagnose aller Sensortypen und pH-7-/pH-4-Kalibrierung ueber ADS1115 A0 |
| `ds18b20_diagnostic` | derselbe ESP32, temporär geflasht | 1-Wire-Diagnose auf GPIO23 |
| `cyd_remote_control` | vorhandenes CYD | primaere Fernbedienung |
| `cyd_grow_display` | dasselbe CYD, alternative Firmware | alternative LVGL-Oberflaeche |
| `powerbox_actuator` | Legacy-Leistungscode | **nicht** als ESP32-C3-Firmware verwenden; C3-Programm und WLAN-Anbindung noch offen |

## Kleinspannungs-Pins

| GPIO | Funktion |
|---:|---|
| 21/22 | ADS1115 (0x48) + internes OLED (0x3C) + optionale MLX90640 (0x33); I²C Bus A |
| 13/14 | I²C Bus B: BH1750 (0x23) separat |
| 23 | DS18B20-Datenbus (4,7-kΩ-Pull-up nach 3,3 V) |
| ADS1115 A0 | **pH-Spannungsteiler:** PH4502C PO → **10 kΩ in Reihe** → ADS A0, **20 kΩ von A0 nach GND**. Vorhandene, kalibrierte Verdrahtung beibehalten. |
| ADS1115 A1 | CQRobot TDS/EC-Signal, **ohne den 10k/20k-pH-Spannungsteiler**; maximale Analogspannung prüfen |
| 25/26/27 | DHT22 #1/#2/#3 |
| 16 | **Buzzer-Steuersignal**, deaktiviert bis zum Abgleich von Typ und Treiber |
| 4 | **RGB-LED Grün**, 440 Ω Vorwiderstand (2×220 Ω in Reihe); ersetzt GPIO32 |
| 33 | **RGB-LED Rot**, 440 Ω Vorwiderstand (2×220 Ω in Reihe) |
| 19 | **Ein 4-Pin-Taster**, zwischen D19 und GND (je einen Kontakt aus verschiedenen Schaltpaaren) |
| RGB-Blau | Nicht angeschlossen |
| 18 | **Nicht herausgeführt** am vom Nutzer abgelesenen ESP32-Board |

**I²C-Initialisierung:** ADS1115 fest auf **D21/D22, 0x48**, gemeinsam mit dem OLED. BH1750 fest auf **D13/D14**, Adresse **0x23** oder **0x5C**. **Keine automatische Umzuordnung.** Bei fehlendem ADS1115 und auf seriellen Befehl **`i`** werden beide Busse nur zur Diagnose nach Adressen durchsucht. Die Produktions-Firmware versucht einen fehlenden ADS1115 alle 30 Sekunden erneut zu initialisieren; die Kalibrierwerte bleiben erhalten.

**Nicht verdrahten:** PH4502C TO und DO. Die eigentliche Leistungsschaltung liegt auf dem separaten ESP32-C3 SuperMini, nicht auf der Sensorplatine. Dort sind **fünf Relais** geplant: ein gemeinsamer Lichtkanal für die **zwei Lampen**, Abluft, Umluft, Heizstrahler und die neu geplante **Filter-/Umwälzpumpe**. Ein **zweiter, nicht rastender Pumpen-Drucktaster am separaten ESP32-C3 IO6/GPIO6 gegen C3-GND** ist zusätzlich zum Sensor-Phasentaster D19 vorgesehen (interner `INPUT_PULLUP`, kein externer Widerstand; C3-Firmware noch nicht umgesetzt): **erster Druck = Pumpe AUS/Wartungssperre; zweiter Druck = Pumpe EIN/normaler Betrieb**. Die Umschaltung ist elektronisch per entprellter Tastflanke vorgesehen, keine mechanische Rastfunktion. Im Wartungszustand darf keine Automatik die Pumpe einschalten. Der C3-GPIO und die sichere Wartungstrennung sind noch offen. C3-Firmware und Relaissteuerung sind bislang nicht implementiert.

### Bedienfeld: ein 4-Pin-Taster, eine 4-Pin-RGB-LED, ein 2-Pin-Buzzer

- **Taster 1 (WUCHS/BLÜTE)** zwischen GPIO19 und GND (`INPUT_PULLUP`, kein externer Widerstand); bei einem vierbeinigen Taster dürfen die zwei gewählten Kontakte **ohne Tastendruck nicht verbunden** sein. Die Firmware erfasst FALLING-Flanken per Interrupt mit 50 ms Entprellung, sodass kurze Drücke auch während blockierender ADS-Messungen nicht verloren gehen. Serial-Monitor (115200 Baud): Beim Start HIGH/LOW, nach Druck `TASTER D19 ...` und `PHASE ...`; Kalibriersketch `t` zeigt momentanen GPIO19-Pegel. Auswahl bleibt in NVS (`growphase_v1`) gespeichert.
- **RGB-LED:** Grün über 440 Ω (2×220 Ω in Reihe) an GPIO4 (D4) statt GPIO32, Rot über eigene 440 Ω (2×220 Ω in Reihe) an GPIO33, Blau frei. **Gemeinsame Kathode an GND** (vom Nutzer bestätigt), HIGH schaltet Farbe ein. **Grün = WUCHS**, **Rot+Grün = Gelb = BLÜTE**. Firmware ist mit `GROW_ENABLE_PHASE_RGB=1` und `GROW_RGB_COMMON_ANODE=0` eingestellt. **Vor dem Flashen DATA des DS18B20 auf D23 und Grün der LED vom ausgefallenen D32 auf D4 umverdrahten.**
- **Zweipoliger Buzzer aus Sensor-Kit:** **GPIO16** als Steuersignal, Platz für **zweipoligen Buzzer-Stecker und Transistortreiber** reservieren. Keine Annahme zur Spannung oder aktiv/passiv aus der Anschlusszahl ableiten, daher **noch nicht direkt an GPIO anschließen**. Das bislang für aktive Buzzer vorgesehene Alarmsignal ist deaktiviert (`GROW_ENABLE_PH_BUZZER=0`) und braucht ggf. Anpassung auf PWM bei passivem Piezo.
- Im Produktionssketch stehen unter `control.phase` sowie `phase` in Telemetrie der gespeicherte Zustand; **BLÜTE** hat bislang kein definiertes Klimasollwertprofil, also bleibt die automatische Klimaregelung dafür gesperrt. Die wochenabhängigen pH-/EC-Sollwerte sind unabhängig vom Taster; die pH-Alarmgrenzen bleiben 5,5–6,2.

**Vollständiger Anschluss-/Bestückungsplan:** [../wiring_plan_v2.md](../wiring_plan_v2.md).

### 4×4-Matrix-Tastatur (nur spätere Option; Befestigung fehlt)

Die vorhandene **16-Tasten-Matrix** bleibt vorerst nur als spätere Bedienoption für sämtliche Funktionen vorgemerkt, nicht nur Relais. **Noch keine Befestigungsmöglichkeit: keine Montage, GPIO-Belegung, PCF8574-Verdrahtung oder Tastatur-Firmware jetzt.** Entwurf: **PCF8574 am separaten ESP32-C3**, CYD als Menü-/Statusanzeige, Sensor-ESP32 weiterhin zuständig für Messung und Kalibrierung. Tasten **1–5 Aktoren; 6 Klima; 7 pH; 8 EC; 9 Wasser; 0 Phase/Woche; A Pumpen-Wartungsmodus AUS/EIN; B Automatik/Manuell; C Status; D Einstellungen; * Zurück; # Bestätigen**. Der vorhandene Phasentaster GPIO19 bleibt unverändert. Der separate Pumpen-Drucktaster **bleibt Bestandteil der aktuellen Planung**; die A-Taste ist allenfalls eine spätere Zusatzbedienung. **PCF8574-Modul, Matrixkontakt-Reihenfolge, C3-GPIOs und zugehörige Firmware sind noch nicht verifiziert/implementiert.** Der Taster bietet keine sichere elektrische Freischaltung für Wartungsarbeiten.

## Kalibrier-/Diagnosesketch

Arduino Serial Monitor **115200 Baud**:
- `t`: vollstaendige Hardwarediagnose
- `p`: automatischer pH-Alarm-Selbsttest (PASS/FAIL) im Kalibriersketch
- `1`: pH **5,40** simulieren -> Alarm **LOW** (<5,5)
- `2`: pH **6,30** simulieren -> Alarm **HIGH** (>6,2)
- `3`: pH **6,00** simulieren -> **NORMAL**
- `0`: Testmodus beenden, echte pH-Messung und Alarmanzeige fortsetzen
- Die Befehle `0`–`3` funktionieren **im Kalibrier- und im Produktionssketch** ueber den seriellen Monitor mit 115200 Baud. Testwerte bleiben von den realen pH-/ADS-Messwerten, der NVS-Kalibrierung und den SSR-Ausgaengen getrennt. Auf dem OLED steht `TEST`, solange ein Wert simuliert wird. Die Produktions-Firmware uebermittelt simulierte Zustandswechsel mit dem Prefix `[TEST, SIMULIERT]` als *Info*-Event an `/api/v1/alerts` und kennzeichnet Testmodus und Teststatus in den JSON-Daten separat (`ph_alarm_test`, `ph_alarm_test_status`, `ph_alarm_test_value`). Der reale Alarm wird weiterhin gesondert geprueft und uebermittelt.
- `7`: stabilen pH-7-Pufferpunkt ueber ADS1115 A0 speichern
- `4`: stabilen pH-4-Pufferpunkt ueber ADS1115 A0 speichern
- `s`: pH-Kalibrierpunkte und Konstanten fuer die produktive Firmware anzeigen
- `r`: nur die neue ADS-Kalibrierung im NVS (`phads_v1`) loeschen
- `e`: 1413-uS/cm-EC-Referenz (1,413 mS/cm bei 25 Grad C) am ADS1115 A1 erfassen; bei zu unruhigem Signal keine Speicherung
- `w <ROM>`: Wasser-DS18B20 auf einem Bus mit mehreren Sensoren eindeutig per 16-stelliger 1-Wire-ROM waehlen und im NVS `waterds_v1` speichern
- `a <C>`, `b <C>`: zwei bekannte Gehaeusetemperaturen zur TO-Kalibrierung erfassen (je mindestens 5 °C Abstand); nur nach Freigabe von A2
- `h`: Hilfe

ADS1115 A0=PO pH, A1=TDS/EC. PH4502C TO und DO sind dauerhaft nicht vorgesehen (`PH_TO_WIRED=false`, `PH_DO_WIRED=false`). Das OLED, der ADS1115 und die geplante MLX90640-Kamera liegen auf I²C **GPIO21/22**; der BH1750 liegt separat auf **GPIO13/14**. Das alte Kalibrierschema ueber VP bleibt in einem **getrennten NVS-Namespace** unberuehrt; die alte VP-Kalibrierung wird nicht unbemerkt fuer ADS-Werte verwendet. pH-Kalibrierpunkte werden nur mit einer gueltigen, aktuell gemessenen Wassertemperatur vom DS18B20 auf D23 gespeichert. Die Kalibriertemperatur wird fuer pH 7 und pH 4 jeweils erneut eingelesen und getrennt im NVS gesichert. Die fruehere feste 21,4-C-Temperatur ist aus dem Kalibriersketch entfernt. Bei fehlendem Temperaturmesswert bleibt die bisherige Kalibrierung erhalten. Bei fehlendem ADS1115, fehlendem DS18B20-Wert oder unruhiger Messung wird die Erfassung abgelehnt.

Die drei DHT22 und der BH1750 sind bisher nicht verkabelt. Sie duerfen in der Diagnose als nicht erkannt angezeigt werden, ohne die anderen Tests zu blockieren. Der Sensor-Diagnosesketch konfiguriert keine Relais-Ausgaenge mehr.

## MLX90640 - optionale Blatt-Temperatur / VPD (noch nicht montiert)

In `growroom_telemetry_client.ino` **und** `ph_calibration_tool.ino` ist die 32x24-Waermebildkamera MLX90640 vorbereitet. Auslieferungszustand: `#define GROW_ENABLE_MLX90640 0`. Der Code laeuft daher ohne Kamera und ohne zusaetzliche Bibliothek weiter. pH/EC/NVS und bestehende Regelung bleiben unveraendert.

Nach Eintreffen und Pruefung des konkreten Moduls:
1. Auf der **26×30-Lochrasterplatine** einen 4-poligen Kamerastecker vorsehen: **SDA D21, SCL D22, GND, VIN/VCC**. I2C-Adresse **0x33**, OLED am selben Bus **0x3C**. Die Versorgung **erst nach Prüfung der konkreten Kameraplatine** festlegen; bis dahin VCC/VIN nicht anschließen. Der ADS1115 (0x48) liegt mit OLED und Kamera auf **D21/D22**; der BH1750 (0x23) bleibt auf dem getrennten Bus **D13/D14**. Für BH1750 einen **5-poligen** Anschluss inkl. **ADDR an GND** reservieren.
2. Wärmebildkamera außerhalb des Gehäuses mit freiem Blick auf das Pflanzenblatt montieren; gewöhnliches Glas/Acryl vor der Linse vermeiden. I2C-Kabel möglichst kurz halten und Bus-Takt bei längeren Leitungen prüfen. In der Arduino-Bibliotheksverwaltung `Adafruit MLX90640` samt Abhaengigkeiten installieren.
3. `GROW_ENABLE_MLX90640` auf `1` setzen, uebersetzen und Sensor erkennen lassen.
4. Alle 10 Sekunden ein 32x24-Temperaturbild auslesen. Der Median der zentralen ROI (x=8..23, y=6..17, maximal 192 Pixel) wird als `thermal_roi_temperature` ausgegeben. Die Kamera erkennt **nicht automatisch**, ob die Bildregion wirklich nur Blaetter enthaelt.
5. Mit mindestens zwei gueltigen DHT22-Messungen wird aus ROI-Temperatur und Luft-Partialdampfdruck der vorlaeufige Blatt-VPD `vpd_leaf_candidate` berechnet. Die drei DHT22 werden dafuer als drei Dampfdruckwerte ausgewertet und daraus der Median gebildet (bei zwei Sensoren der Mittelwert).
6. In lokaler Zusammenfassung und Server-Telemetrie: `mlx90640_ok`, `thermal_roi_temperature`, `vpd_leaf_candidate`. Die bisherige `vpd`-Angabe bleibt **Luft-VPD**.
7. **Keine automatische Aktorregelung** auf Basis dieser Werte, solange Kameraposition, Bildausschnitt/Blattabdeckung, Messgenauigkeit und Sensorreaktion nicht am montierten Aufbau verifiziert wurden. Die bestehenden Temperatur-/RH-Regeln werden dadurch nicht geaendert.

Bei Bildlesefehlern werden die Kamera- und Blatt-VPD-Werte verworfen (`null`). Die Diagnose zeigt MLX-Status im Befehl `t` und bei vorhandenem Modul alle zehn Sekunden im seriellen Monitor. Kamera-Bildrohdaten werden derzeit **nicht** an den Pi uebertragen; nur die abgeleiteten Messwerte. Die alte Kalibrierung wird weder geloescht noch erneut benoetigt.

## Nährstoff-Sollwerte nach Woche (pH / EC)

**EC = elektrische Leitfähigkeit in mS/cm**, nicht TDS in ppm. Die Benutzer-Sollwerte sind:

| Woche | pH-Ziel | EC-Ziel (mS/cm) |
|---|---:|---:|
| 1 | 6,0 | 1,6 |
| 2 | 5,8 | 1,8 |
| 3–7 | 5,5 | 2,0 |
| 8 | 5,5 | 2,5 |
| 9–10 | 5,5 | 1,8 |
| 11–12 | 5,5 | 0,2 |

**Feste pH-Alarme:** bei **pH <5,5** LOW bzw. **pH >6,2** HIGH. Die Werte **5,5 und 6,2** sind inklusive innerhalb der erlaubten Spanne; bestehende **0,1-pH-Hysterese** und Bestätigung nach **3 Messungen** bleiben bestehen. Das neue HIGH-Testsignal ist **pH 6,30**; `p` im Diagnosesketch prüft die neuen Grenzen inklusive 5,50/6,20. Alle Anzeigen (OLED und beide CYD-Sketches) sind aktualisiert.

Diese Wochenwerte sind **Sollwerte, keine EC-Alarmgrenzen**; eine zulässige Abweichung für EC wurde noch nicht definiert. Es gibt **keine automatische Dosierung oder EC-Regelung**, nur Messung, Anzeige und dokumentierten Sollwert. Die unveränderte EC-Messkette liefert weiterhin einen temperaturkompensierten Wert erst nach DS18B20-Prüfung und Kalibrierung.

Die aktuelle **Woche ist nicht automatisch 1**, da ein Startdatum fehlt. Es wird kein Datum angenommen und nicht täglich/woechentlich selbstständig weitergezählt. Vor der ersten Auswahl gilt Woche `0` (unbekannt). Manuelle Wahl ist über **Serial** `k1` bis `k12` (mit Zeilenumbruch) in beiden Sensor-Sketches möglich oder in der Produktions-Firmware per **POST** `/api/local/control/week` mit JSON `{"week":3}` (HTTP-Header `X-Telemetry-Key` falls konfiguriert). Die Woche wird in `growweek_v1` auf dem Sensor-ESP32 gespeichert und beim Neustart wieder geladen. Die Phasentaster `WUCHS/BLÜTE` bleiben eine **separate** Einstellung.

Die API `/api/local/summary` liefert `control.grow_week`, `control.ph_target`, `control.ec_target_ms_cm`, `control.ph_alarm_min` und `control.ph_alarm_max`; die entsprechenden Felder stehen ebenfalls unter `reading` und in der Telemetrie. Ohne gewählte Woche sind die Sollwerte `null`, die festen pH-Alarme funktionieren dennoch.

## TDS/EC-Kalibrierung
Der CQRobot-TDS-Sensor liegt am ADS1115 A1. Sein EC/TDS-Wert wird fuer die Bezugstemperatur 25 °C mit der **tatsaechlich gemessenen Wassertemperatur des DS18B20** kompensiert (Hersteller-Näherung 2 %/°C). Fehlt diese Messung, bleibt `ec=null`, es gibt keinen Ersatzwert von 25 °C. Die bisherige EC-Kalibrierung im NVS bleibt beim Umlegen der I2C-Datenleitung gespeichert, Messwerte danach aber prüfen. Die bisherige EC-Kalibrierung bleibt gespeichert; wurde sie ohne Wasser-DS erstellt, wird `ec_reference_temp_verified=false` gemeldet und eine erneute Kalibrierung mit DS empfohlen. Der Kalibriersketch erfasst mit Befehl `e` die Referenzloesung mit 1413 uS/cm (1,413 mS/cm bei 25 C), prueft die Signalstreuung und schreibt einen Kalibrierfaktor nach NVS `ecads_v1`. Die Kalibrierung prueft acht unabhaengige Median-Messfenster (je 30 ADS-A1-Werte, Abtastperiode 40 ms gemaess CQRobot-Dokumentation), die denselben Filter wie die Live-Anzeige nutzen. Liegt die Standardabweichung dieser gefilterten Fenster ueber 10 mV oder die Spanne ueber 30 mV, wird KEINE Kalibrierung gespeichert. Grosse Schwankungen der Rohmesswerte werden gesondert protokolliert und als Hinweis auf moegliche elektrische Stoerungen ausgegeben. Die Produktions-Firmware verwendet einen nicht blockierenden 30-Werte-Ringpuffer mit 40 ms Abtastperiode (Median), liest `ecads_v1` beim Start, zeigt EC in mS/cm auf OLED und CYD und liefert `ec` in Telemetrie und lokaler API. Ohne gespeicherte Kalibrierung bleibt `ec=null`, statt einen scheinbar genauen Wert zu melden. Bei einem einzelnen DS18B20 wird dessen Temperatur verwendet. Bei zwei oder mehr Sensoren ist eine eindeutige ROM-Zuordnung ueber `w <ROM>` noetig; die Index-Reihenfolge wird nicht als Wasser-Zuordnung angenommen.

## pH-Ueberwachung

- Unter pH 5,5: Alarm `low`; ueber pH 6,2: Alarm `high`; 5,5–6,2: `normal` (mit bestehender Hysterese bei Wechseln).
- Fehlender ADS1115, unplausibler pH oder fehlende echte pH-4/pH-7-Kalibrierung: `sensor_fault` statt erfundenem Messwert.
- Drei aufeinanderfolgende Abfragen zur Bestaetigung von Alarmzustaenden, 0,1 pH Hysterese.
- OLED, beide CYD-Anzeigen, lokale Zusammenfassung (`reading.ph_alarm`) und Server-Telemetrie zeigen den Status; Statuswechsel werden an `/api/v1/alerts` gesendet.
- Der Kalibriersketch speichert pH7/pH4 **und die jeweils gemessene DS18B20-Wassertemperatur** in `phads_v1`; die produktive Firmware liest beides. Der pH-Wert erhaelt eine Steilheitskorrektur nach Nernst mit der aktuell gemessenen DS18B20-Wassertemperatur, wenn beide Kalibriertemperaturen gueltig und nahe beieinander sind. Ansonsten wird der pH-Wert unkompensiert angezeigt (`ph_temperature_compensated=false`), keine 25-Grad-Substitution. Bei alter pH-Kalibrierung ohne DS oder mit damals fest eingetragenen 21,4 C: beide Pufferpunkte mit DS18B20 erneut erfassen. Die vorhandenen Punkte werden dabei einzeln ueberschrieben, nicht automatisch geloescht.
- TO und DO der PH4502C werden **nicht** gebraucht. Wassertemperatur ausschliesslich ueber den einen DS18B20 auf GPIO23 (zuvor auf GPIO32 erfolgreich getestet). `PH_DO_WIRED=false` und `PH_TO_WIRED=false` bleiben gesetzt. pH-Alarm beruht nur auf dem kalibrierten PO-Signal.

## Lokale HTTP-API, Port 8081

| Methode | Pfad | Zweck |
|---|---|---|
| GET | `/api/local/summary` | Livewerte, Modus, Sollwerte und lokale Ausgangszustaende |
| GET | `/api/local/actuators` | API-Kompatibilitaet: Soll-/Ist-GPIO-Zustaende |
| POST | `/api/local/control/targets` | Gibt **HTTP 503 `esp32_c3_not_connected`** zurueck, solange der separate C3-Leistungscontroller fehlt |
| POST | `/api/local/control/mode` | `auto`/`manual` |

Die **Trennung ist in der Sensor-Firmware umgesetzt:** Alle direkten SSR-Pins und GPIO-Schaltbefehle wurden entfernt. Der Sensorcontroller liest Sensoren und verwaltet Sollwerte, meldet Aktoren aber als **`esp32_c3_not_connected`**. **Noch nicht umgesetzt:** eigenes C3-Programm, dessen Kommunikation/Rueckmeldung und sichere Ausfallbehandlung. Es gibt keine betriebsfaehige Leistungssteuerung. Fuer eine Heizlast ist eine unabhaengige physische Temperaturbegrenzung erforderlich.

## Benoetigte Arduino-Bibliotheken

Adafruit ADS1X15, Adafruit GFX, Adafruit SSD1306, DHT sensor library, OneWire, DallasTemperature, ArduinoJson und ESP32 Arduino Core. **Optional bei aktivierter Waermebildkamera:** Adafruit MLX90640 (mit Abhaengigkeiten). Fuer das CYD zudem die Bibliotheken der gewaehlten CYD-Variante.

## Konfiguration und Pruefung

Im Projektroot `python export_firmware_secrets_from_env.py` ausfuehren, um `device_secrets.h` fuer den Sensor-ESP32-Sketch und CYD zu erzeugen (Dateien sind git-ignoriert). Keine WLAN-Passwoerter in Git committen.

Vor dem Flashen muss die geaenderte Sensor-Firmware mit der lokal installierten Arduino-ESP32-Toolchain kompiliert werden. Danach die Kleinspannungs-Sensorik auf der 26×30-Platine testen. Separate ESP32-C3-Firmware ist noch zu entwickeln; Aktoren bleiben ohne Verbindung. Die SSR-Lastseite bleibt bis zur fachgerechten Pruefung ausser Betrieb.
