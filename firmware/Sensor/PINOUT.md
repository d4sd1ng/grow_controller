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

**ADS1115 A0:** Das vom Nutzer beschriebene Messsignal des bisherigen
Aufbaus wird **direkt von PH4502C PO nach ADS1115 A0** übernommen.
Der 10-kΩ/20-kΩ-Spannungsteiler in `wiring_plan_v2.md` war eine
unbestätigte, ausdrücklich korrigierte Annahme und ist **nicht** Teil
der neuen Sensorplatine. Es werden **keine R_PH_OBEN/R_PH_UNTEN**
bestückt oder geroutet. Die vorhandene Kalibrierung/Firmware bleibt
unverändert; Analogpegel und reale Funktion sind bei der Inbetriebnahme
der neuen PCB zu testen.

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

### Steckverbindungen für externe Taster

Die zwei in der Sensor-Firmware bereits vorhandenen Taster erhalten
**je einen separaten 2-poligen, abziehbaren Gehäuseanschluss**:

| Steckplatz | Kontakt 1 | Kontakt 2 | Funktion |
| --- | --- | --- | --- |
| BTN-PHASE | GPIO16 | GND | Umschaltung Wuchs/Blüte |
| BTN-SERVICE | GPIO19 | GND | Pumpen-Wartungssperre |

Vorgesehene Steckverbinder: **Phoenix Contact MSTB 2,5,
2-polig, 5,08 mm Raster**:
- 2 × PCB-Grundleiste `1757242`
- 2 × Gegenstecker `1757019`

Die Firmware verwendet für **beide** Taster `INPUT_PULLUP`
und reagiert auf das Schließen nach GND. Es sind deshalb **keine
zusätzlichen externen Pull-up-Widerstände** vorgesehen.
Die Gehäusetaster sind als potenzialfreie Schließkontakte zu planen,
nicht mit einer externen Spannung zu speisen. Der Wartungstaster
bildet nur eine **softwareseitige** Sperre, keine galvanische Trennung.

**Aktueller Steckverbinder-Zwischenstand Sensorplatine:**
- 6 × 3-polig (DHT22 × 3, DS18B20, A02YYUW, EC/TDS)
- 2 × 4-polig (BH1750, PH4502C)
- 2 × 2-polig (Wuchs/Blüte, Pumpen-Wartung)
- **Summe: 10 Steckerplätze** für bisher einzeln festgelegte
  externe Komponenten; Anschlüsse anderer Boardbaugruppen
  sind nicht mitgezählt.

## Verbindliche Ergänzungen: MLX90640, ADS1115, RGB, USB-C

### Wärmebildkamera MLX90640 — vorgesehen

Die Kamera erhält **einen eigenen vierpoligen, abziehbaren Steckanschluss**
`CAM-MLX90640` auf der Sensorplatine. Sie teilt den bereits
zugeordneten I²C-Bus mit dem ADS1115 (GPIO21/22, Kamera-Adresse 0x33):

| Kontakt | Signal | Verbindung |
| --- | --- | --- |
| 1 | 3V3 | Modulversorgung, sofern die konkrete Kameraplatine 3,3 V unterstützt |
| 2 | GND | gemeinsame Sensormasse |
| 3 | SDA | GPIO21 |
| 4 | SCL | GPIO22 |

Baureihe: wie BH1750, **Phoenix Contact MSTB 2,5, vierpolig,
5,08 mm**; vollständige Artikelnummern und Footprint noch
zu prüfen. Versorgung und mögliche I²C-Pull-ups anhand des
tatsächlichen MLX90640-Breakouts verifizieren. Die Firmware
besitzt bereits MLX90640-Code, aber `GROW_ENABLE_MLX90640` steht
aktuell auf **0**; physische Montage heißt noch nicht,
dass Kameramessungen aktiviert oder validiert sind.

### ADS1115 — steckbares Modul direkt auf der Sensorplatine

Kein externer Kabelanschluss für das ADS1115-Modul: Es bekommt
**eine passende steckbare Stiftleisten-/Buchsenleisten-Aufnahme**
direkt auf der Sensorplatine. Elektrische Signale:
`3V3`, `GND`, `SDA=GPIO21`, `SCL=GPIO22`,
`ADDR=GND` für I²C-Adresse 0x48 sowie `A0` (pH)
und `A1` (EC/TDS). Weitere Modulanschlüsse sind nur
entsprechend dem tatsächlich verwendeten Breakout vorzusehen.
**Pinzahl, Kontaktreihenfolge, Raster und Befestigung sind noch
nicht anhand der konkreten ADS1115-Platine verifiziert.**
Vorhandene I²C-Pull-ups und ADDR-Beschaltung prüfen.

