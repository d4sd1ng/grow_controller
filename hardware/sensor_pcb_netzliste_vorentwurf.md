# Sensor-PCB — eindeutige Anschluss-/Netzliste (Vorentwurf, 15.11.2026)

**Zweck:** Übergabe der bekannten Steckverbinder- und GPIO-Zuordnungen
an den Schaltplanentwurf. **Nicht** KiCad-Netzliste, Schaltplan, geprüfter
Footprint, Layout oder Gerber. Ausgangspunkt:
`firmware/Sensor/PINOUT.md` sowie aktuelle Nutzerfestlegungen.

**Baugruppe:** steckbarer ESP32 mit 30 Pins, zwei Reihen à 15 Kontakte,
nominelles Längsraster 2,54 mm; vorläufiger Reihenmittenabstand 25,40 mm,
Außenmaß 28 × 51 mm. Versorgung des ESP32 **nur über USB-C (5 V)**.
USB-C-Zugang, Modulreglerreserve und reale Kontaktfolge **offen**.

## 1. Externe abziehbare Steckverbinder

Bei den folgenden Angaben handelt es sich jeweils um die **beabsichtigte
Kontaktfolge auf der Trägerplatine**, nicht um die ungeprüfte
Aderreihenfolge des jeweiligen Sensormoduls.

| Ref | Stecker / Pole | Kontakt 1 | Kontakt 2 | Kontakt 3 | Kontakt 4 |
| --- | --- | --- | --- | --- | --- |
| J_DHT1 | 3-polig | 3V3 | DHT1_DATA / GPIO25 | GND | – |
| J_DHT2 | 3-polig | 3V3 | DHT2_DATA / GPIO26 | GND | – |
| J_DHT3 | 3-polig | 3V3 | DHT3_DATA / GPIO27 | GND | – |
| J_DS18 | 3-polig | 3V3 | DS18_DATA / GPIO23 | GND | – |
| J_LEVEL | 3-polig | 3V3* | LEVEL_TX / GPIO18 (UART RX) | GND | – |
| J_EC | 3-polig | 5V* | GND | EC_ANALOG / ADS1115 A1* | – |
| J_PH | 4-polig | 5V* | PH_GND1* | PH_GND2* | PH_PO → A0-Eingangskreis* |
| J_BH1750 | 4-polig | 3V3 | GND | BH_SDA / GPIO13 | BH_SCL / GPIO14 |
| J_MLX | 4-polig | 3V3* | GND | I2C_SDA / GPIO21 | I2C_SCL / GPIO22 |
| J_RGB | 4-polig | RGB_R über R_RGB_R / GPIO33 | RGB_G über R_RGB_G / GPIO4 | RGB_B über R_RGB_B / GPIO17 | GND (gemeinsame Kathode) |
| J_PHASE | 2-polig | GPIO16 | GND | – | – |
| J_SERVICE | 2-polig | GPIO19 | GND | – | – |

**Sternchen = ungeprüfte elektrische Bedingung / kein automatisch
freigegebenes Netz.**
- `J_LEVEL`: A02YYUW-Versorgung 3,3 V, TX-Ausgangspegel und Betriebsmodus
  am konkreten Sensor verifizieren.
- `J_EC`: Modulversorgung 5 V am realen Breakout prüfen;
  EC_ANALOG muss innerhalb des sicheren ADS1115-Eingangsbereichs bleiben.
- `J_PH`: Zwei separat bezeichnete GND-Pins am Modul;
  interne Verbindung und passende Beschaltung am echten Modul prüfen,
  bevor die GND-Netze verbunden werden. PO wird nicht ungeprüft direkt
  nach A0 geführt.
- `J_MLX`: 3,3-V-Versorgung am tatsächlichen MLX90640-Breakout prüfen;
  Firmwarefunktion aktuell deaktiviert.
- 5-V-Versorgung analoger Breakouts wird erst nach Nachweis der
  Belastbarkeit des USB-C-/ESP32-Modul-Versorgungspfads freigegeben.

**Summen:** 2 × 2-polig, 6 × 3-polig, 4 × 4-polig = **12 externe
Anschlüsse**; zusammen **38 externe Kontaktpositionen**
(2×2 + 6×3 + 4×4 = 38).

## 2. Steckbare interne Module

### U_ESP32 — Sensor-ESP32, 30 Kontakte

- **2 × 15-polige Buchsenleiste**, 2,54-mm-Raster längs, vorläufig
  25,40-mm-Reihenmittenabstand.
- Firmwarebelegung: GPIO4,13,14,16,17,18,19,21,22,23,25,26,27,33.
- **Physische Kontaktzuordnung ist aus dem vom Nutzer gelieferten 30-Pin-ESP32-Pinout-Bild abgelesen** (siehe Tabelle am Ende dieser Datei). Das reale Modul ist vor Fertigungsfreigabe gegen das Bild zu prüfen.
- Keine zusätzliche Power-/Status-LED auf der Trägerplatine.

### U_ADS — ADS1115-Modul, 10 Kontakte

- **1 × 10-polige Buchsenleiste**, Längsraster 2,54 mm,
  Pinmitten 1–10 = 22,86 mm.
- Funktionsnetze: 3V3, GND, SDA/GPIO21, SCL/GPIO22,
  ADDR nach GND (I²C 0x48), A0=pH, A1=EC/TDS.
