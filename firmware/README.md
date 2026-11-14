# Grow Controller – Firmwarestruktur

Stand: 13.11.2026. **Drei getrennte Arduino-Sketches, eindeutige Namen.**

| Rolle | Firmware | Board | Status |
| --- | --- | --- | --- |
| Sensor | `firmware/Sensor/Sensor.ino` | ESP32-Pro-Controller ohne lokales Display, mit ADS1115, DHT22 und DS18B20 | Serielle Live-Messwerte im Korrektur-Branch; Hardwaretest offen |
| Remote | `firmware/Remote/Remote.ino` | CYD ESP32-2432S028R mit LVGL 8.4 / TFT_eSPI | Neu strukturierte 320×240-Querformatoberfläche, Hardwaretest offen |
| C3 | `firmware/C3/C3.ino` | ESP32-C3 SuperMini + PCA9685 | Hardwareausgänge gesperrt, fünf Dosierkanäle, Hardwaretest offen |

**Keine der drei Rollen darf eine Firmwaredatei einer anderen Rolle ersetzen.** Es gibt kein Installationsskript, das Dateien anderer Geräte überschreibt. Der Arduino-Sketchname entspricht immer dem jeweiligen Ordnernamen.

**Verbindlicher Sensor-GPIO-Abgleich:** [Sensor/PINOUT.md](Sensor/PINOUT.md). Die früher dokumentierten physischen Kontaktpositionen beziehen sich auf das ältere Board. Beim Pro-Controller sind diese Positionen noch nicht verifiziert; ältere Verdrahtungsanleitungen dürfen nicht als aktive Anschlusspläne verwendet werden.

## Wiederherstellung Sensor

`Sensor.ino` wurde aus dem letzten erhaltenen umfangreichen Sensor-Entwicklungsstand des geschlossenen, nicht gemergten PR #1 (Commit `48e2677dc9094118a16ebd9a58c9afefb57a5f9e`) wiederhergestellt, statt den veralteten Stand von `main` zu verwenden. Enthalten sind drei DHT22 (GPIO25/26/27), DS18B20 (GPIO23), ADS1115 (A0 pH / A1 TDS; I²C GPIO21/22), optionaler BH1750 (GPIO13/14), pH-/EC-Kalibrierbehandlung, Phasentaster GPIO16 und Wartungstaster GPIO19. Die lokale OLED-Ansteuerung wurde aus der Sensor-Firmware entfernt; Live-Messwerte werden alle 2 Sekunden im seriellen Monitor ausgegeben. Der separate LVGL-Remote zeigt die Messwerte über die Server-API an. Die lokale API auf Port 8081 bietet `/api/local/summary`, `/api/local/actuators` und weitere Funktionen. **Der Stand ist rekonstruiert, nicht auf realer Hardware als identisch mit der zuletzt installierten Firmware bestätigt.** Lokale, nicht in Git gespeicherte Änderungen und Kalibrierwerte lassen sich nicht aus GitHub rekonstruieren.

## Remote

`Remote.ino` ist die LVGL-Fernbedienung, keine Sensorfirmware. Querformat 320×240 mit separaten Ansichten für Werte, Klima, Lüfter und fünf Dosierpumpen. Die Gerätedaten werden aus `GET /api/v1/mobile/summary` gelesen, C3-Status aus `GET /api/v1/actuator/state`. `lv_label_set_text_static` wird nicht verwendet, um den nachgewiesenen LVGL-Absturz beim Punkte-Kürzungsmodus auszuschließen. Display wird beim Start vollständig gelöscht. Alle Dosieraktionen sind gesperrt. **Klima-/Lüfter-Schaltflächen bleiben deaktiviert, solange der Touchscreen nicht verifiziert ist** (`TOUCH_ACTUATION_VERIFIED=false`).