### RGB-Status-LED — Gehäusestecker

Ein eigener **vierpoliger, abziehbarer Anschluss** `LED-RGB`
für die vorhandene LED mit **gemeinsamer Kathode**:

| Kontakt | Bezeichnung | Firmware-Signal |
| --- | --- | --- |
| 1 | R | GPIO33 (über Vorwiderstand R_RGB_R) |
| 2 | G | GPIO4 (über Vorwiderstand R_RGB_G) |
| 3 | B | GPIO17 (über Vorwiderstand R_RGB_B) |
| 4 | COM | GND, gemeinsame Kathode |

**Drei getrennte Vorwiderstandspositionen** auf der
Sensorplatine vorsehen. Exakte Widerstandswerte erst nach
LED-Datenblatt (Vorwärtsspannung/Nennstrom) und
GPIO-Stromgrenzen bestimmen. Steckfamilie: vierpoliger
MSTB 2,5, Artikel/Footprint noch zu prüfen.

### Versorgung

Die **Sensor-ESP32-Baugruppe wird mit 5 V über USB-C**
versorgt. Bei der geplanten Trägerplatine ist die vorhandene
USB-C-Versorgung des tatsächlichen ESP32-Moduls einschließlich
USB-Zugänglichkeit und der 3,3-V-Versorgungskapazität zu prüfen.
**Keinen zusätzlichen eigenmächtigen 5-V-Einspeisepunkt**
parallel zu USB-C festlegen, bevor Rückspeisung und
Versorgungstopologie überprüft sind.

### Aktualisierte Anschlusszählung der Sensorplatine

| Klasse | Anzahl | Enthalten |
| --- | ---: | --- |
| 2-polig | 2 | Phasen-/Wartungstaster |
| 3-polig | 6 | 3 DHT22, DS18B20, A02YYUW, EC/TDS |
| 4-polig | 4 | BH1750, PH4502C, MLX90640, RGB-LED |
| **Abziehbare externe Steckplätze** | **12** | ohne ADS1115-Steckaufnahme |

Hinzu kommt **eine steckbare ADS1115-Modulaufnahme**.
Vier 4,7-kΩ-Datenpull-ups (3 DHT22, 1 DS18B20)
und drei RGB-Vorwiderstandspositionen sind vorgesehen.
pH-PO wird direkt zum ADS1115 A0 geführt; **kein Spannungs-
teiler** auf dem Sensorträger. Optionale I²C-Pull-ups bleiben bis
zur Prüfung der Breakouts unbestückt; vor Fertigung sind noch
Footprint-Abgleich und KiCad-DRC erforderlich.

Das Sensor-Board hat **kein lokales Display**. Die Messwerte erscheinen im seriellen Monitor und über die Server-API auf dem separaten LVGL-Remote.

Versorgung und Masse sowie physische Pinpositionen sind anhand des tatsächlich verbauten Pro-Boards zu verifizieren. Keine Netzspannungs-/SSR-Lastverkabelung am Sensorcontroller.

## Verifizierung

- [x] Software-GPIOs und Sensorzuordnungen geprüft
- [x] Alte lokale Display-Ansteuerung aus Sensor-Firmware entfernt
- [ ] Reale Pinpositionen des Pro-Boards geprüft
- [ ] Arduino-Kompilierung erfolgreich
- [ ] Serieller Messwerttest und LVGL-Remote-Test an realer Hardware erfolgreich


## Steckbare ESP32-Modulaufnahme

**Verbindliche Konstruktionsanforderung:** Der Sensor-ESP32 wird **nicht direkt auf die Trägerplatine eingelötet**, sondern erhält eine passende steckbare Buchsenleisten-/Modulaufnahme. Der Ausbau soll ohne Entlöten möglich sein.

- Buchsenleisten müssen zur tatsächlichen Pinzahl, Reihenanordnung und Rastergeometrie des eingesetzten Sensor-ESP32 passen.
- USB-/Programmieranschluss muss auch bei gestecktem Modul zugänglich bleiben.
- Die Versorgung über den vorhandenen USB-C-Anschluss bleibt vorgesehen; keine zweite 5-V-Einspeisung ohne Rückspeiseprüfung.
- Antennenfreiraum und Bauhöhe über der Trägerplatine sind zu berücksichtigen.
- **Der exakte Footprint ist noch nicht freigegeben:** Die tatsächliche Modellrevision und Kontaktabstände des Sensor-ESP32 sind vor der PCB-Fertigung zu verifizieren.

