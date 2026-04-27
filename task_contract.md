# Task Contract — Growroom Control System

## Offene Aufgaben

| ID | Aufgabe | Status | Priorität | Beschreibung |
|----|---------|--------|-----------|--------------|
| T01 | Steuerlogik auf ESP32 auslagern | **ERLEDIGT** | — | `growroom_telemetry_client.ino`: Regelung (Hysterese) vom Python-Server auf den ESP32 verlagert. ESP32 ist autonome Steuerzentrale. Server dient Logging + Dashboard + MANUAL-Override. |
| T02 | OLED-Anzeige einbauen | **ERLEDIGT** | — | SSD1306 128x64 in `growroom_telemetry_client.ino` eingebaut. Zeigt Luft-Temp, Luft-RH, Gehäuse-Temp, EC, pH, Relais-Status. Keine Touch/Steuerung über Display. |
| T03 | Sensor-Libraries einbinden | **ERLEDIGT** | — | Echte DHT22-Library (GPIO36), ADC-Lesung für EC (GPIO34) und pH (GPIO35) mit LM358-Verstärker-Modul eingebunden. |
| T04 | CYD LVGL v8 stabilisieren | **ERLEDIGT** | — | `cyd_grow_display.ino`: Buffer vergrößert, WiFi-Reconnect, HTTP-Timeout, Touch-Buttons für Relais, `lv_timer_handler()` optimiert. |
| T05 | cyd_remote_control wiederherstellen | **ERLEDIGT** | — | Original TFT_eSPI-Code vor LVGL-Commit wiederhergestellt (`git checkout 745d1b8`). |

## Abgeschlossene Aufgaben (diese Session)

| ID | Aufgabe | Ergebnis |
|----|---------|----------|
| T01 | Steuerlogik auf ESP32 auslagern | `growroom_telemetry_client.ino` komplett umgebaut: Hysterese, AUTO/MANUAL, echte Sensoren, OLED. |
| T02 | OLED-Anzeige einbauen | SSD1306 128x64 mit I2C-Autoerkennung eingebaut. 4 Zeilen: Temp/RH, Case-Temp, EC/pH, Modus/Relais. |
| T03 | Sensor-Libraries einbinden | DHT22-Library für GPIO36, ADC mit Kalibrierung für EC/pH auf GPIO34/35. |
| T04 | `cyd_grow_display` stabilisieren | Buffer 320×20, WiFi-Reconnect, 5s HTTP-Timeout, 4 Touch-Buttons (HEAT/PUMP/CIRC/EXH), TFT_eSPI Setup kommentiert. |
| T05 | `cyd_remote_control` zurücksetzen | Ordner auf Original-Stand vor `feat(display)` Commit wiederhergestellt. |

## Nächster Schritt (vorgeschlagen)

Keine offenen Tasks. Projekt ist feature-complete.

### Akzeptanzkriterien für T04 (alle erfüllt)
- [x] CYD (`cyd_grow_display`) kompiliert mit LVGL v8
- [x] Touch-Input funktioniert (4 Relais-Buttons)
- [x] HTTP-Kommunikation mit Server läuft (Timeout 5s, Reconnect)
- [x] Zeigt aktuelle Werte vom Server an
