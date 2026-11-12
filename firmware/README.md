# Grow Controller — drei Firmware-Rollen (Arbeitsstand)

**Stand: 2026-11-12, Branch feature/six-pump-c3-firmware-integration.**

| Firmware | Hardware | Rolle |
|---|---|---|
| growroom_telemetry_client/growroom_telemetry_client.ino | ESP32-WROOM / ESP Pro mit OLED | Sensoren, Hysterese, Telemetrie, Weitergabe der SSR-Schaltziele |
| cyd_grow_display/cyd_grow_display.ino | CYD mit LVGL v8 | Remote-Monitoring und Bedienung |
| esp32_c3_power_controller/esp32_c3_power_controller.ino | ESP32-C3 SuperMini **ohne Display** | Aktor-Knoten mit 5 SSR und genau 6 MOSFET-Dosierpumpen |

Die historische TFT_eSPI-Firmware cyd_remote_control bleibt unveraendert und ist keine dieser drei Rollen.

## Physische Kanaele

- Fuenf SSR-Funktionen: Licht, Abluft, Umluft, Heizung, Wasserpumpe (separat von den Dosierpumpen).
- Sechs unidirektionale Dosierpumpen an 12 V DC / 3 W je Pumpe. **18 W Nennleistung / 1,5 A nominal zusammen**; Einschaltstrom ist nicht bekannt.
- C3: **kein OLED, kein Display-GPIO**, keine Umpolung.
- OLED bleibt ausschliesslich am Sensor-ESP.

### Hardwarefreigabe noch offen

Der C3 soll elf physische Schaltausgaenge bedienen. Fuer das konkrete SuperMini-Board wurde keine konfliktfreie GPIO-Belegung freigegeben. USB-/Boot-Pins duerfen nicht blind belegt werden; ggf. wird ein separater I/O-Expander oder anderer Treiber notwendig.

Der neue C3-Sketch ist bewusst **fail-closed**: OUTPUTS_APPROVED=false, alle SSR_GPIO und DOSER_GPIO=-1, MAX_DOSING_MS=0; das LVGL-Display hat DOSER_UI_DURATION_MS=0. Alle realen Ausgaenge sind deshalb gesperrt. Erst nach Pruefung von Boardpins, Treiberpolaritaten, MOSFET-Stufen, Pumpenkalibrierung und Abschaltverhalten darf die Hardwarefreigabe erfolgen.

## Netzwerk

Sensorcontroller und LVGL-Remote verwenden die bisherige API:
- GET /api/v1/control/state
- POST /api/v1/control/targets
- POST /api/v1/telemetry
- GET /api/v1/mobile/summary

Der C3 bietet lokal auf Port 80 mit X-Telemetry-Key:
- GET /api/v1/actuator/state (SSR-Zustand, aktiver Dosierkanal, Hardwarefreigabe)
- POST /api/v1/actuator/relays (boolesche Teilupdates fuer light, exhaust_vent, circulation_vent, heater, water_pump)
- POST /api/v1/actuator/dose (Pumpen-ID 1..6 und gepruefte duration_ms; maximal eine gleichzeitig)
- POST /api/v1/actuator/stop (saemtliche Ausgaenge abschalten)

Der Sensor-ESP leitet die vier bekannten SSR-Ziele des bestehenden Servers per WLAN an den C3 weiter. Die bestehende Server-Datenbank kennt bisher kein light-Ziel; das LVGL-Display kann Licht darum nur direkt beim C3 anfordern. Dosierbefehle werden bisher nicht serverseitig protokolliert. Die Erweiterung der Server-API bleibt eine gesonderte Aufgabe.

## Geheime Konfiguration

Im Projektroot in .env: IOT_WIFI_SSID, IOT_WIFI_PASSWORD, IOT_TELEMETRY_SERVER_HOST, TELEMETRY_API_PORT und TELEMETRY_API_KEY. **Neu:** IOT_ACTUATOR_HOST ist der Hostname oder die LAN-IP des C3. Der Befehl python export_firmware_secrets_from_env.py erstellt in jedem Firmwareordner die ignorierte device_secrets.h, auch beim neuen C3. Keine Zugangsdaten commiten.

## Sensorcontroller — derzeitige Grenzen

- DHT22: GPIO25/26/27; der erste Sensor ist Steuerwert, die weiteren Zonen sind Diagnose.
- ADS1115 auf GPIO21/22; Rohspannung von A0 (pH) und A1 (TDS), **keine** unkalibrierte Umrechnung in pH/EC.
- Passives OLED auf demselben I2C-Bus; BH1750 auf getrenntem Bus GPIO13/14.
- DS18B20 an GPIO23 ist noch **nicht** in den Firmwarecode integriert; die Wassertemperatur wird als fehlend statt erfunden gemeldet.
- Lokale SSR-GPIO-Ausgaenge sind entfernt; ohne gueltigen DHT-Wert sind die AUTO-Schaltziele AUS.
- Keine automatische Dosierung aufgrund von unkalibrierten Sensoren.

## Software und Pruefstatus

Bestehende Bibliotheken: Arduino-ESP32-Core (WiFi, WebServer, HTTPClient, Wire, WiFiClientSecure), ArduinoJson v6, DHT, Adafruit_GFX, Adafruit_SSD1306; auf dem Display zusaetzlich LVGL v8, TFT_eSPI und XPT2046_Touchscreen.

**Noch kein Arduino-Compile und kein Hardware-Smoke-Test.** Keine Freigabe zum Anschluss von Lasten und kein Release-Tag. Das Repository folgt weiterhin architecture_rules.md und dem dortigen Änderungs-/Validierungsvertrag.
