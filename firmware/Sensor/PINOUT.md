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

### Steckverbindung DS18B20-Wassertemperatur

Auf der **Sensor-ESP32-Platine** wird ein eigener **3-poliger, abziehbarer
Anschluss TEMP-WASSER** für den wasserdichten DS18B20 vorgesehen:

| Kontakt | Signal | Verbindung |
| --- | --- | --- |
| 1 | 3V3 | DS18B20 VDD (rot, sofern Sensorleitung entsprechend beschriftet) |
| 2 | DATA | ESP32 GPIO23 (gelb, sofern entsprechend beschriftet) |
| 3 | GND | Gemeinsame Sensormasse (schwarz, sofern entsprechend beschriftet) |

Als steckbares System wird die auf der C3-Platine geplante
**Phoenix Contact MSTB 2,5 / 3-polig / 5,08 mm** Baureihe vorgemerkt
(Grundleiste 1757255, Gegenstecker 1757022).
Die Steckerkontakte werden auf der Platine eindeutig als 3V3 / DATA / GND
beschriftet; die tatsächlichen Aderfarben müssen vor Anschluss verifiziert
werden. Der 1-Wire-Pull-up **R_TEMP_WASSER = 4,7 kΩ** wird auf der Sensorplatine
**zwischen DATA (GPIO23) und 3,3 V** vorgesehen (nicht in Reihe zur
Datenleitung). Der Widerstand sitzt in der Nähe von `TEMP-WASSER`.
Bei der Inbetriebnahme sind Leitungslänge, Signalqualität und der
reale Sensor zu prüfen; ein früher verwendeter 20-kΩ-Widerstand ist
**nicht** der vorgesehene Bestückungswert. **Nicht** an die C3-SSR-/Pumpenplatine anschließen.

Die Aufnahme dieses Anschlusses ist eine Dokumentations-/PCB-Anforderung;
sie ändert weder die Firmware noch die bestehenden Kalibrierungen.

### Steckverbindungen DHT22 (Raumklima)

Auf der **Sensor-ESP32-Platine** werden drei **separate 3-polige,
abziehbare Steckanschlüsse** vorgesehen. Alle drei Anschlüsse haben
dieselbe Kontaktfolge, aber unterschiedliche DATA-GPIOs:

| Anschluss | Kontakt 1 | Kontakt 2 | Kontakt 3 |
| --- | --- | --- | --- |
| DHT-1 | 3,3 V | DATA → GPIO25 | GND |
| DHT-2 | 3,3 V | DATA → GPIO26 | GND |
| DHT-3 | 3,3 V | DATA → GPIO27 | GND |

Mechanisches Stecksystem wie beim Wassertemperatursensor:
**Phoenix Contact MSTB 2,5, 3-polig, Raster 5,08 mm**.
Für die DHT22 zusätzlich vorzusehen:

| Bauteil | Hersteller-Nr. | Menge |
| --- | --- | ---: |
| PCB-Grundleiste MSTBA 2,5/3-G-5,08 | 1757255 | 3 |
| Abziehbarer Gegenstecker MSTB 2,5/3-ST-5,08 | 1757022 | 3 |
| DATA-Pull-up 4,7 kΩ nach 3,3 V | Wert festgelegt, Bauform offen | 3 |

Die Pull-ups liegen jeweils **zwischen dem zugehörigen DATA-Pin und
3,3 V**, nicht in Reihe. Es sind drei eigenständige Datenleitungen.
Falls ein eingesetztes DHT22-Modul bereits einen Pull-up enthält, ist
die Bestückung vorab zu prüfen und gegebenenfalls anzupassen.
Die Pin-/Aderreihenfolge am realen Sensor ist vor dem Anschluss zu
verifizieren; die Tabelle ist die **Steckerbelegung auf der Platine**,
keine Behauptung über die Reihenfolge am jeweiligen Sensorgehäuse.

**Anschlussbudget Sensorplatine (nur die hier festgelegten
Temperatur-/Feuchtesensoren):** 1 × TEMP-WASSER + 3 × DHT22 =
**4 dreipolige Steckplätze** mit vier PCB-Grundleisten und
vier Gegensteckern. Die DHT22-Ergänzung ändert die vorhandene
Firmware-GPIO-Zuordnung nicht.

### Steckverbindung A02YYUW (Ultraschall-Füllstand)

Auf der **Sensor-ESP32-Platine** wird ein eigener **3-poliger,
abziehbarer Steckanschluss `LEVEL-A02`** vorgesehen. Die vorhandene
Firmware verwendet den Ultraschallsensor als **UART-Empfänger**:

