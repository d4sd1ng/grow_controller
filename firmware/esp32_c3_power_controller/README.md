# Grow Controller — ESP32-C3 SuperMini (Firmware 1.2.0)

**Bereitgestellt:** 12. November 2026. **Der fertige Sensorcontroller wurde nicht geändert.**

## Festgelegte Funktionen

- Kein OLED, keine weiteren physischen Taster am C3.
- Zwei bereits vorhandene Taster **auf der Sensorplatine**: Wasserpumpe EIN/AUS (Filterwechsel), Wachstum/Blüte; letzterer ist kein C3-Befehl.
- Fünf SSR-Steuersignale: Licht, Abluft, Umluft, Heizung, Wasserpumpe.
- Wasserpumpe folgt **ausschließlich** dem booleschen `water_pump`-Feld aus `GET /api/local/actuators` des Sensorcontrollers. Fehlt das Feld, bleibt dieser C3-Ausgang AUS; kein Ersatzwert wird erfunden.
- Genau sechs 12-V-/3-W-Dosierpumpen an PCA9685 Kanälen 0–5 (MOSFET-Stufen, keine Umpolung).
- Genau vier 120-mm-/12-V-PC-Lüfter, in zwei Zweiergruppen parallel, als reine EIN/AUS-Lasten an PCA9685 Kanälen 6–7. Keine Drehzahlsteuerung, TACH-Auswertung oder PWM-Regelung der Lüfter.
- WLAN-Sollwerte vom Sensor-ESP, ESP32-C3-Status für LVGL, Fail-Safe bei WLAN-/Steuerungsausfall, gesicherte HTTP-Endpunkte und Hardware-Zeitwächter für Dosierimpulse.

## GPIO-Matrix des Firmwareentwurfs

| C3-GPIO | Funktion |
|---|---|
| GPIO0 | SSR Licht |
| GPIO1 | SSR Abluft |
| GPIO3 | SSR Umluft |
| GPIO4 | SSR Heizung |
| GPIO5 | SSR Wasserpumpe |
| GPIO7 | I²C SDA → PCA9685 SDA |
| GPIO10 | I²C SCL → PCA9685 SCL |
| GPIO20 | PCA9685 OE (HIGH gesperrt, LOW freigegeben) |

Die PCA9685-Logikversorgung ist 3,3 V; die Motor-/Lüfterversorgung ist davon getrennt. PCA-Ausgänge sind **nur Steuersignale an MOSFET-Stufen**, keine 12-V-Leistungsausgänge. 230-V-Anschluss/Verkabelung ist nicht Teil dieses Pakets.

## HTTP-Schnittstellen

### Sensorcontroller → C3
- `GET http://grow-sensor.local:8081/api/local/actuators` mit `X-Telemetry-Key`.
- Erwartet `{"ok":true,"commands":{"light":true,"exhaust_vent":false,"circulation_vent":false,"heater":false,"water_pump":true}}`. **Beispiel eines Protokolls, kein geprüfter Ist-Zustand der fertigen Sensor-Firmware.**
- `POST http://grow-sensor.local:8081/api/local/actuator-status` meldet alle fünf SSR-Zustände zurück.

### LVGL → C3
- `GET /api/v1/actuator/state` für SSR-/Lüfter-/Pumpenstatus.
- `POST /api/v1/actuator/fans` für die beiden Lüftergruppen.
- `POST /api/v1/actuator/dose` nur für begrenzte, kalibrierte Impulse; ab Werk gesperrt.
- `POST /api/v1/actuator/stop` setzt Ausgänge sofort auf AUS und verriegelt bis Neustart.

Die LVGL-Remote erhält **keinen** eigenen Taster für Wasserpumpe oder Wachstum/Blüte. Der C3 akzeptiert keinen HTTP-Schaltbefehl zum Umgehen der physischen Wasserpumpenbedienung.

## Konfiguration

1. Ausschliesslich die lokale Datei `.env` im Projektroot mit `IOT_WIFI_SSID`, `IOT_WIFI_PASSWORD`, `IOT_TELEMETRY_SERVER_HOST` und `TELEMETRY_API_KEY` pflegen. Keine echten Zugangsdaten ins Repository stellen.
2. Im Projektroot `python export_firmware_secrets_from_env.py` ausfuehren. Erst danach den C3-Sketch kompilieren: der Exporter erzeugt die ignorierte `device_secrets.h`. Ohne diese Datei bricht der Sketch absichtlich mit Fehlermeldung ab.
3. Zielboard: ESP32-C3 SuperMini mit Arduino-ESP32 Core und ArduinoJson v6. `esp_timer.h`, HTTPClient, WebServer, ESPmDNS und Wire stammen aus dem ESP32-Arduino-/ESP-IDF-Umfeld.
4. **Nicht ohne Hardwarefreigabe entsperren:** `HARDWARE_OUTPUTS_ARMED=false` schützt gegen ungetestete Elektrik. Alle sechs `MAX_DOSE_MS` stehen absichtlich auf 0, bis reale Pumpenkalibrierung und Sicherheitsprüfung erfolgt sind. /OE muss im realen Aufbau auch vor dem Start des Sketches einen definierten sicheren Zustand haben.

## Ehrlicher Prüfstatus

- Quelltext- und Schnittstellenprüfungen sind durchgeführt (siehe `tests/check_c3.py`).
- **Keine** Arduino-C3-Kompilierung; `arduino-cli` und die Bibliotheken sind in dieser Arbeitsumgebung nicht installiert.
- **Kein** echter PCA9685-/MOSFET-/SSR-Test und **kein** Test der Integration mit der bereits fertigen Sensor-Firmware.
- Ein Schaltbefehl für die Wasserpumpe wird vom C3 verarbeitet, **wenn** die fertige Sensorcontroller-Firmware tatsächlich `commands.water_pump` sendet. Das wurde bislang nicht an der endgültigen Firmware geprüft; ohne dieses Feld bleibt der Kanal aus.

**Kein neuer GitHub-Branch, keine Änderung am Sensorcontroller, keine Platinenbearbeitung.**