# PCB-Bestellfreigabe — Stand 15.11.2026, Zieltermin 16.11.2026

**Status: NICHT BESTELLFREIGEGEBEN.** Diese Datei ist eine belegte
Konstruktions-/Prüfliste, **kein** Schaltplan, kein PCB-Layout, keine Gerberdaten
und keine ausführbare chemische Dosiersteuerung.

## 1. Verbindliche Systemarchitektur aus dem Projekt

- Sensor-ESP32 als austauschbares Steckmodul, 5-V-Einspeisung über USB-C.
- C3: ESP32-C3 Super Mini als austauschbares Steckmodul, eigene
  5-V-Einspeisung **ausschließlich über USB-C**.
- ADS1115 als steckbares Modul auf Sensorträger; 3,3 V, I2C GPIO21/22;
  pH auf A0, EC/TDS auf A1.
- BH1750: separat GPIO13/14; MLX90640: GPIO21/22, vorgesehen,
  in Firmware noch deaktiviert.
- DHT22 ×3, DS18B20 ×1, A02YYUW ×1, pH ×1, EC/TDS ×1,
  RGB-Status-LED ×1, Gehäusetaster ×2.
- C3: PCA9685, sechs mechanisch reservierte 12-V-Pumpenplätze,
  ein gemeinsamer Steueranschluss für zwei Lüfter, fünf SSR-Steueranschlüsse;
  Prima Klima PK125-2 Abluft über SSR EIN/AUS, Stufe manuell.
- Pumpenangaben: 12 V und 3 W je Stück -> rechnerisch 0,25 A Nennstrom
  je Pumpe. **Anlauf-/Blockierstrom unbekannt**.
- Dosierkanal 1 = Plagron pH Min; chemische Dosierung bleibt gesperrt.

## 2. Sensorträger: Anschlussbudget

| Anschlussart | Anzahl | Funktionen |
| --- | ---: | --- |
| 2-polige abziehbare Klemme | 2 | Taster Wuchs/Blüte und Wartung |
| 3-polige abziehbare Klemme | 6 | DHT22 ×3, DS18B20, A02YYUW, EC/TDS |
| 4-polige abziehbare Klemme | 4 | PH4502C, BH1750, MLX90640, RGB-LED |
| Steckbare Modulaufnahme | 2 | Sensor-ESP32, ADS1115 |
| USB-C | über ESP32 | 5-V-Einspeisung, keine weitere 5-V-Zuführung |

Noch ohne Footprint: **reales Sensor-ESP32-Modul und reales ADS1115-Breakout**,
ihre Pinabstände, Höhen und USB-Zugänglichkeit.

### Bestätigte passiv zu berücksichtigende Positionen

- **R_DHT1, R_DHT2, R_DHT3 = je 4,7 kΩ** von DATA nach 3,3 V,
  modulinterne Widerstände noch prüfen.
- **R_DS18 = 4,7 kΩ** von DATA/GPIO23 nach 3,3 V.
- **R_RGB_R/G/B** = drei separate Vorwiderstandspositionen;
  **3 × 440 Ω laut korrigierter Nutzerangabe**, elektrische Eignung für die vorhandene RGB-LED noch zu prüfen.
- **R_PH_OBEN und R_PH_UNTEN** = zwei Widerstands-Footprints
  für bisher dokumentierten Eingangsspannungsteiler A0. Die alte,
  ausdrücklich **veraltete** Verkabelungsdatei nennt 10 kΩ / 20 kΩ;
  bestehender realer Teiler und Kalibrierung sind vor Bestückung
  zu messen/verifizieren. Keine Änderung ohne Neukalibrierungsbewertung.
- **ADS1115 A1**: bislang kein Spannungsteiler bestätigt; etwaigen
  Überspannungsschutz erst nach verifiziertem Modulausgang festlegen.
- **I2C SDA/SCL-Pull-ups**: auf vorhandenen ADS1115-, MLX90640- und
  BH1750-Modulen überprüfen; keine unkontrollierte Parallelbestückung.

**ADS1115-Datenblatt**: Analogeingänge absolut maximal
GND − 0,3 V bis VDD + 0,3 V; bei 3,3-V-Versorgung kein
ungeregelter 5-V-Eingang.
Quelle: https://www.ti.com/lit/ds/symlink/ads1115.pdf

## 3. C3-Träger: Anschlussbudget

