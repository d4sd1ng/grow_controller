# Grow Controller

Grow-Controller fuer Indoor-Salat auf **einem ESP32 mit integriertem OLED**, mit Cheap Yellow Display (CYD) als Fernbedienung und Raspberry Pi als Telemetrie-/Dashboard-Server.

**Aktuelle Anschluesse:** [wiring_plan_v2.md](wiring_plan_v2.md) (Ein-ESP32-Plan V3) · **Firmware:** [firmware/README.md](firmware/README.md) · **Status:** [current_state.md](current_state.md)

## Architektur

```text
DHT22 / DS18B20 / PH4502C / CQRobot TDS / BH1750
                   |
                   v
     Ein ESP32 mit OLED + ADS1115
         |                 |
         |                 +--> SSR1..4 (Licht/Umluft/Abluft/Heizung)
         |
         +---- WLAN <----> CYD (Anzeige und Bedienung)
         +---- WLAN -----> Raspberry Pi (FastAPI, Dashboard, Historie)
```

Kein zweiter ESP32 und kein ATmega2560. Keine Wasserpumpe als Lastkanal. Die 230-V-Seite ist nicht Teil der Mikrocontrollerverdrahtung.

## Firmware

- `firmware/growroom_telemetry_client/` — aktive Sensor- und SSR-Steuerung fuer den **einen ESP32**.
- `firmware/ph_calibration_tool/` — temporaerer Hardware-/pH-Kalibriersketch auf demselben ESP32 (SSR AUS).
- `firmware/ds18b20_diagnostic/` — optionaler 1-Wire-Testscketch.
- `firmware/cyd_remote_control/` — CYD-Fernbedienung.
- `firmware/cyd_grow_display/` — alternative CYD-Firmware.
- `firmware/powerbox_actuator/` — alter Zwei-ESP32-Sketch, **nicht mehr verwenden**.

## Server

Der aktuelle Code nutzt weiterhin FastAPI und Streamlit. Die Telemetrie liegt im Zwischenstand in SQLite; die geplante PostgreSQL-Migration ist ein separater Schritt. Die lokale ESP32-Steuerung benoetigt zur Aktorschaltung keine Kommunikation mit dem Raspberry Pi.

## Vorbereitung

```bash
python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
python export_firmware_secrets_from_env.py
```

Dafuer muss eine gueltige lokale `.env` vorliegen. Keine Secrets in Git ablegen.

## Tests

```bash
python -m unittest system_tests test_extended
```

Arduino-Sketches zusaetzlich mit der realen ESP32-Toolchain kompilieren und anschliessend zuerst nur die Kleinspannungs-Sensoren pruefen. SSR-Lastseite bis zur fachgerechten Pruefung ausser Betrieb lassen.