Die Datei `Remote/TFT_eSPI_User_Setup.h` ist eine *Referenzkonfiguration* für den im Sketch genannten CYD-Boardtyp. TFT_eSPI verwendet zur Kompilierzeit die globale `~/Arduino/libraries/TFT_eSPI/User_Setup.h` und nicht automatisch die Datei im Sketch-Ordner. Nicht blind auf andere Boards übertragen, nicht andere Sketches damit überschreiben. Die bereits lokal eingerichteten numerischen GPIOs überprüfen. Die Touch-Koordinaten sind noch nicht kalibriert.

## C3

Fünf 12-V-Dosierpumpen an PCA9685-Ausgängen 0–4 vorgesehen; PCA-Kanal 5 ist unbenutzt. Die beiden Lüftergruppen bleiben auf PCA-Kanälen 6 und 7. Die vorhandenen fünf SSR-Steuersignale bleiben im Code. `HARDWARE_OUTPUTS_ARMED=false`, alle Dosier-Maximalzeiten sind 0: Es erfolgen **keine** wirksamen Leistungsschaltungen vor gesonderter Hardwarevalidierung. Diese Dokumentation umfasst keine 230-V-Verdrahtung. Die C3-Firmware erwartet den Sensor unter `grow-sensor.local:8081`, die Abstimmung der APIs ist noch nicht auf Hardware bestätigt.

## Lokaler Update- und Build-Ablauf

1. Einmal den gewünschten Git-Branch beziehen; vor Änderungen `git status --short` prüfen. **Nicht** `git reset --hard` einsetzen, um lokale Dateien zu entfernen.
2. Nur im Projektroot `python export_firmware_secrets_from_env.py` ausführen. Das erzeugt die ignorierten `device_secrets.h`-Dateien für `Sensor`, `Remote`, `C3`. Persönliche Zugangsdaten gehören weder ins Repository noch in Support-Ausgaben.
3. In der Arduino IDE jeweils **genau eine** Datei öffnen: `Sensor.ino`, `Remote.ino` oder `C3.ino`; Board und Port entsprechend der realen Hardware wählen.
4. Zuerst kompilieren und den seriellen Monitor prüfen; danach getrennt flashen und die Hardwarefunktion verifizieren. Keine automatisch ausführbaren Kopier-/Installationsskripte.

**Teststatus:** Git-Dateikonsistenz und Schnittstellen sind textuell überprüft. Ein vollständiger Arduino-Compile, reale Touch-Kalibrierung, Sensor-Kalibrierung und integrierter Hardwaretest sind **nicht** nachgewiesen. Keine automatische Dosierung freigeben.


## pH- und EC-Kalibrierung direkt in der Sensor-Firmware

**Kein Wechsel auf einen Diagnose-Sketch mehr noetig.** In der laufenden
`firmware/Sensor/Sensor.ino` sind die Kalibrierungen per seriellem Monitor
mit **115200 Baud** verfuegbar. Den gewuenschten Buchstaben bzw. die
Ziffer senden (Zeilenende optional):

| Eingabe | Wirkung |
| --- | --- |
| `7` | pH-7,00-Puffer messen; 160 ADS1115-A0-Werte im 20-ms-Abstand |
| `4` | pH-4,00-Puffer messen; identische stabile Zwei-Punkt-Erfassung |
| `e` | 1413-uS/cm-EC-Referenz auf ADS1115 A1; 8 Fenster mit je 30 Werten/40 ms |
| `s` | pH-Punkte, EC-Faktor, DS18B20-Temperatur und Messfortschritt anzeigen |
| `x` | Laufende Erfassung abbrechen, keine neuen Werte speichern |
| `h` / `?` | Komplette serielle Befehlshilfe anzeigen |

Ein Startbefehl sollte erst gesendet werden, wenn die jeweilige Sonde
sauber und stabil in der zugehoerigen Referenzloesung steht. Anschliessend
die Ausgabe `KALIBRIERUNG OK: NVS geschrieben und zurueckgelesen` abwarten.
Vor pH4 die pH-Sonde mit geeignetem Wasser abspuelen und vorsichtig
abtupfen; Loesungen nicht gegenseitig verunreinigen.

