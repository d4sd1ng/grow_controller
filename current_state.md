# Current State — Growroom Control System

**Stand:** 2026-10-07  
**Arbeitsbranch:** `hardware-v2-firmware`

## Architektur

```text
Sensor-ESP (ESP32 OLED-0.96 V3.0)
  -> Sensoren lesen
  -> AUTO/MANUAL-Zielzustände halten
  -> lokale HTTP-API :8081
  -> Telemetrie an Server
             |
             | WLAN
             v
Stromkasten-ESP
  -> GPIO17 Licht
  -> GPIO18 Umluft
  -> GPIO19 Abluft
  -> GPIO23 Heizung
  -> Watchdog: Kommunikationsverlust => alle vier Ausgänge AUS

CYD
  -> Livewerte + Steuerung direkt vom Sensor-ESP
  -> Historie/Alerts über den Server (TFT_eSPI-Remote)

Raspberry Pi / Server
  -> FastAPI
  -> Streamlit Dashboard
  -> derzeit noch SQLite
  -> PostgreSQL-Umbau ist der nächste separate Schritt
```

## Hardware

### Sensor-ESP

| Funktion | Pin |
|---|---:|
| OLED + ADS1115 SDA | GPIO21 |
| OLED + ADS1115 SCL | GPIO22 |
| DHT22 #1 | GPIO25 |
| DHT22 #2 | GPIO26 |
| DHT22 #3 | GPIO27 |
| 2x DS18B20 1-Wire | D18 |
| BH1750 SDA | GPIO13 |
| BH1750 SCL | GPIO14 |

ADS1115: A0 = PH4502C über 10k/20k-Teiler, A1 = CQRobot TDS.

### Stromkasten-ESP

| GPIO | SSR |
|---:|---|
| 17 | Licht |
| 18 | beide Umluftventilatoren |
| 19 | Abluft |
| 23 | Heizstrahler |

FOTEK SSR-10DA: vorhandene Geräte sind mit 3–32 VDC Input beschriftet. Deshalb keine zusätzliche ULN2803A-Stufe im aktuellen Plan.

Die Wasserpumpe läuft dauerhaft und wird nicht geschaltet.

## Firmware

| Sketch | Rolle |
|---|---|
| `firmware/growroom_telemetry_client/growroom_telemetry_client.ino` | Sensor-/Steuerknoten |
| `firmware/powerbox_actuator/powerbox_actuator.ino` | Stromkasten-Aktorknoten |
| `firmware/cyd_remote_control/cyd_remote_control.ino` | primäre 3-Panel-CYD-Fernbedienung |
| `firmware/cyd_grow_display/cyd_grow_display.ino` | alternative kompakte LVGL-CYD-Oberfläche |
| `firmware/ph_calibration_tool/ph_calibration_tool.ino` | dauerhaftes Service-/Kalibrier-Tool auf Ersatz-ESP |

## pH

Historische reale Kalibrierung:

`pH = -5.7 × U_PO + 21.34`

Mit dem aktuellen 10k/20k-Spannungsteiler sind in der Sensor-Firmware vorläufig äquivalente ADS1115-Punkte hinterlegt:

- pH 7: ca. 1.677193 V am ADS1115
- pH 4: ca. 2.028070 V am ADS1115

Das Kalibrier-Servicegeraet verwendet keinen ADS1115. Der PH4502C-PO-Ausgang wird ueber 10k/(2x10k) direkt auf D4 des Ersatz-ESP gefuehrt und mit dessen internem ADC gemessen. Ein DS18B20 auf D18 misst die Kalibriertemperatur; pH-7-/pH-4-Punkte werden nur mit gueltiger Temperatur angenommen und zusammen mit dieser im ESP32-NVS gespeichert. Das integrierte SSD1306-OLED auf D21/D22 zeigt Temperatur, ADC-/PO-Spannung, Streuung, Kalibrierstatus und nach zwei Punkten den berechneten pH-Wert. Eine automatische Puffer-Temperaturkorrektur bleibt bewusst aus, bis die Temperaturtabelle des konkret verwendeten Pufferprodukts feststeht. Offen sind Kompilier-/Flash-Test und die physische Kalibrierung mit den realen Pufferloesungen.

## Server-Zwischenstand vor PostgreSQL

Der Server bleibt bis zum nächsten Umbau SQLite-basiert. Wenn `IOT_SENSOR_HOST` gesetzt ist, leiten Dashboard und FastAPI MANUAL-/Modus-Befehle über die lokale Sensor-ESP-API weiter; ohne diese Konfiguration bleibt der Python-Simulator/Fallback aktiv. Das Daten-/Control-Modell ist bereits auf Hardware V2 umgestellt:

- Aktoren: `light`, `heater`, `circulation_vent`, `exhaust_vent`
- keine steuerbare Pumpe mehr;
- neue Messwerte wie TDS, zusätzliche DHT22- und DS18B20-Werte werden vom Ingest validiert und können als Zusatzfelder gespeichert werden;
- bestehende SQLite-Dateien erhalten beim Öffnen automatisch eine `light`-Spalte; eine alte physische `pump`-Spalte kann als ungenutztes Legacy-Feld bestehen bleiben.

## Offener Software-Schritt

**PostgreSQL-Migration.**

Dabei wird der bestehende PostgreSQL-Container auf dem Raspberry Pi verwendet. SQLite wird danach aus dem Produktionspfad entfernt.

## Vor Merge nach main

- Python-Unit-Tests ausführen.
- Sensor-ESP kompilieren.
- Stromkasten-ESP kompilieren.
- verwendete CYD-Variante kompilieren.
- nach Hardwareaufbau pH-Punkte und DS18B20-ROM-Zuordnung verifizieren.
