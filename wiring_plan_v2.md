# Grow Controller — Wiringplan V3 (ein ESP32)

**Stand:** 2026-10-12. Dieser Plan ersetzt den vorherigen Zwei-ESP32-Aufbau.
**Status:** Firmware-Umstellung vorgenommen; Kompilier- und Hardwaretests ausstehend.

Dieser Plan dokumentiert die **Kleinspannungs-Sensor- und Steuersignale**. Die 230-V-Lastseite ist kein Bestandteil dieser Anschlussanleitung. Netzspannungsinstallationen gehören in einen geeigneten, berührungssicheren und fachkundig geprüften Leistungsbereich.

## Architektur

```text
Ein ESP32 mit integriertem OLED (gemeinsames Controllergehaeuse)
  ├── OLED + ADS1115: I2C GPIO21 SDA / GPIO22 SCL
  │     ├── ADS A0: PH4502C PO ueber Spannungsteiler
  │     └── ADS A1: CQRobot TDS-Analogsignal
  ├── DS18B20: 1-Wire GPIO23 (bis zu zwei Sensoren)
  ├── DHT22 #1/#2/#3: GPIO25/26/27 (noch nicht verkabelt)
  ├── BH1750: I2C GPIO13/14 (noch nicht verkabelt)
  ├── SSR1: GPIO17, Licht
  ├── SSR2: GPIO18, Umluft
  ├── SSR3: GPIO19, Abluft
  └── SSR4: GPIO33, Heizstrahler
        |
        +-- WLAN <--> Cheap Yellow Display (CYD)
        +-- WLAN ----> Raspberry Pi (API, Dashboard, Telemetrie)
```

Kein zweiter ESP32, kein ATmega2560 und keine Datenverbindung zwischen zwei Steuerplatinen. Eine Wasserpumpe ist nicht Bestandteil des aktuellen Aufbaus und wird nicht als Relaiskanal eingeplant.

## GPIO-Belegung

| Board-Pin | GPIO | Funktion |
|---|---:|---|
| D21 | 21 | internes OLED SDA und ADS1115 SDA |
| D22 | 22 | internes OLED SCL und ADS1115 SCL |
| D23 | 23 | DS18B20 DATA / gemeinsamer 1-Wire-Bus |
| D25 | 25 | DHT22 #1, Pflanzenzone |
| D26 | 26 | DHT22 #2, Unter-/Zuluftzone |
| D27 | 27 | DHT22 #3, Ober-/Abluftzone |
| D13 | 13 | BH1750 SDA, separater I2C-Bus |
| D14 | 14 | BH1750 SCL, separater I2C-Bus |
| TX2 / D17 | 17 | SSR1, Licht (Steuereingang) |
| D18 | 18 | SSR2, Umluft (Steuereingang) |
| D19 | 19 | SSR3, Abluft (Steuereingang) |
| D33 | 33 | SSR4, Heizstrahler (Steuereingang) |
| D32 | 32 | Reserve |
| VP / VN | 36 / 39 | nur analoge Eingangsreserve |

## Sensoranschluesse (Kleinspannung)

### Bereits verkabelt / noch elektrisch zu pruefen

**ADS1115:** VDD 3,3 V; GND gemeinsame Kleinspannungsmasse; SDA GPIO21; SCL GPIO22; ADDR GND fuer I2C-Adresse 0x48; A0 pH; A1 TDS. Das integrierte SSD1306-OLED nutzt dieselben I2C-Leitungen, normalerweise Adresse 0x3C.

**PH4502C:** 5 V und GND; PO -> 10 kOhm -> ADS A0; A0 -> 20 kOhm -> GND. Nominales Teilungsverhaeltnis: 2/3. **Vor dem Anschluss des analogen ADS-Eingangs** sicherstellen, dass die reale Spannung am Teilerknoten im zulaessigen Eingangsbereich des mit 3,3 V versorgten ADS1115 bleibt. Die 3,33 V, die bei idealisierten 5 V am PO rechnerisch entstehen, duerfen nicht als normaler erlaubter Arbeitsbereich angenommen werden.

**CQRobot CQRSENTDS01:** Versorgung gemaess Modulvorgabe, gemeinsames GND, analoges Signal an ADS A1. Auch diesen Eingang vor Anschluss auf zulaessige Spannungen pruefen. Ein Kalibrierwert fuer EC liegt noch nicht vor.