| Anschlussart | Anzahl | Funktionen |
| --- | ---: | --- |
| 2-polige abziehbare Klemme | 12 | 12-V-Einspeisung ×1, SSR ×5, Motorplätze ×6 |
| 3-polige abziehbare Klemme | 1 | gemeinsame Lüftertreiber-Schnittstelle, Pins offen |
| Steckbare Modulaufnahme | 1 | ESP32-C3 Super Mini |
| USB-C | über C3 | einzige 5-V-C3-Einspeisung |

Die PCA9685-VDD-Logik arbeitet laut Datenblatt im Bereich 2,3–5,5 V;
bei 3,3 V VDD sind bis zu 10 mA **reiner IC-Betriebsstrom ohne Last**
angegeben. Dies ist weder ein Motorstrom noch ein Netzteil-Gesamtwert.
Quelle: https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf

**Wichtige Schaltungsfrage:** PCA9685 /OE wirkt auf **alle**
Ausgänge (sechs Dosierplätze und Lüfter). Die bestehende Firmware
führt zwei Lüftergruppen, obwohl ein gemeinsames Steuersignal
gefordert ist. Keine Hardwarefreigabe, bis die Kanaltrennung und
Schnittstellen der tatsächlichen Geräte geklärt sind.

## 4. Steckverbinder-BOM (externe Anschlüsse, beide Träger)

| Pole | Sensor | C3 | Summe je Teil (Grundleiste + Gegenstecker) |
| --- | ---: | ---: | ---: |
| 2 | 2 | 12 | 14 |
| 3 | 6 | 1 | 7 |
| 4 | 4 | 0 | 4 |
| **Gesamt** | **12** | **13** | **25** |

Bereits in Projektplanung festgelegte Artikel:
- 2-polig: Phoenix Contact `MSTBA 2,5/2-G-5,08` **1757242**;
  Stecker `MSTB 2,5/2-ST-5,08` **1757019**.
- 3-polig: Phoenix Contact `MSTBA 2,5/3-G-5,08` **1757255**;
  Stecker `MSTB 2,5/3-ST-5,08` **1757022**.
- 4-polig: Phoenix Contact `MSTBA 2,5/4-G-5,08` **1757268**;
  Stecker `MSTB 2,5/4-ST-5,08` **1757035**.

Die Nummern der vierpoligen Grundleiste sind auf Phoenix-Contact-Produktseiten
belegt; für die vierpolige Steckerausführung liegt ein Datenblatt vor.
4-polige Quelle Grundleiste:
https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-4-g-508-1757268
Gegenstecker-Datenblatt:
https://robu-prod-media.s3.ap-south-1.amazonaws.com/uploads/2025/02/C90078.pdf

**Hinweis:** Die 25 Anschlussplätze sind die bisherigen
Planungsplätze, keine automatisch freigegebenen Motor-/Säureausgänge
und keine Aussage zu nötigen zusätzlichen elektrischen Prüfanschlüssen.
Kein direktes 230-V-Führen auf dieser Platine.

## 5. Was für eine Produktionsbestellung noch fehlt

| Prüfpunkt | Ergebnis/Fehlstelle |
| --- | --- |
| Echter Schaltplan/netzlistenbasierte ERC | fehlt |
| Getestete 5-V-USB-/3,3-V-Versorgung inkl. Strombudget beider Boards | fehlt |
| Konkret vermessene Modul-Footprints (C3, Sensor-ESP32, ADS1115, PCA9685) | fehlt |
| Verifizierte Einbau-/Leitungsaustrittsmaße aller Steckverbinder | fehlt |
| Werte der RGB-Widerstände und geprüfte I2C-Pull-ups | fehlt |
| A0/A1-Analog-Schutz-/Messkreis in realem Aufbau validiert | fehlt |
| 12-V-/5-V-Masse-, Schutz-/Entstörkonzept | fehlt |
| Treiberelektrik und unabhängige sichere Lastfreigabe | fehlt |
| Exakte Steuerfunktion des gemeinsamen 12-V-Lüftertreibers | fehlt |
| SSR-Eingangsdaten/Verträglichkeit zur Controller-Logik | fehlt |
| Gehäuseinnenraum, Steckerzugang, Lochbild, Höhen | fehlt |
| PCB-Layout, DRC, Gerber, Drill, Board-Outline | fehlt |
| Funktionstest/Compilation der Firmware | fehlt |

