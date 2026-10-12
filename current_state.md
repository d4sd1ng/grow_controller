# Current State — Grow Controller

**Stand:** 2026-10-12
**Arbeitsbranch:** `feature/ph-calibration-service-esp`
**Architektur:** ein ESP32 mit OLED, vier lokale SSR-Steuerausgaenge

## Hardware

| Pin | Funktion |
|---|---|
| GPIO21/22 | OLED und ADS1115 |
| GPIO23 | DS18B20 1-Wire |
| GPIO25/26/27 | 3x DHT22 — noch nicht verkabelt |
| GPIO13/14 | BH1750 — noch nicht verkabelt |
| GPIO17 | SSR1 Licht |
| GPIO18 | SSR2 Umluft |
| GPIO19 | SSR3 Abluft |
| GPIO33 | SSR4 Heizung |

ADS1115: A0 PH4502C ueber Spannungsteiler 10k/20k, A1 CQRobot TDS. Das vorhandene OLED wird gemeinsam mit dem ADS1115 am I2C-Bus betrieben.

Der einzelne ESP32 und die Sensorplatinen ziehen in das gemeinsame Controller-/Relaisgehaeuse. Netzein- und -ausgaenge bleiben in einer getrennten, fachgerecht installierten und geprueften Leistungssektion. Klimasensoren muessen ausserhalb des Gehaeuses in den Messzonen positioniert werden.

## Firmware-Stand

- `firmware/growroom_telemetry_client` schaltet die vier Kleinspannungs-SSR-GPIOs lokal, ohne Powerbox-ESP32 ueber WLAN abzufragen.
- Die CYD- und Server-API bleibt weitgehend kompatibel; der alte `power_node`-Status und dessen POST-Endpunkt sind entfallen.
- Bei ungueltiger DHT22-#1-Messung wird die Heizung gesperrt; unabhaengiger Uebertemperaturschutz ist weiterhin erforderlich.
- `firmware/ph_calibration_tool` dient als temporaerer Hardwaretest und pH-Kalibrierung **ueber ADS1115 A0**, mit TDS-Auslesung auf A1. OLED, DS18B20, DHT22 und BH1750 werden diagnostiziert.
- DHT22/BH1750 duerfen vor dem Anschluss als fehlend erscheinen. Vier SSR-Steuerausgaenge bleiben waehrend des Diagnosesketchs auf AUS.
- Die alte pH-Kalibrierung des ESP32-internen ADC bleibt in einem getrennten NVS-Namespace erhalten und wird nicht automatisch auf ADS1115-Werte angewendet.
- Alter `firmware/powerbox_actuator`-Sketch bleibt als Legacy im Repo und darf fuer den Einzel-ESP32-Aufbau nicht verwendet werden.

## Serverstand

FastAPI/Streamlit/SQLite-Zwischenstand unveraendert. PostgreSQL-Migration ist nach dem Hardware-Funktionstest als separater Schritt geplant. Die Sensordaten- und Aktor-Keys bleiben kompatibel, nur die Powerbox-Node-Meldung entfällt.

## Noch offen

1. Kalibriersketch und Hauptfirmware mit der lokalen Arduino-ESP32-Toolchain kompilieren.
2. Zuerst ADS1115 A0/A1, OLED und DS18B20 ueber den Hardware-Diagnosesketch pruefen.
3. Drei DHT22 und BH1750 verkabeln und dann die Diagnose erneut ausfuehren.
4. Spannungsgrenzen am ADS1115 und SSR-Steuereingangsstroeme elektrisch pruefen.
5. pH-7-/pH-4-Pufferkalibrierung der realen ADS1115-Messkette.
6. Steuerungs- und Sicherheitspruefungen vor jeder Inbetriebnahme mit Lasten.
7. PostgreSQL-Migration.
