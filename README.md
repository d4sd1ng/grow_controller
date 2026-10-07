# Growroom Control System

Autonomes Growroom-Steuersystem mit getrenntem Sensor-ESP und Stromkasten-ESP, portablem CYD, FastAPI/Streamlit-Server und persistenter Telemetrie.

**Verdrahtung:** [wiring_plan_v2.md](wiring_plan_v2.md) · **Firmware:** [firmware/README.md](firmware/README.md) · **Aktueller Stand:** [current_state.md](current_state.md)

## Hardware-V2-Architektur

```text
Sensor-ESP
  3x DHT22
  2x DS18B20
  ADS1115 -> PH4502C + CQRobot TDS
  BH1750
  OLED
       |
       | WLAN / lokale HTTP-API
       v
Stromkasten-ESP
  LIGHT / CIRC / EXH / HEAT
       ^
       |
       +---- CYD: Live-Steuerung direkt zum Sensor-ESP

Sensor-ESP ---- Telemetrie ----> Raspberry Pi / FastAPI / Dashboard
                                 derzeit SQLite
                                 PostgreSQL-Migration folgt separat
```

Die Wasserpumpe läuft dauerhaft. Sie ist **kein** SSR-Kanal, kein GPIO-Ziel und kein Software-Aktor.

## Setup

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r requirements.txt
```

Unter Linux entsprechend die venv mit `source .venv/bin/activate` aktivieren.

`.env.example` nach `.env` kopieren und Werte setzen. Secrets gehören nur in `.env` bzw. in die daraus generierten `device_secrets.h`-Dateien.

```bash
python export_firmware_secrets_from_env.py
```

Der Export erzeugt Secrets für Sensor-ESP, Stromkasten-ESP und beide CYD-Sketchvarianten.

## Server starten

```bash
python run_telemetry_api.py
streamlit run main.py
```

Standard-API-Port: `8765`.

Wichtige Server-Endpunkte:

- `POST /api/v1/telemetry`
- `GET /api/v1/telemetry/latest`
- `GET /api/v1/telemetry/series`
- `GET /api/v1/alerts/recent`
- `GET/POST /api/v1/control/*`

Die reale Echtzeitsteuerung zwischen Sensor-ESP, Stromkasten-ESP und CYD ist nicht vom Raspberry Pi abhängig.

## Aktorkanäle

| Key | Funktion |
|---|---|
| `light` | Beleuchtung |
| `heater` | Heizstrahler |
| `circulation_vent` | beide Umluftventilatoren |
| `exhaust_vent` | Abluft |

## Telemetrie

Der aktuelle Zwischenstand verwendet weiterhin SQLite unter `data/grow_telemetry.db`. Die Hardware-V2-Felder sind bereits berücksichtigt; zusätzliche Sensorwerte bleiben über `extras_json` erhalten.

Die geplante Produktionsarchitektur verwendet den bereits vorhandenen PostgreSQL-Container auf dem Raspberry Pi. Dieser Umbau ist der nächste separate Schritt.

## Firmware

- `firmware/growroom_telemetry_client/` — Sensor-/Steuerknoten
- `firmware/powerbox_actuator/` — Stromkasten-Aktorknoten
- `firmware/cyd_remote_control/` — 3-Panel-CYD-Fernbedienung
- `firmware/cyd_grow_display/` — alternative LVGL-CYD-Oberfläche
- `firmware/ph_calibration_tool/` — pH-Servicegerät auf Ersatz-ESP

## pH-Kalibrierung

Historische Messgerade:

`pH = -5.7 × U_PO + 21.34`

Für die neue ADS1115-Messkette liegen entsprechende Startwerte in der Sensor-Firmware. Das separate Kalibrier-Tool erfasst später pH 7 und pH 4 erneut über die komplette reale Messkette.

## Tests

```bash
python -m unittest system_tests test_extended
```

Vor dem Merge des Hardware-V2-Branches nach `main` sollen zusätzlich die verwendeten Arduino-Sketches mit der realen ESP32-Toolchain kompiliert werden.