| Platinenkontakt | Bezeichnung | Verbindung |
| --- | --- | --- |
| 1 | 3V3 | Versorgung A02YYUW, sofern das konkrete Sensormodul 3,3 V unterstützt |
| 2 | SENSOR-TX | A02YYUW TX → Sensor-ESP32 GPIO18 (UART1 RX) |
| 3 | GND | Gemeinsame Sensormasse |

Der **RX-Pin des A02YYUW wird in dieser Firmware nicht verwendet**;
er wird daher **nicht** auf den 3-poligen Stecker geführt. Vor Fertigung
sind Versorgungsspannung, UART-Ausgangspegel und der gewünschte
Betriebsmodus anhand des tatsächlich gelieferten A02YYUW zu verifizieren.
Insbesondere bei 5-V-Betrieb darf ein eventuell 5-V-führendes
TX-Signal **nicht ungeprüft** direkt an GPIO18 angeschlossen werden.

Für die Steckverbindung ist ebenfalls **Phoenix Contact MSTB 2,5,
3-polig, Raster 5,08 mm** vorgesehen:
- 1 × PCB-Grundleiste `1757255`
- 1 × abziehbarer Gegenstecker `1757022`

Die Kontaktfolge 3V3 / SENSOR-TX / GND ist die **Platinenbelegung**,
nicht die unbestätigte Kontaktreihenfolge am A02YYUW-Sensorkabel.
Für diesen UART-Anschluss ist **kein zusätzlicher Pull-up-Widerstand
vorgesehen**; die 4,7-kΩ-Pull-ups gelten für die drei DHT22-Datenleitungen
und den DS18B20-1-Wire-Bus.

**Sensor-Steckverbinder-Zwischenstand:** drei DHT22 + ein DS18B20
Wassertemperatur + ein A02YYUW = **fünf 3-polige Steckanschlüsse**.
Zusammen sind **vier Pull-up-Widerstände mit 4,7 kΩ** vorgesehen,
vorbehaltlich der Prüfung bereits vorhandener Modul-Pull-ups.

### Steckverbindung BH1750 (Lichtsensor)

Auf der **Sensor-ESP32-Platine** wird ein eigener **4-poliger, abziehbarer
Anschluss `LIGHT-BH1750`** vorgesehen. Der Sensor bleibt am separaten
I²C-Bus der bereits vorhandenen Firmware:

| Platinenkontakt | Signal | Verbindung |
| --- | --- | --- |
| 1 | 3V3 | Sensorversorgung, 3,3 V |
| 2 | GND | Gemeinsame Sensormasse |
| 3 | SDA | Sensor-ESP32 GPIO13 |
| 4 | SCL | Sensor-ESP32 GPIO14 |

Steckverbinder: **Phoenix Contact MSTB 2,5, 4-polig, Raster 5,08 mm**
(gleiche Steckerfamilie wie die anderen Sensoranschlüsse).
Die **exakte Artikelnummer für die vierpolige Grundleiste und den
Gegenstecker ist noch nicht anhand des Herstellerkatalogs verifiziert**
und wird deshalb nicht erfunden. Für das PCB wird ein 4-poliger
Steckplatz reserviert; sein Footprint ist erst nach Artikelbestätigung
verbindlich.

Für den I²C-Bus sind Pull-ups an SDA und SCL erforderlich. Ob diese
bereits auf dem tatsächlich verwendeten BH1750-Modul bestückt sind,
ist noch zu prüfen. **Keine zusätzlichen festen Pull-ups blind
parallel bestücken**; je nach vorhandener Beschaltung und Leitungslänge
müssen sie gezielt ausgelegt werden.

Die tatsächliche Kontaktreihenfolge am BH1750-Modul ist vor dem Anschluss
mit der oben genannten **Platinenbelegung** abzugleichen. Ein eventuell
vorhandener ADDR-Pin des Moduls bleibt mit der bestehenden
Adresskonfiguration separat zu prüfen.

**Aktualisiertes Anschlussbudget der hier dokumentierten Sensoren:**
5 × 3-polig (3 DHT22, 1 DS18B20-Wasser, 1 A02YYUW) sowie
1 × 4-polig (BH1750). Weiterhin 4 × 4,7-kΩ-Pull-up fest
vorgesehen (3 DHT22 und 1 DS18B20); die I²C-Pull-ups sind
noch abhängig vom realen Modul zu verifizieren.

