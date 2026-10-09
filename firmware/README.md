# ESP32-Firmware — Hardware V2

## Rollen

| Sketch | Gerät | Aufgabe |
|---|---|---|
| `growroom_telemetry_client` | Sensor-ESP | Sensoren lesen, AUTO/MANUAL-Ziele halten, OLED, lokale API, Telemetrie |
| `powerbox_actuator` | Stromkasten-ESP | vier SSR-Eingänge schalten, Sollwerte direkt vom Sensor-ESP holen, Failsafe |
| `cyd_remote_control` | CYD | primäre 3-Panel-Fernbedienung; Live/Control direkt zum Sensor-ESP, Historie/Alerts vom Server |
| `cyd_grow_display` | CYD | alternative kompakte LVGL-Oberfläche |
| `ph_calibration_tool` | Ersatz-ESP | pH-7-/pH-4-Kalibrierung direkt ueber den internen ESP32-ADC; kein ADS1115 am Servicegeraet |

Es gibt nur **ein CYD**; `cyd_remote_control` und `cyd_grow_display` sind alternative Firmwarevarianten für dieses Gerät.

## Sensor-ESP lokale API

Port: `8081`

| Methode | Pfad | Zweck |
|---|---|---|
| GET | `/api/local/summary` | Livewerte, Modus, Ziele, Power-Node-Status |
| GET | `/api/local/actuators` | Sollzustände für Stromkasten-ESP |
| POST | `/api/local/control/targets` | Teilupdate LIGHT/HEAT/CIRC/EXH, setzt MANUAL |
| POST | `/api/local/control/mode` | `auto` / `manual` |
| POST | `/api/local/actuator-status` | Rückmeldung des Stromkasten-ESP |

Der Header `X-Telemetry-Key` wird verwendet, wenn `TELEMETRY_API_KEY` gesetzt ist.

## Bibliotheken

### Sensor-ESP

- Adafruit ADS1X15
- Adafruit GFX
- Adafruit SSD1306
- DHT sensor library
- OneWire
- DallasTemperature
- ArduinoJson
- ESP32 Arduino Core

### pH-Servicegeraet

- Adafruit GFX
- Adafruit SSD1306
- OneWire
- DallasTemperature
- ESP32 Arduino Core
- kein ADS1115

### CYD Remote

- TFT_eSPI
- XPT2046_Touchscreen
- ArduinoJson

### CYD LVGL

- LVGL v8
- TFT_eSPI
- XPT2046_Touchscreen
- ArduinoJson

### Stromkasten-ESP

- ArduinoJson
- ESP32 Arduino Core

## Secrets

Im Projektroot:

```bash
python export_firmware_secrets_from_env.py
```

Dadurch wird in jedem relevanten Sketchordner eine git-ignorierte `device_secrets.h` erzeugt.

Benötigte Werte:

- `IOT_WIFI_SSID`
- `IOT_WIFI_PASSWORD`
- `IOT_TELEMETRY_SERVER_HOST`
- `TELEMETRY_API_KEY`
- optional Server-Port/HTTPS-Parameter

## Aktoren

Nur diese vier Keys sind gültig:

- `light`
- `heater`
- `circulation_vent`
- `exhaust_vent`

Die Wasserpumpe wird nicht gesteuert.

## pH-Servicegerät

`ph_calibration_tool.ino` benoetigt keinen ADS1115. Der PH4502C-PO-Ausgang geht ueber denselben 10k/(2x10k)-Spannungsteiler direkt auf D4 des Ersatz-ESP. Ein DS18B20 auf D18 misst die Temperatur; jeder pH-7-/pH-4-Punkt wird nur mit gueltiger Temperatur angenommen und zusammen mit dieser im NVS gespeichert. Das integrierte SSD1306-OLED auf GPIO21/22 zeigt Temperatur, ADC-/PO-Spannung, Streuung, Kalibrierstatus und nach zwei Punkten den berechneten pH-Wert.

Serial Monitor: 115200 Baud.

- `7` = stabilen pH-7-Punkt mitteln und dauerhaft im ESP32-NVS speichern
- `4` = stabilen pH-4-Punkt mitteln und dauerhaft im ESP32-NVS speichern
- `s` = Status und direkt übernehmbare `PH_CAL_VOLT_4`/`PH_CAL_VOLT_7`-Konstanten ausgeben
- `r` = gespeicherte Punkte aus RAM und NVS löschen
- `h` = Hilfe

Messungen mit mehr als 10 mV Standardabweichung oder ohne gueltige DS18B20-Temperatur werden verworfen und nicht gespeichert. Nach einem Neustart laedt das Servicegeraet die zuletzt gespeicherten Kalibrierpunkte samt Temperatur automatisch wieder. Eine automatische Korrektur des Soll-pH der Pufferloesung wird nicht vorgenommen, weil dafuer die Temperaturtabelle des konkret verwendeten Pufferprodukts erforderlich ist.

## Vor produktivem Flashen

Jeden tatsächlich verwendeten Sketch mit der installierten ESP32-/Arduino-Toolchain kompilieren. Danach zuerst Kleinspannungsseite und WLAN-Kommunikation testen.