**Bestellentscheidung:** Keine Stückliste oder Gerberdatei
als abgeschlossen markieren, bevor die fehlenden Maße und Tests
vorliegen. Falls ein Termin nicht verschiebbar ist, kommt nur
ein klar als **unbestückter, elektrisch nicht freigegebener
mechanischer Prototyp** in Betracht; das ist keine
Fertigungsfreigabe für den Live-Controller.

## 6. Neue Modul-Pinzahl-Angaben

Vom Nutzer am 15.11.2026 bestätigt:
- **ADS1115-Breakout: insgesamt 10 Pins.** Anzahl Pins pro Seite, Pinreihen und Raster sind **noch nicht bestätigt**.
- **Sensor-ESP32: 30-Pin-Ausführung.** Konkrete Anzahl je Reihe, Reihenmittenabstand und Raster sind **noch zu messen**.
- Beide Module sind **steckbar** auszuführen; kein Footprint ohne verifizierte Kontaktabmessungen freigeben.

### Steckkontakt-Raster

Nutzer bestätigt: Die Steckkontakte des **30-Pin-Sensor-ESP32** und des **10-Pin-ADS1115-Moduls** passen auf eine gewöhnliche Lochrasterplatine. **Daraus folgt nominal 2,54 mm Kontaktabstand entlang der Pinreihe** (vor Fertigung am tatsächlichen Modul nachprüfen). **Reihenmittenabstände** der Module bleiben unbekannt; außerdem sind die Modulaußenmaße noch zu ermitteln. Ein kompletter Footprint kann deshalb noch nicht freigegeben werden.

### Sensor-ESP32 Reihenabstand: unbestätigte Angabe

Nutzerangabe: **vermutlich 36 mm** Abstand der beiden Pinreihen beim 30-Pin-Sensor-ESP32. Die Angabe ist eine **Schätzung**, nicht nachgemessen; zudem noch nicht bestätigt, ob Pinmitte-zu-Pinmitte gemeint ist. **Keinesfalls als freigegebenen Footprint verwenden.** Vor Layout-Freigabe exaktes Reihenmittenmaß am realen Modul ermitteln.

### Recherche zum 30-Pin-ESP32-Reihenabstand

15.11.2026: Nutzer schätzt 35–36 mm, ohne Messung. Recherchierte **verschiedene** 30-Pin-ESP32-DevKit-V1-Bauformen geben stattdessen beispielsweise 22,86 mm (0,9 Zoll) oder 25,40 mm (1 Zoll) Reihenmittenabstand an. Die Angabe 36 mm darf **nicht** für den Footprint übernommen werden. Die exakte Boardvariante muss an den realen Steckleisten oder anhand eines passenden, maßhaltigen Hersteller-Footprints geklärt werden. Quellen: https://mokxi.com/docs/boards/esp32-devkit und https://electronics360.org/parts/esp32-devkit-v1

### Sensor-ESP32 Produktbild: Außenmaße und Adapter

Nutzerbild vom 15.11.2026: zeigt einen **30-Pin-ESP32-Entwicklungsboard** mit separater **Terminal-Adapterplatine**.

- ESP32-Außenmaß laut bemaßter Abbildung: **28 mm Breite × 51 mm Länge** (Länge der dargestellten Boardkontur; USB-C-Überstand berücksichtigen).
- Terminal-Adapterplatine: **66 mm × 63 mm**. Diese große Adapterplatine ist **nicht** Teil unserer Sensorplatine; unsere eigene Trägerplatine bekommt eine passende Buchsenleistenaufnahme.
- ESP32-Aufnahme: zwei Reihen mit **je 15 Steckkontakten**, nominell 2,54-mm-Längsraster.
- Bildbeschriftungen sind teilweise fehlerhaft/uneinheitlich: Zwischenmaße 5.1mm/2in, 4mm/1.57in, 2.6mm/1.02in sind nicht wörtlich als Millimeter verwendbar.
- **Pinreihen-Mittenabstand aus Bild nicht eindeutig belegt.** Bisherige Schätzung 35–36 mm bezieht sich offenbar nicht auf den reinen 28-mm-breiten ESP32; nicht in Footprint übernehmen. Noch messen oder originalen Adapter/Modul-Footprint verifizieren.

### Präzisierung: ca. 26 mm Abstand laut Adapterabbildung (15.11.2026)