Die steckbare ESP32-Aufnahme ist **keiner der 12 externen Sensor-/Bedienstecker** und wird separat in der Bauteilliste geführt. Keine Änderung der Firmware oder GPIO-Zuordnung.

### LED-Bestückung: ausschließlich vorhandene externe RGB-LED

**Verbindliche Nutzerfestlegung (15.11.2026):** Die einzige vorgesehene Status-LED ist die **bereits vorhandene externe RGB-LED mit gemeinsamer Kathode**. Sie wird über den dokumentierten vierpoligen Stecker `LED-RGB` (R=GPIO33, G=GPIO4, B=GPIO17, gemeinsame Kathode=GND) angeschlossen. Auf der Trägerplatine **keine zusätzlichen SMD-LEDs, keine Power-LEDs, keine separaten Status-LEDs** vorsehen. Die drei erforderlichen **Vorwiderstände** werden als vorhandene **bedrahtete Keramikwiderstände** auf der Platine vorgesehen; keine SMD-Widerstände und keine zusätzlichen LEDs. Die Vorwiderstandswerte sowie Körper- und Drahtmaße bleiben bis zur Prüfung der vorhandenen RGB-LED und Widerstände offen. Diese Festlegung betrifft die Trägerplatine; integrierte LEDs auf fertig gekauften ESP32-Modulen sind keine zusätzlich zu bestückenden Leiterplatten-LEDs.

### Widerstands-Bauform: Nutzerbestand, keine SMD-Widerstände

Verbindliche Nutzerfestlegung vom 15.11.2026: **Auf der Sensor-Trägerplatine keine SMD-Widerstände bestücken.** Sämtliche geplanten Widerstandspositionen sind für **vorhandene bedrahtete Keramikwiderstände** auszulegen. Das betrifft DHT22-/DS18B20-Pull-ups, RGB-Vorwiderstände, optional bestückbare I²C-Pull-ups sowie die beiden pH-Eingangswiderstände. **SMD-/0805-Vorschläge aus früheren Planungsständen sind hiermit überholt.**

Vor Footprint-Freigabe sind Körperlänge/-durchmesser, Drahtdurchmesser, benötigtes Anschlussraster, Widerstandswerte und zulässige Verlustleistung der tatsächlich vorhandenen Widerstände zu prüfen. Die frühere elektrische Vorgabe **4,7 kΩ** für vier Daten-Pull-ups wird dadurch nicht geändert; für alle weiteren Positionen bleiben Werte unbestätigt. „Keramikwiderstand“ allein belegt weder Wert noch Bauform.

### RGB-Vorwiderstände: bevorzugt einzelne 220 Ω je Farbe

**Aktuelle Nutzerfestlegung (15.11.2026):** Keine Widerstände in Reihe schalten. Wenn elektrisch zulässig, **je ein einzelner bedrahteter 220-Ω-Vorwiderstand** für Rot (GPIO33), Grün (GPIO4) und Blau (GPIO17). Insgesamt drei Widerstände, **keine SMD-Bestückung** und **keine zusätzliche LED**. Die 220-Ω-Werte sind ein **bevorzugter Bestückungsansatz**, keine verifizierte elektrische Freigabe: LED-Durchlassspannungen, gewünschte Ströme und GPIO-Toleranzen des tatsächlichen Moduls sind noch zu bestätigen. Beispiel bei 3,3 V: Rot mit Vf = 2,0 V ergibt ca. 5,9 mA; Grün/Blau mit Vf = 3,0 V ergeben ca. 1,4 mA. Diese Vf-Werte sind Rechenannahmen, keine Messungen.


## Bildgestützter physischer Pinout-Abgleich (15.11.2026)

**Quelle:** Die zwei vom Nutzer am 15.11.2026 hochgeladenen Produktbilder.
Nicht mit elektrischer Durchgangsmessung verifiziert.

### 30-Pin ESP32 DevKit — Kontaktpositionen aus dem Pinoutbild

Zählweise der Abbildung: **linke Reihe von oben nach unten 1–15**,
**rechte Reihe von unten nach oben 16–30**; Draufsicht mit
Antennenende oben und USB unten.

