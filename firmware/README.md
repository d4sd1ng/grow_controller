# Grow Controller — Firmware fuer einen ESP32

## Aktive Geraete

| Sketch | Geraet | Funktion |
|---|---|---|
| `growroom_telemetry_client` | einziger ESP32 (mit OLED) | 3x DHT22, 2x DS18B20, ADS1115 pH/TDS, BH1750, vier lokale SSR-Steuerausgaenge, AUTO/MANUAL, CYD-API, Telemetrie |
| `ph_calibration_tool` | derselbe ESP32, temporär geflasht | Hardwarediagnose aller Sensortypen und pH-7-/pH-4-Kalibrierung ueber ADS1115 A0 |
| `ds18b20_diagnostic` | derselbe ESP32, temporär geflasht | 1-Wire-Diagnose auf GPIO23 |
| `cyd_remote_control` | vorhandenes CYD | primaere Fernbedienung |
| `cyd_grow_display` | dasselbe CYD, alternative Firmware | alternative LVGL-Oberflaeche |
| `powerbox_actuator` | **LEGACY — nicht mehr einsetzen** | alter zweiter ESP32; nicht Teil der Ein-ESP32-Architektur |

## Kleinspannungs-Pins

| GPIO | Funktion |
|---:|---|
| 21/22 | OLED und ADS1115 |
| 23 | DS18B20-Datenbus |
| 25/26/27 | DHT22 #1/#2/#3 |
| 13/14 | BH1750 separater I2C-Bus |
| 17 | SSR1 Licht |
| 18 | SSR2 Umluft |
| 19 | SSR3 Abluft |
| 33 | SSR4 Heizung |

Vollstaendig: [../wiring_plan_v2.md](../wiring_plan_v2.md).

## Kalibrier-/Diagnosesketch

Arduino Serial Monitor **115200 Baud**:
- `t`: vollstaendige Hardwarediagnose
- `7`: stabilen pH-7-Pufferpunkt ueber ADS1115 A0 speichern
- `4`: stabilen pH-4-Pufferpunkt ueber ADS1115 A0 speichern
- `s`: pH-Kalibrierpunkte und Konstanten fuer die produktive Firmware anzeigen
- `r`: nur die neue ADS-Kalibrierung im NVS (`phads_v1`) loeschen
- `h`: Hilfe

ADS1115 A0=pH, A1=TDS; OLED und ADS1115 auf I2C GPIO21/22. Das alte Kalibrierschema ueber VP bleibt in einem **getrennten NVS-Namespace** unberuehrt; die alte VP-Kalibrierung wird nicht unbemerkt fuer ADS-Werte verwendet. Kalibrierpunkte benoetigen eine gueltige DS18B20-Temperatur. Bei fehlendem ADS1115 oder unruhiger Messung wird die Erfassung abgelehnt.

Die drei DHT22 und der BH1750 sind bisher nicht verkabelt. Sie duerfen in der Diagnose als nicht erkannt angezeigt werden, ohne die anderen Tests zu blockieren. SSR-Ausgaenge bleiben im Diagnosesketch immer LOW.

## Lokale HTTP-API, Port 8081

| Methode | Pfad | Zweck |
|---|---|---|
| GET | `/api/local/summary` | Livewerte, Modus, Sollwerte und lokale Ausgangszustaende |
| GET | `/api/local/actuators` | API-Kompatibilitaet: Soll-/Ist-GPIO-Zustaende |
| POST | `/api/local/control/targets` | `light`/`heater`/`circulation_vent`/`exhaust_vent`, MANUAL |
| POST | `/api/local/control/mode` | `auto`/`manual` |

Ein separater HTTP-Power-Knoten existiert nicht mehr. Der Heizkanal wird bei ungueltiger Klimamessung gesperrt. Eine unabhaengige physische Temperaturbegrenzung ist trotzdem notwendig.

## Benoetigte Arduino-Bibliotheken

Adafruit ADS1X15, Adafruit GFX, Adafruit SSD1306, DHT sensor library, OneWire, DallasTemperature, ArduinoJson und ESP32 Arduino Core. Fuer das CYD zudem die Bibliotheken der gewaehlten CYD-Variante.

## Konfiguration und Pruefung

Im Projektroot `python export_firmware_secrets_from_env.py` ausfuehren, um `device_secrets.h` fuer den Einzel-ESP32-Sketch und CYD zu erzeugen (Dateien sind git-ignoriert). Keine WLAN-Passwoerter in Git committen.

Vor dem Flashen muss die neue Firmware mit der lokal installierten Arduino-ESP32-Toolchain kompiliert werden. Danach die Kleinspannungs-Sensorik testen. Die SSR-Lastseite bleibt bis zur fachgerechten Pruefung ausser Betrieb.
