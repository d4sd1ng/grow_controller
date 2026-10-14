# Grow Controller — Sensor-Firmware und geplanter ESP32-C3-Leistungscontroller

## Aktive Geraete

| Sketch | Geraet | Funktion |
|---|---|---|
| `growroom_telemetry_client` | Sensor-ESP32 mit OLED, **26×30-Lochraster** | 3x DHT22, DS18B20, ADS1115 pH/EC, BH1750; **keine Relais-GPIOs**, HTTP-API, Telemetrie |
| `ph_calibration_tool` | derselbe ESP32, temporär geflasht | Hardwarediagnose aller Sensortypen und pH-7-/pH-4-Kalibrierung ueber ADS1115 A0 |
| `ds18b20_diagnostic` | derselbe ESP32, temporär geflasht | 1-Wire-Diagnose auf GPIO32 |
| `cyd_remote_control` | vorhandenes CYD | primaere Fernbedienung |
| `cyd_grow_display` | dasselbe CYD, alternative Firmware | alternative LVGL-Oberflaeche |
| `powerbox_actuator` | Legacy-Leistungscode | **nicht** als ESP32-C3-Firmware verwenden; C3-Programm und WLAN-Anbindung noch offen |

## Kleinspannungs-Pins

| GPIO | Funktion |
|---:|---|
| 21/22 | Internes OLED und **MLX90640-Kamera** (I2C-Bus A: 0x3C / 0x33) |
| 13/14 | ADS1115 und BH1750 gemeinsam (I2C-Bus B: 0x48 / 0x23) |
| 32 | DS18B20-Datenbus (4,7 kOhm Pull-up nach 3,3 V) |
| ADS1115 A0 | PH4502C PO (vorhandene kalibrierte Analog-Messkette) |
| ADS1115 A1 | CQRobot TDS/EC (vorhandene Analog-Messkette) |
| 25/26/27 | DHT22 #1/#2/#3 |
| 33 | Reserve: keine DO- oder Relaisfunktion |

**Nicht bestuecken:** PH4502C TO/DO; TO hat am vorhandenen Exemplar 3,3–3,4 V geliefert und ist nicht direkt ADS-A2-tauglich. **Keine Relaisleitungen auf dem Sensor-ESP32.** Separater ESP32-C3 SuperMini im Leistungsgehäuse.

Vollstaendig: [../wiring_plan_v2.md](../wiring_plan_v2.md).

## Kalibrier-/Diagnosesketch

Arduino Serial Monitor **115200 Baud**:
- `t`: vollstaendige Hardwarediagnose
- `p`: automatischer pH-Alarm-Selbsttest (PASS/FAIL) im Kalibriersketch
- `1`: pH **5,40** simulieren -> Alarm **LOW** (<5,5)
- `2`: pH **6,60** simulieren -> Alarm **HIGH** (>6,5)
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

ADS1115 A0=PO pH, A1=TDS/EC. PH4502C TO und DO sind dauerhaft nicht vorgesehen (`PH_TO_WIRED=false`, `PH_DO_WIRED=false`). Das OLED liegt mit der geplanten MLX90640-Kamera auf I2C **GPIO21/22**; ADS1115 und BH1750 auf dem zweiten Bus **GPIO13/14**. Das alte Kalibrierschema ueber VP bleibt in einem **getrennten NVS-Namespace** unberuehrt; die alte VP-Kalibrierung wird nicht unbemerkt fuer ADS-Werte verwendet. Kalibrierpunkte koennen auch ohne DS18B20 gespeichert werden; die fehlende Temperatur wird als unbekannt behandelt und der Pufferwert ist anhand seiner Referenztemperatur zu bewerten. Bei fehlendem ADS1115 oder unruhiger Messung wird die Erfassung abgelehnt.

Die drei DHT22 und der BH1750 sind bisher nicht verkabelt. Sie duerfen in der Diagnose als nicht erkannt angezeigt werden, ohne die anderen Tests zu blockieren. Der Sensor-Diagnosesketch konfiguriert keine Relais-Ausgaenge mehr.

## MLX90640 - optionale Blatt-Temperatur / VPD (noch nicht montiert)

In `growroom_telemetry_client.ino` **und** `ph_calibration_tool.ino` ist die 32x24-Waermebildkamera MLX90640 vorbereitet. Auslieferungszustand: `#define GROW_ENABLE_MLX90640 0`. Der Code laeuft daher ohne Kamera und ohne zusaetzliche Bibliothek weiter. pH/EC/NVS und bestehende Regelung bleiben unveraendert.