**DS18B20:** DATA GPIO23, VDD 3,3 V, GND, ein gemeinsamer 4,7-kOhm-Pull-up zwischen DATA und 3,3 V. Bei zwei Sensoren beide DATA-Leitungen auf denselben 1-Wire-Bus; ROM-Zuordnung spaeter messen. Die pH-Kalibrierung erwartet mindestens einen funktionierenden DS18B20.

### Noch NICHT verkabelt

**3 x DHT22:** je 3,3 V, GND und separater DATA-Pin:
- DHT22 #1: DATA -> GPIO25, Pflanzenzone ca. 10–15 cm oberhalb der Blatthoehe
- DHT22 #2: DATA -> GPIO26, Unter-/Zuluftzone nicht im direkten Luftstrom
- DHT22 #3: DATA -> GPIO27, obere Abluft-/Lampenzone nicht im direkten Saugstrom

Je Sensorleitung 4,7 kOhm von DATA nach 3,3 V, sofern auf dem verwendeten DHT22-Modul kein geeigneter Pull-up bereits verbaut ist. Die Sensoren muessen **ausserhalb des warmen Controllergehaeuses** messen.

**BH1750:** VCC 3,3 V; GND; SDA GPIO13; SCL GPIO14; ADDR nach Modul-Default (normalerweise 0x23), separater I2C-Bus mit anfaenglich 50 kHz. Sensor bei den Pflanzen mit freiem Blick zur Beleuchtung positionieren; bei laengerer Leitung geeignete paarweise gefuehrte Signalleitungen verwenden.

Beide bisher fehlenden Sensortypen sind optional im Diagnosesketch: bis zum Anschluss melden sie „NICHT ERKANNT“ und blockieren die vorhandenen Tests nicht.

## Vier SSR-Steuersignale

| Kanal | ESP32-Pin | Funktion |
|---|---|---|
| SSR1 | GPIO17 | Licht |
| SSR2 | GPIO18 | Umluft links und rechts |
| SSR3 | GPIO19 | Abluft |
| SSR4 | GPIO33 | Heizstrahler |

Vorgesehene Logik: LOW AUS, HIGH EIN. Externe 10-kOhm-Pulldowns am Steuereingang verhindern einen undefinierten Zustand vor Initialisierung der GPIOs. Die tatsaechliche erforderliche SSR-Eingangsstromstaerke und zuverlaessige 3,3-V-Ansteuerung sind zu verifizieren; die Beschriftung „3–32 VDC Input“ allein reicht nicht aus.

Beim **Hardware-Diagnosesketch** bleiben saemtliche vier Ausgaenge AUS. Es erfolgt **kein Last-/Netzspannungstest**. Fuer die Heizlast ist zusaetzlich eine unabhaengige Temperaturbegrenzung erforderlich.

## Software / Teststand

- `firmware/growroom_telemetry_client/growroom_telemetry_client.ino`: einzelner ESP32; Sensorik + vier lokale SSR-Ausgaenge; CYD-API und Telemetrie weiterhin vorhanden.
- `firmware/ph_calibration_tool/ph_calibration_tool.ino`: Hardwaretest OLED, ADS A0/A1, DS18B20 und spaeter DHT22/BH1750; pH 7/4 ueber ADS A0 mit NVS-Speicherung `phads_v1`.
- `firmware/ds18b20_diagnostic/ds18b20_diagnostic.ino`: separates Diagnosetool, GPIO23.
- `firmware/powerbox_actuator/`: **Legacy, nicht mehr flashen**, da kein zweiter ESP32 vorhanden ist.
- CYD-Firmware und Server folgen weiterhin dem bestehenden API-Schema; die alten separaten Power-Node-Meldungen werden nicht mehr benoetigt.
- Der Server verwendet derzeit noch SQLite; PostgreSQL-Migration bleibt ein separater Schritt.

**Noch nicht geprueft:** Kompilierung unter der lokalen Arduino-ESP32-Toolchain, Sensorwerte, ADS-Grenzspannungen, tatsaechlicher Relais-Eingangsstrom, Ein-/Ausschaltverhalten und reale pH-Kalibrierung. Die 230-V-Seite bleibt bis zur fachgerechten Pruefung ausser Betrieb.
