# Project Overview — Grow Controller

## Zweck

Autonome Messung und Regelung eines Indoor-Salat-Growrooms. Die lokale Steuerung ist nicht von der Netzwerkverbindung zum Raspberry Pi abhaengig.

## Komponenten

| Hardware | Aufgabe |
|---|---|
| Ein ESP32 mit OLED | alle Sensoren, VPD-/AUTO-Regelung, MANUAL-Befehle, vier SSR-Steuersignale |
| Cheap Yellow Display (CYD) | portable Anzeige und Bedienung ueber WLAN |
| Raspberry Pi 4 | FastAPI, Dashboard, Telemetrie, Historie |
| PostgreSQL | vorhandener Server; Migration noch offen |

## Datenfluss

```text
Sensoren -> ESP32 -> lokale GPIOs -> SSR-Steuereingaenge
                |
                +---- WLAN <--> CYD
                +---- WLAN ----> Raspberry Pi / Dashboard
```

## Sensoren

- 3x DHT22 an GPIO25/26/27 (noch nicht verkabelt)
- bis zu 2x DS18B20 an GPIO23 (gemeinsamer 1-Wire-Bus)
- ADS1115 an GPIO21/22: A0 PH4502C mit 10k/20k-Spannungsteiler, A1 CQRobot TDS
- BH1750 an GPIO13/14 (noch nicht verkabelt)
- integriertes OLED an GPIO21/22

## SSR-Aktoren

| GPIO | Kanal |
|---:|---|
| 17 | Licht |
| 18 | Umluft |
| 19 | Abluft |
| 33 | Heizstrahler |

Bei Sensorausfall sperrt die Firmware den Heizkanal. Zusaetzlich ist eine unabhaengige Temperaturbegrenzung notwendig. Ein separater Powerbox-ESP32 existiert nicht mehr. Die Wasserpumpe ist kein Bestandteil des aktuellen Aufbaus.

## Software

Die lokale API stellt Messwerte, AUTO-/MANUAL-Betrieb sowie Soll- und Ist-GPIO-Zustaende bereit. Das CYD nutzt weiter die gleichen Befehlsnamen: `light`, `heater`, `circulation_vent`, `exhaust_vent`. Die Telemetrie wird an den Raspberry Pi gesendet.

Das Kalibrier-/Diagnosetool liest pH ueber ADS1115 A0 und TDS-Signal ueber A1, prueft OLED/DS18B20 und meldet spaeter DHT22/BH1750 nach Anschluss. Keine SSR-Aktivierung im Diagnosesketch.

Die Server-Datenbank ist derzeit noch SQLite, PostgreSQL folgt separat.

## Status

Ein-ESP32-Firmware und Dokumentation aktualisiert. Ausstehend: Kompilierung, reale Sensor- und SSR-Steuereingangstests (nur Kleinspannung), DS18B20-ROM-Zuordnung, pH-Kalibrierung, Pruefung der Netzspannungsinstallation.