Korrektur der bisherigen Bildinterpretation: Die Maßlinie zwischen den **inneren** Buchsenleisten ist in der Abbildung mit **„2.6 mm / 1.02 in“** bezeichnet. Der Millimetertext enthält ersichtlich einen Faktor-10-Fehler: **1,02 Zoll = 25,908 mm**, also **ca. 26 mm**. Dieses Bildmaß ist eine **Annäherung**, kein bestätigtes 25,4-mm- oder 26,0-mm-Nennmaß. **25,4 mm = 10 × 2,54 mm** wäre ein mögliches standardisiertes Reihenmaß, darf aber ohne Prüfung der realen Buchsenanordnung nicht als fertiger Footprint fixiert werden. Die früher genannten **35–36 mm** sind nicht der innere Reihenabstand.

### Vorläufiges Konstruktionsmaß: Sensor-ESP32 25,40 mm

Nutzer hält den aus der Produktabbildung abgeleiteten Standard-Reihenabstand von **25,40 mm** für wahrscheinlich. Für den 30-Pin-Sensor-ESP32 wird deshalb **2 × 15 Buchsenleisten mit 2,54 mm Längsraster und vorläufig 25,40 mm Reihenmittenabstand** als Konstruktionsansatz geführt. Modulaußenmaße laut Bild: 28 × 51 mm. **Status: vorläufig, nicht gemessen; keine Fertigungsfreigabe**, bis die reale Modulgeometrie bestätigt wurde. Die nominalen 25,40 mm dürfen nicht als bereits verifiziert ausgegeben werden.

### ADS1115: eine 10-polige Kontaktreihe

Nutzer bestätigt: Das vorhandene ADS1115-Breakout hat **eine einzige Reihe mit 10 Pins**, nicht zwei Pinreihen. Steckbar über **eine 1×10-Buchsenleiste**, Pinraster nach vorheriger Angabe nominal **2,54 mm**; Achsabstand erster–letzter Kontakt rechnerisch **9 × 2,54 = 22,86 mm**. Kein Reihenabstand erforderlich. Vor der Fertigungsfreigabe Außenkontur, USB-/Nachbarbauteilfreiraum (falls relevant), Orientierung und Lage des Pin-Headers auf dem realen Modul abgleichen. Die früheren Formulierungen »Pinreihen«/»Reihenabstand« für ADS1115 sind damit überholt.


## 7. Sensorplatine — passive Bauteile und Versorgungsbilanz

Stand 15.11.2026: Ableitung aus dem bereits dokumentierten Sensor-Pinout;
**nur eine passiv-elektrische Designvorbereitung**, keine fertige Schaltung.

### 7.1 Passiv-BOM mit bestückbaren und unbestückten Positionen

| Referenz | Anzahl | Wert / Typ | Bestückungsstatus | Grund |
| --- | ---: | --- | --- | --- |
| R_DHT1–3 | 3 | 4,7 kΩ, bedrahteter Keramikwiderstand aus Nutzerbestand; Bauform/Belastbarkeit prüfen | bedingt, nach Kontrolle bereits vorhandener Pull-ups | DATA nach 3V3 |
| R_DS18 | 1 | 4,7 kΩ, bedrahteter Keramikwiderstand aus Nutzerbestand; Bauform/Belastbarkeit prüfen | vorgesehen | DATA nach 3V3 |
| R_RGB_R/G/B | 3 | **440 Ω laut korrigierter Nutzerangabe, elektrisch noch zu verifizieren**, bedrahtete Positionen, Abmessungen offen | **DNP** bis LED-Typ und Sollstrom feststehen | je eine Strombegrenzung |
| R_PH_OBEN/UNTEN | 2 | **offen**, passende Widerstandspositionen vorsehen | **DNP** bis reale Eingangsbeschaltung und Kalibrierbezug geprüft | ADS1115 A0 |
| R_I2C_ADS_SDA/SCL | 2 | **offen**, optionale bedrahtete Positionen | DNP bis Modul-Pull-ups geprüft | I²C GPIO21/22 |
| R_I2C_BH_SDA/SCL | 2 | **offen**, optionale bedrahtete Positionen | DNP bis Modul-Pull-ups geprüft | I²C GPIO13/14 |

