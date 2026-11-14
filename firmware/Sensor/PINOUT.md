# Sensor-ESP32 Pro — GPIO-Belegung (ohne lokales Display)

**Firmware:** `firmware/Sensor/Sensor.ino`. Diese Liste beschreibt die GPIO-Zuordnung im Firmwarestand; die Reihenfolge der physischen Kontakte des neu vorgesehenen Pro-Boards ist nicht verifiziert. Die vorher dokumentierten Kontaktpositionen bezogen sich auf ein anderes Board und gelten hier nicht als Anschlussplan.

| Funktion | GPIO |
| --- | ---: |
| DHT22 #1 DATA | 25 |
| DHT22 #2 DATA | 26 |
| DHT22 #3 DATA | 27 |
| DS18B20 DATA (1-Wire) | 23 |
| ADS1115 SDA (I²C) | 21 |
| ADS1115 SCL (I²C) | 22 |
| BH1750 SDA (separater I²C) | 13 |
| BH1750 SCL (separater I²C) | 14 |
| RGB Rot | 33 |
| RGB Grün | 4 |
| RGB Blau | 17 |
| Wuchs-/Blüte-Taster | 16 |
| Pumpen-Wartungstaster | 19 |
| Buzzer (reserviert/deaktiviert) | 5 |

**ADS1115:** Adresse 0x48, A0 = pH, A1 = TDS/EC. ADS1115 und eine optionale MLX90640 verwenden GPIO21/22, nicht das lokale Display.

**DS18B20:** 1-Wire auf GPIO23. Für die Wassertemperatur ist ein wasserdichter Sensor vorgesehen. Der Pull-up liegt zwischen DATA und 3,3 V.

**BH1750:** Separater I²C-Bus mit GPIO13/14.

Das Sensor-Board hat **kein lokales Display**. Die Messwerte erscheinen im seriellen Monitor und über die Server-API auf dem separaten LVGL-Remote.

Versorgung und Masse sowie physische Pinpositionen sind anhand des tatsächlich verbauten Pro-Boards zu verifizieren. Keine Netzspannungs-/SSR-Lastverkabelung am Sensorcontroller.

## Verifizierung

- [x] Software-GPIOs und Sensorzuordnungen geprüft
- [x] Alte lokale Display-Ansteuerung aus Sensor-Firmware entfernt
- [ ] Reale Pinpositionen des Pro-Boards geprüft
- [ ] Arduino-Kompilierung erfolgreich
- [ ] Serieller Messwerttest und LVGL-Remote-Test an realer Hardware erfolgreich
