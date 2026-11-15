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
- Board-seitige **physische Pinpositionen und Versorgungsausgänge**
  sind noch nicht im konkreten Produkt-Pinout verifiziert.
- Keine zusätzliche Power-/Status-LED auf der Trägerplatine.

### U_ADS — ADS1115-Modul, 10 Kontakte

- **1 × 10-polige Buchsenleiste**, Längsraster 2,54 mm,
  Pinmitten 1–10 = 22,86 mm.
- Funktionsnetze: 3V3, GND, SDA/GPIO21, SCL/GPIO22,
  ADDR nach GND (I²C 0x48), A0=pH, A1=EC/TDS.
- Modul hat 10 tatsächliche Pins, **deren Reihenfolge ist noch
  nicht verifiziert**. Übrige Pins dürfen nicht beliebig belegt werden;
  tatsächliche Beschriftung und Modul-Outlines prüfen.

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

- Exaktes Hersteller-/Lieferantenpinout des 30-Pin-ESP32 samt
  tatsächlichen physischen Pinpositionen fehlt.
- Exakte 10-polige ADS1115-Pinreihenfolge, Modulbreite und
  Orientierung fehlen.
- Reglerstromreserve 3,3 V und 5-V-Ausgangspfad sind unbewiesen.
- Analogpegel/Schutz an ADS1115 A0/A1, A02YYUW UART-Spannung
  und I²C-Pull-up-Bestückung nicht geprüft.
- Vierpolige Klemmen-Footprints, Lochabstände/LED-Widerstands-Footprints,
  Platzierung, Gehäuse und Montagelöcher müssen finalisiert werden.
- Ein fertiger Schaltplan mit ERC und ein Layout mit DRC fehlen.

**Die Liste ist ein prüfbarer PCB-Vorentwurf, keine Bestellungsvorlage
für eine vollständig funktionsfähige Platine.**