- Die **10-polige Kontaktfolge ist anhand des Nutzerfotos abgelesen** (Tabelle am Dateiende); die Einbaulage, Außenkontur und mechanische Pinanordnung sind am realen Modul zu prüfen. Übrige Pins nicht beliebig belegen.

## 3. Passivteile (ausschließlich bedrahtet, keine SMD-Widerstände)

| Ref | Anschluss A | Anschluss B | Bestückung |
| --- | --- | --- | --- |
| R_DHT1 | 3V3 | GPIO25 / DHT1_DATA | 4,7 kΩ; zusätzliche Modul-Pull-ups kontrollieren |
| R_DHT2 | 3V3 | GPIO26 / DHT2_DATA | 4,7 kΩ; zusätzliche Modul-Pull-ups kontrollieren |
| R_DHT3 | 3V3 | GPIO27 / DHT3_DATA | 4,7 kΩ; zusätzliche Modul-Pull-ups kontrollieren |
| R_DS18 | 3V3 | GPIO23 / DS18_DATA | 4,7 kΩ |
| R_RGB_R | GPIO33 | J_RGB.1 | 220 Ω bevorzugt, vorbehaltlich LED-Nachweis |
| R_RGB_G | GPIO4 | J_RGB.2 | 220 Ω bevorzugt, vorbehaltlich LED-Nachweis |
| R_RGB_B | GPIO17 | J_RGB.3 | 220 Ω bevorzugt, vorbehaltlich LED-Nachweis |
| R_PH_TOP | J_PH.4 (PO) | A0_KNOTEN | Wert/Topologie des echten Aufbaus offen |
| R_PH_BOTTOM | A0_KNOTEN | GND (nach Verifizierung) | Wert/Topologie offen |
| R_I2C0_SDA | 3V3 | GPIO21 / SDA | optional, DNP bis Pull-up-Prüfung |
| R_I2C0_SCL | 3V3 | GPIO22 / SCL | optional, DNP bis Pull-up-Prüfung |
| R_I2C1_SDA | 3V3 | GPIO13 / SDA | optional, DNP bis Pull-up-Prüfung |
| R_I2C1_SCL | 3V3 | GPIO14 / SCL | optional, DNP bis Pull-up-Prüfung |

Insgesamt **13 bedrahtete Widerstandspositionen**;
4 × 4,7 kΩ vorgesehen, 3 × 220 Ω als bevorzugte Einzelwerte und
6 noch unbestimmte/optionale Positionen. Kein Wert oder Footprint wird
aus dem Wort „Keramikwiderstand“ abgeleitet. Der tatsächliche
Drahtdurchmesser, Körperabmessungen und der Lochabstand fehlen noch.

## 4. Mechanische Stückliste: Steckverbinder (Sensorplatine)

| Funktion | Phoenix Contact Grundleiste | Gegenstecker | Menge je Teil |
| --- | --- | --- | ---: |
| 2-polig, 5,08 mm | 1757242 | 1757019 | 2 |
| 3-polig, 5,08 mm | 1757255 | 1757022 | 6 |
| 4-polig, 5,08 mm | 1757268 | 1757035 | 4 |

**Summe:** 12 Grundleisten und 12 passende Gegenstecker,
zuzüglich 2 × 15-poliger und 1 × 10-poliger Modulbuchsenleiste
(die genaue Buchsenleistenvariante ist noch offen).

## 5. Harte Freigabesperren

- **Pinbelegungen der beiden Module sind durch Nutzerbilder dokumentiert**, aber die Übereinstimmung mit den real vorliegenden Boards ist noch zu prüfen.
- Mechanische ADS1115-Modulbreite/-orientierung und ESP32-Reihenmittenabstand erfordern noch eine Messung.
- Reglerstromreserve 3,3 V und 5-V-Ausgangspfad sind unbewiesen.
- Analogpegel/Schutz an ADS1115 A0/A1, A02YYUW UART-Spannung
  und I²C-Pull-up-Bestückung nicht geprüft.
- Vierpolige Klemmen-Footprints, Lochabstände/LED-Widerstands-Footprints,
  Platzierung, Gehäuse und Montagelöcher müssen finalisiert werden.
- Ein fertiger Schaltplan mit ERC und ein Layout mit DRC fehlen.

**Die Liste ist ein prüfbarer PCB-Vorentwurf, keine Bestellungsvorlage
für eine vollständig funktionsfähige Platine.**


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


## pH-/EC-Statuskorrektur (15.11.2026)

Die vorhandenen pH- und EC/TDS-Sensoren funktionieren laut Nutzer **bereits am Sensor-ESP32 über den ADS1115**; die bestehende Versorgung reicht aus. Die neue Platine soll **die tatsächlich funktionierende Verdrahtung 1:1 ersetzen**, ohne Änderungen der Analogsignalaufbereitung oder Kalibrierung. `PH_A0_CONDITIONED_UNVERIFIED` und `EC_A1_CONDITIONED_UNVERIFIED` bedeuten lediglich, dass die **konkreten Ist-Verbindungen im CAD noch nicht abgebildet** sind, nicht dass die Sensoren aktuell nicht funktionieren. Der ältere Kabelplan ist als veraltet markiert und darf nicht unbesehen als aktuelle Schaltung gelten.

**Nächste CAD-Arbeit:** Existierende Anschluss-/Widerstandsreihenfolge des funktionierenden Aufbaus anhand einer aktuellen, belastbaren Verdrahtungsquelle übernehmen, dann Schaltplan/Netzliste konsistent aktualisieren. Keine eigenmächtigen neuen Teilerwerte.