**Summe der aufgeführten Widerstandspositionen: 13**
(4 × 4,7-kΩ-Datenpull-up; 3 × RGB; 2 × pH; 4 × optionale I²C).
**Keine SMD-Widerstände:** vorhandene bedrahtete Keramikwiderstände verwenden. Konkrete Baugröße, Raster/Bohrungen und Leistungsdaten sind noch zu prüfen. Die optionale I²C-Bestückung erst nach Vermessung der Breakouts.

### 7.2 Nachgerechnete statische Pull-up-Last

Für den ungünstigen statischen Fall „DATA dauerhaft auf LOW“ und
3,3 V an 4,7 kΩ:

- Strom je Pull-up: **3,3 V / 4700 Ω ≈ 0,702 mA**.
- Verlustleistung je Widerstand: **3,3² / 4700 ≈ 2,32 mW**.
- Vier Pull-ups zusammen maximal: **≈ 2,81 mA**, bei gleichzeitig LOW
  liegenden Datenleitungen.

Die Rechnung bestimmt **nicht** die Last der Sensormodule, Kamera,
ESP32 oder optionalen I²C-Pull-ups. DHT22 und DS18B20 haben eigene
versorgungstechnische Anforderungen. Der GPIO-Ausgangsstrom der RGB-LED
bleibt mangels LED-Daten unbekannt.

### 7.3 Versorgungskonzept: bekannte Netze, offene Messgrößen

| Netz | Verbraucher / Funktion | Dimensionierungsstand |
| --- | --- | --- |
| USB-C 5V am Sensor-ESP32 | ESP32-Modul selbst | festgelegt; USB-Netzteil-Nennstrom unbekannt |
| 3V3 am ESP32-Modul | DHT22 ×3, DS18B20, ADS1115, BH1750, MLX90640*, Pull-ups, LED über Widerstände | Reglerreserve des realen 30-Pin-Boards unbekannt |
| 5V für analoge Module | PH4502C und EC/TDS, falls laut jeweiliger realer Modulbeschaltung erforderlich | **5-V-Abgriff und Belastbarkeit am ESP32-Board ungeprüft** |
| GND | gemeinsamer Signalrückleiter, stern-/rückstrombewusstes Layout | finale Masseführung erst mit Schaltplan |
| ADS1115 A0/A1 | analoge Eingangskreise | Eingangsspannung bei jeder Versorgungslage prüfen |

* MLX90640: im Layout vorgesehen, Firmwarefunktion aktuell ausgeschaltet.

**Keine verbindliche Strombudgetsumme möglich**, solange Board-Revision,
Regler-Daten, Sensormodultypen und tatsächlicher LED-Strom fehlen. Ein
5-V-Pin auf dem Board darf nicht ohne Schaltungsprüfung als
rückspeisefreier, belastbarer USB-Verteilausgang vorausgesetzt werden.
Die Spannung der 5-V-Analogmodule ist vor ADC-Eingang zu bewerten.
USB-C selbst liefert nicht automatisch beliebig viel Leistung.

### 7.4 Entwurfsregeln für den Sensor-Schaltplan

1. Anschlussnamen an allen 12 Klemmen gemäß `firmware/Sensor/PINOUT.md`
   fest zuordnen und eindeutig am Board beschriften.
2. Pull-up-Widerstände als **Verbindung DATA nach 3V3**, nie in Serie.
3. RGB common cathode: jede Farbe benötigt **ihren eigenen** Widerstand;
   ohne LED-Daten keinen Strom- oder Vorwiderstandswert erfinden.
4. ESP32 30 Pin: 2 × 15 Buchsen, 2,54 mm entlang, **25,40 mm
   Reihenmitte vorläufig**, Außenmaß 28 × 51 mm aus Bild.
5. ADS1115: **1 × 10 Buchsen**, 2,54 mm; Achsabstand erster–letzter
   Kontakt = **22,86 mm**. Pinreihenfolge vor Verbindung prüfen.
6. C3 Super Mini: 2 × 8 Buchsen, Reihenmitte **15,24 mm** laut Bild,
   Modul 18 × 22,52 mm; USB-C- und Antennenfreiraum.
7. Sensor-Signalpegel A02YYUW, ADS1115 A0/A1 und beiden I²C-Bussen
   nach konkreten Breakouts und realer Verdrahtung abgleichen.
8. **Kein PCB-Layout ohne Schaltplan/ERC, Footprint-Abgleich und DRC
   als bestellfertig bezeichnen.**

