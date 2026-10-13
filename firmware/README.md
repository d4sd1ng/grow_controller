# Grow Controller — Sensor-Firmware und geplanter ESP32-C3-Leistungscontroller

## Aktive Geraete

| Sketch | Geraet | Funktion |
|---|---|---|
| `growroom_telemetry_client` | Sensor-ESP32 mit OLED (bestehender Code, SSR-Teil noch alt) | 3x DHT22, 2x DS18B20, ADS1115 pH/TDS, BH1750, vier lokale SSR-Steuerausgaenge, AUTO/MANUAL, CYD-API, Telemetrie |
| `ph_calibration_tool` | derselbe ESP32, temporär geflasht | Hardwarediagnose aller Sensortypen und pH-7-/pH-4-Kalibrierung ueber ADS1115 A0 |
| `ds18b20_diagnostic` | derselbe ESP32, temporär geflasht | 1-Wire-Diagnose auf GPIO23 |
| `cyd_remote_control` | vorhandenes CYD | primaere Fernbedienung |
| `cyd_grow_display` | dasselbe CYD, alternative Firmware | alternative LVGL-Oberflaeche |
| `powerbox_actuator` | alter Leistungs-ESP32-Code | bisher nicht auf neuen ESP32-C3 SuperMini portiert; Zwei-Controller-Umstellung noch offen |

## Kleinspannungs-Pins

| GPIO | Funktion |
|---:|---|
| 21/22 | OLED und ADS1115 |
| 23 | DS18B20-Datenbus |
| 32 | PH4502C DO (ein Schwellwert, erst nach Pegelpruefung aktivieren) |
| ADS1115 A2 | PH4502C TO (analoge Rohspannung, erst nach Pegelpruefung aktivieren) |
| 25/26/27 | DHT22 #1/#2/#3 |
| 13/14 | BH1750 separater I2C-Bus |
| 17 | SSR1 Licht |
| 18 | SSR2 Umluft |
| 19 | SSR3 Abluft |
| 33 | SSR4 Heizung |

Vollstaendig: [../wiring_plan_v2.md](../wiring_plan_v2.md).

## Kalibrier-/Diagnosesketch

Arduino Serial Monitor **115200 Baud**:
- `t`: vollstaendige Hardwarediagnose
- `7`: stabilen pH-7-Pufferpunkt ueber ADS1115 A0 speichern
- `4`: stabilen pH-4-Pufferpunkt ueber ADS1115 A0 speichern
- `s`: pH-Kalibrierpunkte und Konstanten fuer die produktive Firmware anzeigen
- `r`: nur die neue ADS-Kalibrierung im NVS (`phads_v1`) loeschen
- `e`: 1413-uS/cm-EC-Referenz (1,413 mS/cm bei 25 Grad C) am ADS1115 A1 erfassen; bei zu unruhigem Signal keine Speicherung
- `w <ROM>`: Wasser-DS18B20 auf einem Bus mit mehreren Sensoren eindeutig per 16-stelliger 1-Wire-ROM waehlen und im NVS `waterds_v1` speichern
- `a <C>`, `b <C>`: zwei bekannte Gehaeusetemperaturen zur TO-Kalibrierung erfassen (je mindestens 5 °C Abstand); nur nach Freigabe von A2
- `h`: Hilfe

ADS1115 A0=PO pH, A1=TDS/EC, A2=TO (derzeit deaktiviert); PH4502C DO vorgesehen an GPIO32 (ebenfalls deaktiviert). OLED und ADS1115 auf I2C GPIO21/22. Das alte Kalibrierschema ueber VP bleibt in einem **getrennten NVS-Namespace** unberuehrt; die alte VP-Kalibrierung wird nicht unbemerkt fuer ADS-Werte verwendet. Kalibrierpunkte koennen auch ohne DS18B20 gespeichert werden; die fehlende Temperatur wird als unbekannt behandelt und der Pufferwert ist anhand seiner Referenztemperatur zu bewerten. Bei fehlendem ADS1115 oder unruhiger Messung wird die Erfassung abgelehnt.

Die drei DHT22 und der BH1750 sind bisher nicht verkabelt. Sie duerfen in der Diagnose als nicht erkannt angezeigt werden, ohne die anderen Tests zu blockieren. SSR-Ausgaenge bleiben im Diagnosesketch immer LOW.