| Kontakt | Platinenbeschriftung | GPIO/Netz | Projektverwendung |
| ---: | --- | --- | --- |
| 1 | EN | EN | Reset/Enable, keine Sensorleitung |
| 2 | VP | GPIO36 | derzeit frei |
| 3 | VN | GPIO39 | derzeit frei |
| 4 | D34 | GPIO34 | derzeit frei |
| 5 | D35 | GPIO35 | derzeit frei |
| 6 | D32 | GPIO32 | derzeit frei |
| 7 | D33 | GPIO33 | RGB Rot |
| 8 | D25 | GPIO25 | DHT22 #1 |
| 9 | D26 | GPIO26 | DHT22 #2 |
| 10 | D27 | GPIO27 | DHT22 #3 |
| 11 | D14 | GPIO14 | BH1750 SCL |
| 12 | D12 | GPIO12 | derzeit frei |
| 13 | D13 | GPIO13 | BH1750 SDA |
| 14 | GND | GND | Masse |
| 15 | VIN / 5V | 5 V | 5-V-Pin; keine separate Einspeisung, USB-C-Versorgung |
| 16 | 3V3 | 3,3 V | Sensor-Versorgung nach Reglerprüfung |
| 17 | GND | GND | Masse |
| 18 | D15 | GPIO15 | derzeit frei |
| 19 | D2 | GPIO2 | derzeit frei |
| 20 | D4 | GPIO4 | RGB Grün |
| 21 | D16 | GPIO16 | Phasentaster |
| 22 | D17 | GPIO17 | RGB Blau |
| 23 | D5 | GPIO5 | Buzzer reserviert, deaktiviert |
| 24 | D18 | GPIO18 | A02YYUW TX → UART RX |
| 25 | D19 | GPIO19 | Wartungstaster |
| 26 | D21 | GPIO21 | ADS1115 + MLX90640 SDA |
| 27 | RX0 | GPIO3 | UART0 RX, derzeit frei |
| 28 | TX0 | GPIO1 | UART0 TX, derzeit frei |
| 29 | D22 | GPIO22 | ADS1115 + MLX90640 SCL |
| 30 | D23 | GPIO23 | DS18B20 DATA |

**Abgleich:** Alle bislang vorgesehenen projektseitigen Sensor-GPIOs sind
in diesem 30-Pin-Bild vorhanden. Die Zahl des Steckkontakts ist
**nicht** gleichbedeutend mit der GPIO-Nummer (z. B. Kontakt 7 = GPIO33).

### ADS1115-Breakout — Kontaktfolge aus dem Foto

Fotoorientierung: **Beschriftung auf der Bauteilseite lesbar**,
10-polige Leiste links, **oben nach unten**:

| Position | Aufdruck | Netzfunktion im Projekt |
| ---: | --- | --- |
| 1 | VDD | 3V3 |
| 2 | GND | GND |
| 3 | SCL | ESP32 Kontakt 29 = GPIO22 |
| 4 | SDA | ESP32 Kontakt 26 = GPIO21 |
| 5 | ADDR | GND für ADS-Adresse 0x48 laut Planung, tatsächliche Modulbeschaltung prüfen |
| 6 | ALRT | derzeit unbenutzt, keinen GPIO verbinden |
| 7 | A0 | pH-Eingangskreis, ungeprüft |
| 8 | A1 | EC/TDS-Eingangskreis, ungeprüft |
| 9 | A2 | derzeit unbenutzt |
| 10 | A3 | derzeit unbenutzt |

**Mechanik:** eine 1×10-Buchsenleiste, nominelles Pinraster 2,54 mm;
Kontaktachse erster–letzter Pin = 22,86 mm. Die physische
Bestückungsrichtung/Buchsenposition auf der neuen Platine muss so gewählt
werden, dass diese **fotografierte Reihenfolge** erhalten bleibt.

**Noch offen vor Herstellungsfreigabe:** echte ESP32-Modulvariante,
Reihenmittenmaß, Rückseite/Orientierung, ADS1115-Modulumriss,
USB-C-Freiraum, Versorgungskapazität und geprüfte Analogpegel.

## Aktueller PCB-Stand (15.11.2026)

Die zu dieser Firmware gehörende **geroutete Sensorträgerplatine**
liegt unter `hardware/cad/sensor_pcb_routed_candidate.kicad_pcb`.
Auswahl: 12 externe steckbare Anschlüsse, steckbarer ESP32 30 Pin,
steckbarer ADS1115 1×10, **11 bedrahtete
Widerstandspositionen** (vier Daten-Pull-ups, drei RGB, vier
optionale I²C), keine zusätzlichen LEDs. pH-PO direkt auf A0,
EC analog direkt auf A1. Die PCB ist **noch nicht fertigungsfreigegeben**
(KiCad-DRC und physische Footprintprüfung offen).