### 7.5 Noch fehlende Angaben, nach Priorität

**P1 vor Fertigung:** reale ESP32-Header-Mitten, ADS1115-Pinreihenfolge,
genaue GPIO-Beschriftung am 30-Pin-Board, realer 5-V-/3V3-Reglerpfad,
A0/A1-Pegel und Spannungsteiler.

**P2 für Bestückung:** LED-Daten, Modul-Pull-ups, tatsächlicher
5-V-Sensorstrom und Temperaturreserve, A02YYUW-Pegel.

**P3 Mechanik:** Gehäuse, Einbauhöhe der Steckmodule, Klemmenausrichtung,
Leitungsabgang und Befestigungsbohrungen.

### LED-Bestückung: ausschließlich vorhandene externe RGB-LED

**Verbindliche Nutzerfestlegung (15.11.2026):** Die einzige vorgesehene Status-LED ist die **bereits vorhandene externe RGB-LED mit gemeinsamer Kathode**. Sie wird über den dokumentierten vierpoligen Stecker `LED-RGB` (R=GPIO33, G=GPIO4, B=GPIO17, gemeinsame Kathode=GND) angeschlossen. Auf der Trägerplatine **keine zusätzlichen SMD-LEDs, keine Power-LEDs, keine separaten Status-LEDs** vorsehen. Die drei erforderlichen **Vorwiderstände** werden als vorhandene **bedrahtete Keramikwiderstände** auf der Platine vorgesehen; keine SMD-Widerstände und keine zusätzlichen LEDs. Die Vorwiderstandswerte sowie Körper- und Drahtmaße bleiben bis zur Prüfung der vorhandenen RGB-LED und Widerstände offen. Diese Festlegung betrifft die Trägerplatine; integrierte LEDs auf fertig gekauften ESP32-Modulen sind keine zusätzlich zu bestückenden Leiterplatten-LEDs.

### Widerstands-Bauform: Nutzerbestand, keine SMD-Widerstände

Verbindliche Nutzerfestlegung vom 15.11.2026: **Auf der Sensor-Trägerplatine keine SMD-Widerstände bestücken.** Sämtliche geplanten Widerstandspositionen sind für **vorhandene bedrahtete Keramikwiderstände** auszulegen. Das betrifft DHT22-/DS18B20-Pull-ups, RGB-Vorwiderstände, optional bestückbare I²C-Pull-ups sowie die beiden pH-Eingangswiderstände. **SMD-/0805-Vorschläge aus früheren Planungsständen sind hiermit überholt.**

Vor Footprint-Freigabe sind Körperlänge/-durchmesser, Drahtdurchmesser, benötigtes Anschlussraster, Widerstandswerte und zulässige Verlustleistung der tatsächlich vorhandenen Widerstände zu prüfen. Die frühere elektrische Vorgabe **4,7 kΩ** für vier Daten-Pull-ups wird dadurch nicht geändert; für alle weiteren Positionen bleiben Werte unbestätigt. „Keramikwiderstand“ allein belegt weder Wert noch Bauform.

### Vorläufiger RGB-Widerstandsbestand (15.11.2026)

**Korrigierte Nutzerangabe:** **3 × 440 Ω** für die drei RGB-LED-Kanäle Rot, Grün und Blau. Alle drei Widerstände sind **bedrahtet, keine SMD-Bauform**, gemäß Nutzerfestlegung. **Die Werte sind als Nutzerangabe eingeplant; elektrische Eignung und reale Kennzeichnung der vorhandenen Bauteile sind noch nicht nachgemessen bzw. anhand des LED-Datenblatts verifiziert.**

### RGB-Vorwiderstände: korrigiert auf 3 × 440 Ω

**Verbindliche Korrektur der Nutzerangabe (15.11.2026):** Die vorhandene externe RGB-LED mit gemeinsamer Kathode erhält **drei bedrahtete Vorwiderstände zu je 440 Ω**, einen je Farbe (R/G/B). Bisherige Angaben von 2 × 220 Ω oder 3 × 220 Ω sind **überholt**. Es werden keine SMD-Widerstände und keine zusätzlichen LEDs auf der Sensor-Trägerplatine vorgesehen. **Elektrische Eignung der 440 Ω für das tatsächliche LED-Modell sowie die tatsächlichen Teilekennzeichnungen sind noch nicht geprüft.**