Nach Eintreffen und Pruefung des konkreten Moduls:
1. Auf der **26×30-Lochrasterplatine** einen 4-poligen Kamerastecker vorsehen: **SDA D21, SCL D22, GND, VIN/VCC**. I2C-Adresse **0x33**, OLED am selben Bus **0x3C**. Die Versorgung **erst nach Prüfung der konkreten Kameraplatine** festlegen; bis dahin VCC/VIN nicht anschließen. ADS1115 (0x48) und BH1750 (0x23) teilen jetzt den zweiten Bus **D13/D14**. Für BH1750 einen **5-poligen** Anschluss inkl. **ADDR an GND** reservieren.
2. Wärmebildkamera außerhalb des Gehäuses mit freiem Blick auf das Pflanzenblatt montieren; gewöhnliches Glas/Acryl vor der Linse vermeiden. I2C-Kabel möglichst kurz halten und Bus-Takt bei längeren Leitungen prüfen. In der Arduino-Bibliotheksverwaltung `Adafruit MLX90640` samt Abhaengigkeiten installieren.
3. `GROW_ENABLE_MLX90640` auf `1` setzen, uebersetzen und Sensor erkennen lassen.
4. Alle 10 Sekunden ein 32x24-Temperaturbild auslesen. Der Median der zentralen ROI (x=8..23, y=6..17, maximal 192 Pixel) wird als `thermal_roi_temperature` ausgegeben. Die Kamera erkennt **nicht automatisch**, ob die Bildregion wirklich nur Blaetter enthaelt.
5. Mit mindestens zwei gueltigen DHT22-Messungen wird aus ROI-Temperatur und Luft-Partialdampfdruck der vorlaeufige Blatt-VPD `vpd_leaf_candidate` berechnet. Die drei DHT22 werden dafuer als drei Dampfdruckwerte ausgewertet und daraus der Median gebildet (bei zwei Sensoren der Mittelwert).
6. In lokaler Zusammenfassung und Server-Telemetrie: `mlx90640_ok`, `thermal_roi_temperature`, `vpd_leaf_candidate`. Die bisherige `vpd`-Angabe bleibt **Luft-VPD**.
7. **Keine automatische Aktorregelung** auf Basis dieser Werte, solange Kameraposition, Bildausschnitt/Blattabdeckung, Messgenauigkeit und Sensorreaktion nicht am montierten Aufbau verifiziert wurden. Die bestehenden Temperatur-/RH-Regeln werden dadurch nicht geaendert.

Bei Bildlesefehlern werden die Kamera- und Blatt-VPD-Werte verworfen (`null`). Die Diagnose zeigt MLX-Status im Befehl `t` und bei vorhandenem Modul alle zehn Sekunden im seriellen Monitor. Kamera-Bildrohdaten werden derzeit **nicht** an den Pi uebertragen; nur die abgeleiteten Messwerte. Die alte Kalibrierung wird weder geloescht noch erneut benoetigt.

## TDS/EC-Kalibrierung
Der CQRobot-TDS-Sensor liegt am ADS1115 A1. Sein EC/TDS-Wert wird fuer die Bezugstemperatur 25 °C mit der **tatsaechlich gemessenen Wassertemperatur des DS18B20** kompensiert (Hersteller-Näherung 2 %/°C). Fehlt diese Messung, bleibt `ec=null`, es gibt keinen Ersatzwert von 25 °C. Die bisherige EC-Kalibrierung im NVS bleibt beim Umlegen der I2C-Datenleitung gespeichert, Messwerte danach aber prüfen. Die bisherige EC-Kalibrierung bleibt gespeichert; wurde sie ohne Wasser-DS erstellt, wird `ec_reference_temp_verified=false` gemeldet und eine erneute Kalibrierung mit DS empfohlen. Der Kalibriersketch erfasst mit Befehl `e` die Referenzloesung mit 1413 uS/cm (1,413 mS/cm bei 25 C), prueft die Signalstreuung und schreibt einen Kalibrierfaktor nach NVS `ecads_v1`. Die Kalibrierung prueft acht unabhaengige Median-Messfenster (je 30 ADS-A1-Werte, Abtastperiode 40 ms gemaess CQRobot-Dokumentation), die denselben Filter wie die Live-Anzeige nutzen. Liegt die Standardabweichung dieser gefilterten Fenster ueber 10 mV oder die Spanne ueber 30 mV, wird KEINE Kalibrierung gespeichert. Grosse Schwankungen der Rohmesswerte werden gesondert protokolliert und als Hinweis auf moegliche elektrische Stoerungen ausgegeben. Die Produktions-Firmware verwendet einen nicht blockierenden 30-Werte-Ringpuffer mit 40 ms Abtastperiode (Median), liest `ecads_v1` beim Start, zeigt EC in mS/cm auf OLED und CYD und liefert `ec` in Telemetrie und lokaler API. Ohne gespeicherte Kalibrierung bleibt `ec=null`, statt einen scheinbar genauen Wert zu melden. Bei einem einzelnen DS18B20 wird dessen Temperatur verwendet. Bei zwei oder mehr Sensoren ist eine eindeutige ROM-Zuordnung ueber `w <ROM>` noetig; die Index-Reihenfolge wird nicht als Wasser-Zuordnung angenommen.

## pH-Ueberwachung

- Unter pH 5,5: Alarm `low`; ueber pH 6,5: Alarm `high`; 5,5–6,5: `normal`.
- Fehlender ADS1115, unplausibler pH oder fehlende echte pH-4/pH-7-Kalibrierung: `sensor_fault` statt erfundenem Messwert.
- Drei aufeinanderfolgende Abfragen zur Bestaetigung von Alarmzustaenden, 0,1 pH Hysterese.
- OLED, beide CYD-Anzeigen, lokale Zusammenfassung (`reading.ph_alarm`) und Server-Telemetrie zeigen den Status; Statuswechsel werden an `/api/v1/alerts` gesendet.
- Der Kalibriersketch speichert pH7/pH4 **und die jeweils gemessene DS18B20-Wassertemperatur** in `phads_v1`; die produktive Firmware liest beides. Der pH-Wert erhaelt eine Steilheitskorrektur nach Nernst mit der aktuell gemessenen DS18B20-Wassertemperatur, wenn beide Kalibriertemperaturen gueltig und nahe beieinander sind. Ansonsten wird der pH-Wert unkompensiert angezeigt (`ph_temperature_compensated=false`), keine 25-Grad-Substitution. Bei alter pH-Kalibrierung ohne DS: beide Pufferpunkte nach Anschluss des Wasser-DS erneut erfassen.
- TO und DO der PH4502C werden **nicht** gebraucht. Wassertemperatur ausschliesslich ueber DS18B20, vorerst noch nicht lieferbar. `PH_DO_WIRED=false` und `PH_TO_WIRED=false` bleiben gesetzt. pH-Alarm beruht nur auf dem kalibrierten PO-Signal.

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
