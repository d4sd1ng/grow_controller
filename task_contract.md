# Task Contract — Growroom Control System

**Stand:** 2026-10-07

## Aktueller Umbau

| ID | Aufgabe | Status | Anmerkung |
|---|---|---|---|
| H01 | Wiringplan V2 | **ERLEDIGT** | Sensor-/Stromkasten getrennt; Pins, Pull-ups, ADS1115, Spannungsteiler und SSR-Steuereingänge dokumentiert. |
| H02 | Sensor-ESP Firmware | **ERLEDIGT (Code)** | 3x DHT22, 2x DS18B20, ADS1115, PH4502C, CQRobot-TDS, BH1750, OLED, lokale AUTO/MANUAL-API. |
| H03 | Stromkasten-ESP Firmware | **ERLEDIGT (Code)** | 4 SSR: Licht, Umluft, Abluft, Heizung; WLAN zum Sensor-ESP; Watchdog/Failsafe. |
| H04 | CYD Hardware-V2 | **ERLEDIGT (Code)** | LIGHT / HEAT / CIRC / EXH; Live-Steuerung direkt zum Sensor-ESP. |
| H05 | pH-Kalibrier-Tool | **ERLEDIGT (Code)** | Separater Ersatz-ESP; pH 7/4 erfassen, mitteln und Konstanten ausgeben. |
| H06 | Server-Datenmodell | **ERLEDIGT für SQLite-Zwischenstand** | steuerbare Pumpe entfernt, `light` als Aktorkanal; neue Sensorfelder werden validiert/als Extras erhalten. |
| H07 | Dashboard / Simulator / ML / Tests | **ERLEDIGT (Code)** | Aktorset auf LIGHT/HEAT/CIRC/EXH umgestellt; Pumpensteuerung entfernt. |
| H08 | PostgreSQL statt SQLite | **OFFEN** | Bestehenden PostgreSQL-Container verwenden; eigener DB/User/Schema-Kontext. |

## Hardware-V2-Aktoren

`light`, `heater`, `circulation_vent`, `exhaust_vent`

Die Wasserpumpe läuft dauerhaft und ist **kein** Aktor, kein SSR-Kanal und kein GPIO-Ziel.

## Noch physisch zu bestimmen

Diese Punkte sind keine offenen Software-Implementierungen:

- finale pH-4-/pH-7-Messwerte mit der aufgebauten ADS1115-Messkette;
- ROM-Adressen der beiden DS18B20, damit ihre physische Position eindeutig zugeordnet werden kann;
- Hardware-Smoke-Test nach dem Aufbau.

## Validierung

Vor Merge nach `main` sind ein Python-Testlauf und echte Arduino-Kompilierchecks der Firmware vorgesehen. Der PostgreSQL-Umbau erfolgt danach als eigener Schritt.