Die Firmware verlangt wie das alte `ph_calibration_tool.ino`
**ADS1115 an 0x48 UND eine reale, gueltige Wassertemperatur des
DS18B20 an GPIO23**. Fuer die Temperaturkompensation muss der
DS18B20 dabei dieselbe Referenzloesung wie die kalibrierte Sonde messen. Ohne DS18B20 wird keine Kalibrierung unter
einer erfundenen Temperatur vorgenommen. Ein einzelner, stabiler
pH-Kalibrierpunkt darf im NVS stehen; ein **gueltiger pH-Wert**
erfordert erst beide Punkte pH7 und pH4 mit Spannungsabstand >=1 mV.
Bei instabilen Messungen oder ADC-/Temperatursensorfehlern wird
**nichts Neues** gespeichert. pH: Messwert-Standardabweichung
max. 10 mV; EC: Standardabweichung der acht Fenster-Mediane max.
10 mV und deren Spanne max. 30 mV.

Die NVS-Namensraeume und Schluessel bleiben **kompatibel**:
`phads_v1` (have7/have4, volt7/volt4, temp7/temp4);
`ecads_v1` (valid, factor, ref_v, temp_c).
Die Sensor-API, Alarmfunktionen und C3-Kommunikation laufen waehrend
der Abtastung weiter. Ein Neustart oder das Flashen mit
`Erase All Flash` wuerde die aktuelle Kalibriererfassung abbrechen;
die bereits gespeicherten NVS-Punkte bleiben bei einem normalen
Sketch-Update ohne Flash-Erase bestehen.

**Test:** `g++ -std=c++17 tests/test_calibration_capture.cpp -o /tmp/test_calibration_capture && /tmp/test_calibration_capture`
(vom Repositoryroot). Der Hosttest prueft den Berechnungskern,
nicht den ESP32/ADS1115-Hardwarepfad.

## Drei DHT22: gemeinsame Klima- und VPD-Messung

Die Sensor-Firmware liest unveraendert DHT22 #1 (GPIO25), #2 (GPIO26)
und #3 (GPIO27). Fuer die aktive Klimaregelung und Blatt-VPD-Berechnung
werden nur **gueltige Temperatur-/Feuchtepaare** beruecksichtigt:

- **3 gueltige**: Median der Temperaturen und Median der Feuchtewerte
- **2 gueltige**: arithmetischer Mittelwert der beiden Wertepaare
- **1 gueltiger**: Einzelmessung bleibt aktiv, Warnstatus
  `air_sensor_warning=true`
- **0 gueltige**: keine Ersatzwerte; Temperatur/RH/VPD werden ungueltig,
  Heizung, Umluft und Abluft erhalten AUS-Sollwerte

`air_valid_sensor_count` (0–3) und `air_measurement_quality`
(`median_3`, `mean_2`, `warning_single_sensor`,
`invalid_no_sensors`) werden in lokaler API und ESP32-Telemetrie
ausgegeben. Der LVGL-Remote zeigt bei <3 gueltigen Sensoren einen
Hinweis, sofern kein bestehender hoeher priorisierter Alarm vorliegt.
Die Einzelmesswerte `dht1_*`, `dht2_*`, `dht3_*` bleiben fuer
Diagnosezwecke erhalten. Die MLX90640 verwendet spaeter denselben
fusionierten Luftmesswert und ersetzt nur die Blatt-Temperatur (nach
Validierung). Ohne Kamera bleibt das -2-C-Modell aktiv.

Host-Regression: `g++ -std=c++17 tests/test_air_fusion.cpp -o /tmp/test_air_fusion && /tmp/test_air_fusion`
(vom Repositoryroot). Die ESP32-Gesamtfirmware benoetigt zusaetzlich
einen Arduino-Build und Hardwaretests.

