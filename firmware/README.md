# Grow Controller – Firmwarestruktur

Stand: 13.11.2026. **Drei getrennte Arduino-Sketches, eindeutige Namen.**

| Rolle | Firmware | Board | Status |
| --- | --- | --- | --- |
| Sensor | `firmware/Sensor/Sensor.ino` | ESP32-WROOM mit OLED, ADS1115, DHT22 und DS18B20 | Historisch aus PR #1 wiederhergestellt; Hardwarestand nicht erneut verifiziert |
| Remote | `firmware/Remote/Remote.ino` | CYD ESP32-2432S028R mit LVGL 8.4 / TFT_eSPI | Neu strukturierte 320×240-Querformatoberfläche, Hardwaretest offen |
| C3 | `firmware/C3/C3.ino` | ESP32-C3 SuperMini + PCA9685 | Hardwareausgänge gesperrt, fünf Dosierkanäle, Hardwaretest offen |

**Keine der drei Rollen darf eine Firmwaredatei einer anderen Rolle ersetzen.** Es gibt kein Installationsskript, das Dateien anderer Geräte überschreibt. Der Arduino-Sketchname entspricht immer dem jeweiligen Ordnernamen.

## Wiederherstellung Sensor

`Sensor.ino` wurde aus dem letzten erhaltenen umfangreichen Sensor-Entwicklungsstand des geschlossenen, nicht gemergten PR #1 (Commit `48e2677dc9094118a16ebd9a58c9afefb57a5f9e`) wiederhergestellt, statt den veralteten Stand von `main` zu verwenden. Enthalten sind drei DHT22 (GPIO25/26/27), DS18B20 (GPIO23), ADS1115 (A0 pH / A1 TDS; I²C GPIO21/22), integriertes OLED, optionaler BH1750 (GPIO13/14), pH-/EC-Kalibrierbehandlung, Phasentaster GPIO16 und Wartungstaster GPIO19. Die lokale API auf Port 8081 bietet `/api/local/summary`, `/api/local/actuators` und weitere Funktionen. **Der Stand ist rekonstruiert, nicht auf realer Hardware als identisch mit der zuletzt installierten Firmware bestätigt.** Lokale, nicht in Git gespeicherte Änderungen und Kalibrierwerte lassen sich nicht aus GitHub rekonstruieren.

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
