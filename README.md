# Grow Controller

Grow-Controller fuer Indoor-Salat mit **zwei ESP32**: Sensor-ESP32 mit integriertem OLED auf **26×30-Lochraster** und separater **ESP32-C3 SuperMini** als geplanter Leistungscontroller. Das Cheap Yellow Display (CYD) dient als Fernbedienung, Raspberry Pi als Telemetrie-/Dashboard-Server.

**Aktuelle Anschluesse:** [wiring_plan_v2.md](wiring_plan_v2.md) (**Zwei-ESP32-Plan V4, Sensorplatine 26×30**) · **Firmware:** [firmware/README.md](firmware/README.md) · **Status:** [current_state.md](current_state.md)

## Architektur

```text
pH / EC / DHT22 / DS18B20 / BH1750
                   |
                   v
     Sensor-ESP32 mit OLED und ADS1115
       (26 x 30 Lochraster, KEINE Relais)
                   |
             WLAN / API
                   |
    +--------------+---------------+
    |              |               |
Raspberry Pi    CYD-Fernanzeige    ESP32-C3 SuperMini
                                  (getrennter Leistungscontroller)
```

**Wichtig:** Die Sensor-Firmware ist nun frei von direkter GPIO-Relaissteuerung. Die C3-Anbindung ist noch ausstehend; die Aktoren werden derzeit **nicht** angesteuert. Die 230-V-Seite gehoert in eine fachkundig gepruefte, separate Installation.


## Firmware

- `firmware/growroom_telemetry_client/` — Sensor-ESP32; pH/EC/Klima, OLED, API/Telemetrie, **keine SSR-GPIO-Ausgaenge**. C3-Anbindung steht noch aus.
- `firmware/ph_calibration_tool/` — temporaerer Hardware-/pH-Kalibriersketch auf demselben ESP32 (SSR AUS).
- `firmware/ds18b20_diagnostic/` — optionaler 1-Wire-Testscketch.
- `firmware/cyd_remote_control/` — CYD-Fernbedienung.
- `firmware/cyd_grow_display/` — alternative CYD-Firmware.
- `firmware/powerbox_actuator/` — alter Powerbox-Sketch, **nicht als getestete ESP32-C3-Firmware verwenden**.

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