## VPD-Klimaregelung (Entwicklungsbranch, nicht hardwarevalidiert)

Der Sensor-ESP32 berechnet ab jetzt Blatt-VPD aus DHT1 mit Blatt-Temperatur
**Lufttemperatur minus 2 C**, entsprechend der Referenztabelle.
Die optionale MLX90640-ROI darf erst nach Verifikation als Blattmessung
verwendet werden (`GROW_MLX_LEAF_VERIFIED`).

Die zeitbasierte Steuerung umfasst **einen vollstaendigen 13-Wochen-Durchlauf**:
W1-2 Wachstum = 0,4-0,8 kPa, W3-4 Blueteeinleitung = 0,8-1,2 kPa,
W5-13 Bluete = 1,2-1,6 kPa. Nach **14 Tagen (W3)** erfolgt der
**automatische Phasenwechsel** von Wuchs auf Bluete, auch ohne Tastendruck.
Nach **91 Tagen** wird der gesamte Durchlauf mit `cycle_stopped=true`
dauerhaft auf STOPP gesetzt. Klimasollwerte und Pumpenanforderungen
werden dabei deaktiviert. Das C3 wertet diesen STOPP aus, sperrt
direkte Fan-/Dosierkommandos und meldet den Status ans Display.
Sensor, Telemetrie und LVGL-Remote bleiben erreichbar.

Die Zeitberechnung verwendet UTC ueber NTP und einen **einzigen im NVS
gespeicherten Durchlaufstart**, der einen ESP32-Neustart uebersteht.
Die abgeschlossene Runde bleibt ebenfalls im NVS gespeichert; es gibt
**keinen automatischen Neustart nach W13**. Ein neuer Durchlauf startet
nur nach expliziter Bedienung (z.B. Phasentaster D16 nach STOPP oder
`POST /api/local/control/phase-clock` mit `{"start_now":true}`).
Bei einer Altinstallation ohne verifiziertes Startdatum wird kein
Durchlaufstart geraten und die Klimaanforderung bleibt gesperrt.
`{"start_unix":1760000000}` zeigt nur das JSON-Format fuer einen
explizit gesetzten historischen UTC-Start. Die bisherige
pH-/EC-Wochentabelle hat 12 Eintraege; fuer Woche 13 wird kein
unbekannter Sollwert erfunden.

Die Regelentscheidung `autoControl()` setzt getrennte Sollkommandos fuer
Heizung, Umluft und Abluft. Bei ungueltiger fusionierter DHT-Messung oder fehlender
Phasen-/Wochenkonfiguration werden die drei Klima-Sollkommandos
abgeschaltet. Abluft kann hohe Luftfeuchtigkeit nur dann reduzieren,
wenn die zugefuehrte Luft trockener ist; der benoetigte
Aussenluftsensor ist noch nicht nachgewiesen. Der Heizungs-Sollwert
ist fuer Wuchs aus dem Bestand (24 C) vorhanden, fuer Bluete erst
nach ausdruecklicher Konfiguration ueber
`POST /api/local/control/phase-profile` (mit `phase=bluete`,
`air_target_c` und `humidity_target_pct`). Die VPD-Bereiche ersetzen
nicht eine gesonderte Sicherheitsgrenze fuer Uebertemperatur.

Die drei Klima-Sollwerte werden vom Sensor-ESP32 ueber
`GET /api/local/actuators` an den C3 uebergeben und ueber
`POST /api/local/actuator-status` rueckgemeldet. Die
**Leistungsausgaenge bleiben mit `HARDWARE_OUTPUTS_ARMED=false` gesperrt**.
Ein erfolgreicher statischer Vertragstest ist KEIN Hardwaretest.
Netzspannungsseitige Verdrahtung, SSR-Polung und Ausfallsicherheit
muessen vor einer spaeteren Freigabe separat durch eine qualifizierte
Person geprueft werden.
