# Grow Controller

Grow-Controller fuer Indoor-Salat mit **zwei ESP32**: Sensor-ESP32 mit integriertem OLED auf **26×30-Lochraster** und separater **ESP32-C3 SuperMini** als geplanter Leistungscontroller. Das Cheap Yellow Display (CYD) dient als Fernbedienung, Raspberry Pi als Telemetrie-/Dashboard-Server.

**Aktuelle Anschluesse:** [wiring_plan_v2.md](wiring_plan_v2.md) (**Wiring Plan V2, fünf Relais, Sensorplatine 26×30**) · **Firmware:** [firmware/README.md](firmware/README.md) · **Status:** [current_state.md](current_state.md)

## Architektur

```text
pH / EC / DHT22 / DS18B20 / BH1750 / MLX90640
                   |
                   v
     Sensor-ESP32 mit OLED und ADS1115
     OLED+ADS+MLX: D21/D22 | BH: D13/D14 | DS: D23
       (26 x 30 Lochraster, KEINE Relais)
                   |
             WLAN / API
                   |
    +--------------+---------------+
    |              |               |
Raspberry Pi    CYD-Fernanzeige    ESP32-C3 SuperMini
                                  (getrennter Leistungscontroller)
```

**Wiring-Status:** Am ADS1115 **A0 den bestätigten Spannungsteiler beibehalten: PH4502C PO → 10 kΩ → A0; A0 → 20 kΩ → GND**. Die bestehende pH-Kalibrierung bezieht sich auf die ADS-A0-Spannung. EC/TDS an A1 ohne diesen Teiler; die zulässigen ADS-Eingangspegel prüfen. Auf der 26×30-Platine sind eigene Anschlüsse für BH1750 (5-polig, inkl. ADDR), MLX90640 (4-polig, VCC/VIN erst nach Modulprüfung) und DS18B20 (D23 mit 4,7-kΩ-Pull-up nach 3V3) vorgesehen. D21/D22 sind der ursprüngliche gemeinsame ADS1115-/OLED-I²C-Bus (mit geplanter MLX90640); D13/D14 bleiben ausschließlich für den BH1750. RGB-Grün auf D4 (440 Ω) statt des ausgefallenen D32, RGB-Rot auf D33 (440 Ω), **Phasentaster 1 auf D19 gegen GND** (in beiden Sketchen jetzt Interrupt-Abfrage gegen übersehene Tastendrücke) und Buzzer-Steuerreserve auf D16.

**Wichtig:** Der separate ESP32-C3 soll **fünf Relais** ansteuern: **Licht für beide Lampen gemeinsam, Abluft, Umluft, Heizung und geplante Filter-/Umwälzpumpe**. Für die Pumpe kommt **Taster 2 am ESP32-C3 IO6/GPIO6 gegen C3-GND** hinzu (`INPUT_PULLUP`, kein externer Widerstand; C3-Firmware noch nicht umgesetzt): **einmal drücken = Pumpe AUS/Wartungssperre, erneut drücken = Pumpe EIN/normaler Pumpenbetrieb**. Die automatische Pumpensteuerung muss während Wartung gesperrt bleiben. Ein Taster/Softwaremodus ist keine sichere Netztrennung für Reinigungsarbeiten. Die Sensor-Firmware enthält keine direkten Relais-GPIOs; die C3-Anbindung ist noch nicht implementiert und Aktoren werden derzeit **nicht** angesteuert. Die Netzspannungsseite bedarf einer fachkundig geprüften, separaten Installation.


## Geplantes zentrales Bedienfeld

Die vorhandene **4×4-Matrix-Tastatur** (0–9, A–D, *, #) wird **nur für später vorgemerkt**, da aktuell eine Befestigungsmöglichkeit fehlt. Nach geeigneter Montage kann sie als **Bedienfeld für den gesamten Grow-Controller** dienen: Aktoren, pH/EC, Klima, Wasser, Woche/Phase, Sollwerte, Zeitpläne, Alarme und Kalibrierungsmenüs. Geplant ist die Anbindung über einen **PCF8574-I²C-Expander an den ESP32-C3**, mit Menü-/Statusanzeige auf dem **CYD**. Die **A-Taste** soll die Pumpe wie gewünscht mit aufeinanderfolgenden Tastendrücken AUS/EIN schalten. Der **separate Pumpen-Drucktaster bleibt in der aktuellen Planung**; A ist allenfalls eine spätere zusätzliche Bedienmöglichkeit. **Keine Montage, Verdrahtung, Pinreservierung oder Firmware-Implementierung für die Tastatur, solange die Befestigung ungeklärt ist.** Tastatur, I²C-Pins, C3-Firmware und Protokoll sind nicht umgesetzt. Details: [wiring_plan_v2.md](wiring_plan_v2.md).

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
