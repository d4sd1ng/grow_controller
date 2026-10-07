# Project Overview — Growroom Control System

## Zweck

Autonome Erfassung und Regelung eines Indoor-Growrooms. Die Echtzeitsteuerung bleibt im lokalen ESP32-Netz; der Raspberry Pi übernimmt Historie, API, Dashboard und Auswertung.

## Komponenten

| Komponente | Aufgabe |
|---|---|
| Sensor-ESP | Sensoren, Plausibilisierung, AUTO/MANUAL-Zielzustände, OLED, lokale HTTP-API, Telemetrie |
| Stromkasten-ESP | vier SSR-Steuerausgänge und Kommunikations-Watchdog |
| CYD | portable Liveanzeige und manuelle Steuerung |
| Raspberry Pi | FastAPI, Streamlit, Historie, Alerts; aktuell noch SQLite |
| PostgreSQL | vorhandener Container; Migration noch offen |

## Datenfluss

```text
Sensoren -> Sensor-ESP -> Sollzustände -> WLAN -> Stromkasten-ESP -> SSR
                    |
                    +-> WLAN -> CYD
                    |
                    +-> Telemetrie -> FastAPI -> SQLite (vorübergehend)
                                           -> Dashboard / Historie
```

## Sensorik

- 3x DHT22
- 2x DS18B20 auf gemeinsamem 1-Wire-Bus
- PH4502C über ADS1115 A0 und 10k/20k-Spannungsteiler
- CQRobot CQRSENTDS01 über ADS1115 A1
- BH1750 auf separatem I2C-Bus
- integriertes OLED des Sensor-ESP

## Aktoren

| Stromkasten-ESP | Last |
|---:|---|
| GPIO17 | Licht |
| GPIO18 | Umluft links + rechts |
| GPIO19 | Abluft |
| GPIO23 | Heizstrahler |

Die Wasserpumpe läuft dauerhaft und ist nicht Teil der Aktorlogik.

## Betriebslogik

AUTO:
- Sensor-ESP regelt Heizung/Umluft/Abluft anhand der lokalen Sensorwerte.
- Lichtzustand ist ein eigener Aktorkanal und kann über Zeitplan/Steuerung gesetzt werden.
- Stromkasten-ESP holt die Zielzustände direkt vom Sensor-ESP.
- bei Kommunikationsverlust setzt der Stromkasten-ESP alle vier Ausgänge AUS.

MANUAL:
- CYD schreibt die Zielzustände direkt an die lokale API des Sensor-ESP.
- LIGHT / HEAT / CIRC / EXH sind einzeln schaltbar.

## Server

FastAPI und Streamlit bleiben außerhalb der Echtzeit-Regelkette. Telemetrie wird gespeichert und für Historie/Charts/Alerts verwendet.

Der aktuelle Hardware-V2-Branch ist serverseitig bereits auf die neuen Aktorbezeichnungen umgestellt, verwendet aber noch SQLite. Die PostgreSQL-Migration ist bewusst ein separater nächster Schritt.

## Firmware

| Pfad | Rolle |
|---|---|
| `firmware/growroom_telemetry_client` | Sensor-/Steuerknoten |
| `firmware/powerbox_actuator` | Stromkasten-Aktor |
| `firmware/cyd_remote_control` | primäre TFT_eSPI-CYD-Fernbedienung |
| `firmware/cyd_grow_display` | alternative LVGL-CYD-Oberfläche |
| `firmware/ph_calibration_tool` | pH-Kalibriergerät |

## Kalibrierung

Historische pH-Gerätekennlinie: `pH = -5.7 × U_PO + 21.34`.

Die neue Messkette wird mit dem separaten Kalibrier-Sketch erneut über pH 7 und pH 4 geprüft. Die beiden DS18B20 werden nach dem Hardwareaufbau anhand ihrer ROM-Adressen eindeutig den physischen Positionen zugeordnet.