## TDS/EC-Kalibrierung
Der CQRobot-TDS-Sensor liegt am ADS1115 A1. Sein EC/TDS-Wert wird fuer die Bezugstemperatur 25 °C mit der **tatsaechlich gemessenen Wassertemperatur des DS18B20** kompensiert (Hersteller-Näherung 2 %/°C). Fehlt diese Messung, bleibt `ec=null`, es gibt keinen Ersatzwert von 25 °C. Die bisherige EC-Kalibrierung bleibt gespeichert; wurde sie ohne Wasser-DS erstellt, wird `ec_reference_temp_verified=false` gemeldet und eine erneute Kalibrierung mit DS empfohlen. Der Kalibriersketch erfasst mit Befehl `e` die Referenzloesung mit 1413 uS/cm (1,413 mS/cm bei 25 C), prueft die Signalstreuung und schreibt einen Kalibrierfaktor nach NVS `ecads_v1`. Die Kalibrierung prueft acht unabhaengige Median-Messfenster (je 30 ADS-A1-Werte, Abtastperiode 40 ms gemaess CQRobot-Dokumentation), die denselben Filter wie die Live-Anzeige nutzen. Liegt die Standardabweichung dieser gefilterten Fenster ueber 10 mV oder die Spanne ueber 30 mV, wird KEINE Kalibrierung gespeichert. Grosse Schwankungen der Rohmesswerte werden gesondert protokolliert und als Hinweis auf moegliche elektrische Stoerungen ausgegeben. Die Produktions-Firmware verwendet einen nicht blockierenden 30-Werte-Ringpuffer mit 40 ms Abtastperiode (Median), liest `ecads_v1` beim Start, zeigt EC in mS/cm auf OLED und CYD und liefert `ec` in Telemetrie und lokaler API. Ohne gespeicherte Kalibrierung bleibt `ec=null`, statt einen scheinbar genauen Wert zu melden. Bei einem einzelnen DS18B20 wird dessen Temperatur verwendet. Bei zwei oder mehr Sensoren ist eine eindeutige ROM-Zuordnung ueber `w <ROM>` noetig; die Index-Reihenfolge wird nicht als Wasser-Zuordnung angenommen.

## pH-Ueberwachung

- Unter pH 5,5: Alarm `low`; ueber pH 6,5: Alarm `high`; 5,5–6,5: `normal`.
- Fehlender ADS1115, unplausibler pH oder fehlende echte pH-4/pH-7-Kalibrierung: `sensor_fault` statt erfundenem Messwert.
- Drei aufeinanderfolgende Abfragen zur Bestaetigung von Alarmzustaenden, 0,1 pH Hysterese.
- OLED, beide CYD-Anzeigen, lokale Zusammenfassung (`reading.ph_alarm`) und Server-Telemetrie zeigen den Status; Statuswechsel werden an `/api/v1/alerts` gesendet.
- Der Kalibriersketch speichert pH7/pH4 **und die jeweils gemessene DS18B20-Wassertemperatur** in `phads_v1`; die produktive Firmware liest beides. Der pH-Wert erhaelt eine Steilheitskorrektur nach Nernst mit der aktuell gemessenen DS18B20-Wassertemperatur, wenn beide Kalibriertemperaturen gueltig und nahe beieinander sind. Ansonsten wird der pH-Wert unkompensiert angezeigt (`ph_temperature_compensated=false`), keine 25-Grad-Substitution. Bei alter pH-Kalibrierung ohne DS: beide Pufferpunkte nach Anschluss des Wasser-DS erneut erfassen.
- DO ist nur ein digitaler Schwellwert, nicht die Messquelle fuer die ZWEI Alarmgrenzen. TO misst die Platinen-/Gehäusetemperatur (nicht Wasser). Zwei Referenzpunkte `a <C>`/`b <C>` werden in `toads_v1` gespeichert; da die Kennlinie der PH-4502C-Platine unbekannt ist, wird nur **zwischen** beiden Punkten interpoliert, nicht extrapoliert. Bis zur bestaetigten elektrischen Pegelsicherheit von A2 bleibt `PH_TO_WIRED=false` und `case_temperature=null`.
- `PH_DO_WIRED` und `PH_TO_WIRED` bleiben `false`, bis die PH4502C-Ausgangssignale auf ESP32-/ADS1115-vertraegliche Pegel geprueft wurden. Nicht direkt mit 5-V-Signalen verbinden.

## Lokale HTTP-API, Port 8081

| Methode | Pfad | Zweck |
|---|---|---|
| GET | `/api/local/summary` | Livewerte, Modus, Sollwerte und lokale Ausgangszustaende |
| GET | `/api/local/actuators` | API-Kompatibilitaet: Soll-/Ist-GPIO-Zustaende |
| POST | `/api/local/control/targets` | `light`/`heater`/`circulation_vent`/`exhaust_vent`, MANUAL |
| POST | `/api/local/control/mode` | `auto`/`manual` |

Die geplante Trennung Sensor-ESP32 / ESP32-C3 SuperMini ist im Produktions-Sketch noch NICHT umgesetzt; der alte direkte SSR-Code besteht weiter. Der Heizkanal wird bei ungueltiger Klimamessung gesperrt. Eine unabhaengige physische Temperaturbegrenzung ist trotzdem notwendig.

## Benoetigte Arduino-Bibliotheken

Adafruit ADS1X15, Adafruit GFX, Adafruit SSD1306, DHT sensor library, OneWire, DallasTemperature, ArduinoJson und ESP32 Arduino Core. Fuer das CYD zudem die Bibliotheken der gewaehlten CYD-Variante.

## Konfiguration und Pruefung

Im Projektroot `python export_firmware_secrets_from_env.py` ausfuehren, um `device_secrets.h` fuer den Sensor-ESP32-Sketch und CYD zu erzeugen (Dateien sind git-ignoriert). Keine WLAN-Passwoerter in Git committen.

Vor dem Flashen muss die neue Firmware mit der lokal installierten Arduino-ESP32-Toolchain kompiliert werden. Danach die Kleinspannungs-Sensorik testen. Die SSR-Lastseite bleibt bis zur fachgerechten Pruefung ausser Betrieb.