### Steckanschlüsse und Eingangsbeschaltung für pH und EC/TDS

Auf der **Sensor-ESP32-Platine** zwei getrennte, abziehbare Anschlüsse
der vorgesehenen Phoenix-Contact-MSTB-2,5-Familie (Raster 5,08 mm):
**pH vierpolig**, **EC/TDS dreipolig**.

Vom Nutzer bestätigte Kontaktbezeichnungen am pH-Modul:
**VCC, GND, GND, PO**.

| Anschluss | Kontakt | Signal | Weiterführung |
| --- | ---: | --- | --- |
| PH-4502C | 1 | VCC | 5-V-Modulversorgung, vor Anschluss verifizieren |
| PH-4502C | 2 | GND 1 | Massekontakt 1, interne Verbindung noch ungeprüft |
| PH-4502C | 3 | GND 2 | Massekontakt 2, interne Verbindung noch ungeprüft |
| PH-4502C | 4 | PO | Analogsignal zum geplanten A0-Eingangskreis |
| EC-TDS | 1 | 5 V | Modulversorgung |
| EC-TDS | 2 | GND | Masse |
| EC-TDS | 3 | Analogsignal | Zum ADS1115 A1 |

Die zwei GND-Kontakte am pH-Modul bleiben im Plan ausdrücklich
**als separate Kontakte** dokumentiert. Ob sie intern direkt miteinander
verbunden sind und wie sie auf der Platine angeschlossen werden,
ist vor der Freigabe am konkreten Modul zu überprüfen.
Die obige Kontaktfolge ist die **vorgesehene Steckverbinderbelegung**;
die reale Steckerorientierung und Pinreihenfolge sind noch zu prüfen.
PH4502C DO/TO werden von der Firmware nicht ausgewertet.

**ADS1115 A0:** Im ausdrücklich als **veraltet** gekennzeichneten
`wiring_plan_v2.md` ist ein Spannungsteiler **10 kΩ vom
PH4502C-PO-Signal zum A0-Knoten und 20 kΩ von A0 nach GND**
dokumentiert. Für die neue Platine werden deshalb **zwei
Widerstandspositionen R_PH_OBEN/R_PH_UNTEN** reserviert. Ihre
Bestückungswerte werden erst nach Abgleich des tatsächlich
betriebenen Aufbaus freigegeben. **Änderungen des Teilungsverhältnisses
beeinflussen die gespeicherte pH-Kalibrierung.**

**ADS1115 A1:** Das bisher dokumentierte CQRobot-TDS-Analogsignal
wurde **ohne Spannungsteiler** an A1 geführt; ein fester
Widerstandswert für A1 ist aktuell **nicht bestätigt**.
Signalpegel und gegebenenfalls benötigter Eingangsschutz müssen
elektrisch geprüft werden. Der ADS1115 wird mit 3,3 V betrieben;
ein eingestellter Messbereich von ±4,096 V macht seine Eingänge
nicht 5-V-tolerant.

**Steckverbinder-Budget dieser Erweiterung:** pH erhält
**1 × 4-polig** (genaue vierpolige MSTB-Artikelnummer noch zu prüfen),
EC/TDS erhält **1 × 3-polig** (Grundleiste `1757255`,
Gegenstecker `1757022`).
Damit sind bei den hier dokumentierten Sensoren insgesamt
**6 × 3-polig** (3 × DHT22, 1 × DS18B20, 1 × A02YYUW, 1 × EC/TDS)
plus **2 × 4-polig** (BH1750, pH) vorgesehen. Hinzu kommen vier
bisher festgelegte 4,7-kΩ-Pull-ups (DHT22 und DS18B20); die
pH-Teilerwerte und die I²C-Pull-ups sind separat zu verifizieren.

Das Sensor-Board hat **kein lokales Display**. Die Messwerte erscheinen im seriellen Monitor und über die Server-API auf dem separaten LVGL-Remote.

Versorgung und Masse sowie physische Pinpositionen sind anhand des tatsächlich verbauten Pro-Boards zu verifizieren. Keine Netzspannungs-/SSR-Lastverkabelung am Sensorcontroller.

## Verifizierung

- [x] Software-GPIOs und Sensorzuordnungen geprüft
- [x] Alte lokale Display-Ansteuerung aus Sensor-Firmware entfernt
- [ ] Reale Pinpositionen des Pro-Boards geprüft
- [ ] Arduino-Kompilierung erfolgreich
- [ ] Serieller Messwerttest und LVGL-Remote-Test an realer Hardware erfolgreich
