# Sensor-ESP32 — Pinout (verbindliche letzte dokumentierte Zuordnung)

**Stand:** Abgleich 13.11.2026 mit `Sensor.ino` und der späteren, nutzerkorrigierten Pinout-Tabelle vom 21.10.2026. Die GPIO-Nummern sind **nicht** die physischen Kontaktnummern. Das tatsächliche angeschlossene Board wurde bei diesem Abgleich nicht neu fotografisch oder elektrisch geprüft. Nicht blind flashen, bevor die realen Leitungen kontrolliert sind.

**Boardansicht:** integriertes OLED sichtbar, USB **unten**, Kontakte auf jeder Seite **von oben** zählen (1–15).

| Funktion | Aufdruck | GPIO | Board-Kontakt |
| --- | --- | ---: | --- |
| DHT22 #1 DATA | D25 | 25 | links 8 |
| DHT22 #2 DATA | D26 | 26 | links 9 |
| DHT22 #3 DATA | D27 | 27 | links 10 |
| DS18B20 DATA | D23 | 23 | rechts 1 |
| ADS1115 & OLED SDA | D21 | 21 | rechts 5 |
| ADS1115 & OLED SCL | D22 | 22 | rechts 2 |
| BH1750 SDA | D13 | 13 | links 13 |
| BH1750 SCL | D14 | 14 | links 11 |
| RGB Rot | D33 | 33 | links 7 |
| RGB Grün | D4 | 4 | rechts 11 |
| RGB Blau | TX2 / D17 | 17 | rechts 9 |
| Wuchs-/Blüte-Taster | RX2 / D16 | 16 | rechts 10 |
| Pumpen-Wartungstaster | D19 | 19 | rechts 6 |
| Buzzer (reserviert/deaktiviert) | D5 | 5 | rechts 8 |

**ADS1115:** I²C-Adresse 0x48; A0 = pH-Eingang, A1 = TDS/EC-Eingang; beide sind **ADS-Kanäle**, keine ESP32-GPIOs. **OLED:** I²C-Adresse 0x3C. **BH1750:** separater I²C-Bus mit SDA13/SCL14. **DS18B20:** gemeinsamer 1-Wire-Bus GPIO23.

**Belegte physische Kontakte:** Die Liste benutzt die aufgedruckten Board-Bezeichnungen. Strom-/GND-Anschlüsse und die Kleinspannungsversorgung müssen separat anhand des realen PCB-Aufdrucks kontrolliert werden. Keine Netzspannungs-/SSR-Lastverkabelung am Sensor-ESP32.

## Warum es widersprüchliche ältere Angaben gab

Die vorangegangene Ausgabe einer Pinliste nannte **Wuchs/Blüte GPIO19, Pumpentaster GPIO18, Buzzer GPIO16** und zählte teils die physischen Boardkontakte verkehrt herum. Diese Werte wurden in späteren Nutzerkorrekturen auf **GPIO16/GPIO19/GPIO5** umgestellt. `wiring_plan_v2.md` ist älter (06.10.2026) und nennt für DS18B20 noch **GPIO32**. `wiring_instructions.md` beschreibt eine noch ältere, nicht mehr zu dieser Hardware passende Gerätearchitektur mit DHT auf GPIO36/39 und gehört **nicht** mehr zur aktiven Anleitung.

## Verifizierung

- [x] Aktuelle Software-`PIN_*`-Definitionen mit der letzten Pinout-Tabelle abgeglichen
- [x] Softwareverwendung der Konstanten kontrolliert (`Wire.begin`, `bhWire.begin`, `pinMode`, DHT und OneWire)
- [ ] Reale Kabelverbindungen an den beschriebenen Kontakten kontrolliert
- [ ] Arduino-Kompilierung vollständig erfolgreich
- [ ] Sensor-/Taster-Test auf dem echten ESP32 abgeschlossen

**Die letzten drei Punkte sind weiterhin offen.** Ein erfolgreicher Compile bestätigt keine Verdrahtung.
