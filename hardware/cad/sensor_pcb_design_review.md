# Sensorplatine — KiCad-Entwurfsprüfung (15.11.2026)

**Referenz:** `sensor_pcb_unrouted_prototype.kicad_pcb` und
`hardware/sensor_pcb_external_pins.csv`,
`hardware/sensor_pcb_module_pins.csv`.

**Status: Entwicklungsstand, nicht bestell-/fertigungsfähig.**

## 1. Rechnerischer CAD-Inventurcheck

| Prüfgröße | Stand |
| --- | --- |
| Externe Phoenix-Steckplätze | 12 (2 × 2-polig, 6 × 3-polig, 4 × 4-polig) |
| Externe Kontakte | 38 |
| Steckmodul-Kontakte | 40 (ESP32 30, ADS1115 10) |
| Bedrahtete Widerstandspositionen | 13, je 2 Pins = 26 |
| Gesamtpads | 104 |
| CAD-Bauteilpositionen | 27 |
| KiCad-Netznamen | 46 |
| Leiterbahnen und Vias | **0** |
| Mechanische Außenkontur | **vorläufig** 179 × 115 mm |

### Steckverbinder-Geometrie

Phoenix Contact **MSTBA 2,5/..-G-5,08**, Raster 5,08 mm.
Der Hersteller gibt für **alle drei exakt ausgewählten Grundleisten**
(1757242 = 2-polig, 1757255 = 3-polig, 1757268 = 4-polig)
einen **PCB-Bohrlochdurchmesser von 1,4 mm** bei 5,08-mm-Pinraster an.
Dieser zuvor offene Punkt ist somit anhand der Originalproduktseiten
**geschlossen**.

Im CAD sind nun **38 Kontaktbohrungen mit 1,4 mm Durchmesser** und
provisorisch **2,5-mm-Rundpads** hinterlegt. **Pad-Außendurchmesser
und Kupferring sind eigene Entwurfswerte**, keine Herstellerangabe.
Das Körperumriss-/Steckraumprofil im CAD bleibt ein **Platzhalter**.
Es muss durch passende Hersteller-Footprints bzw. kontrollierte
CAD-Konturen ersetzt werden.

Hersteller:
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-2-g-508-1757242
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-3-g-508-1757255
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-4-g-508-1757268

## 2. Kritische, aktuell nicht geschlossene Netze

**Der CAD-Vorentwurf enthält Pad-Netz-Zuordnungen,
aber noch keinerlei Leiterbahnen; auch gleich benannte Netze
sind physisch nicht verbunden.** Darüber hinaus sind
einige Signale bewusst nicht vollständig spezifiziert:

| Punkt | Ist-Stand / offene Entscheidung |
| --- | --- |
| ADS1115 ADDR / 0x48 | ADS-Modul-Pin 5 liegt am Prüfnetz `ADS_ADDR_GND_UNVERIFIED`, nicht an verifiziertem GND; erst echte Modulkonfiguration prüfen |
| ADS1115 A0 | `PH_A0_CONDITIONED_UNVERIFIED` und `PH_A0_MID` sind derzeit **verschiedene** Netze; Eingangskreis absichtlich noch nicht fertig |
| ADS1115 A1 | `EC_A1_CONDITIONED_UNVERIFIED` und `EC_ANALOG_UNVERIFIED` sind noch nicht verbunden; elektrische Pegelprüfung erforderlich |
| PH4502C GND1 und GND2 | Zwei getrennte Prüfnetze; gemeinsame Beschaltung nicht ohne Prüfung annehmen |
| 5-V-Versorgung Analogmodule | `ANALOG_5V_UNVERIFIED` nicht an `5V_USB_BOARD` angeschlossen |
| A02YYUW und MLX90640 | Vorläufig separate `3V3_UNVERIFIED`-Netze bis Modulversorgung geprüft |
| I²C-Pull-ups | Vier optionale Widerstandspositionen; Modulbestückung vorab klären |
| 3,3-V-Strombudget | Leistungsfähigkeit des ESP32-Modulreglers für alle Sensoren ungeprüft |
| Montage | Endgültige Modulaußenmaße, Steckerabgang, Bauhöhen, Bohrlöcher und Gehäuse nicht erfasst |
| Firmware | MLX90640 ist im aktuellen Sensor-Sketch noch deaktiviert |

**Wichtig:** Keines der unbestätigten Sensorversorgungs- oder
Analogprüfnetze darf durch automatische, willkürliche
Netzzusammenführung geschlossen werden.

## 3. Benutzervorgaben, die nicht erneut offen sind

- ESP32 30-pin 2×15, Raster 2,54 mm, Konstruktions-Reihenabstand
  **25,40 mm**, Modulnennmaß 28 × 51 mm.
- ADS1115 1×10, Raster 2,54 mm.
- C3 (separate Platine) 2×8, 2,54 mm, 15,24 mm Reihenabstand.
- Beide ESP32 steckbar; jeweils 5 V ausschließlich über ihr USB-C.
- Externe vorhandene RGB-LED mit gemeinsamer Kathode,
  **keine zusätzlichen LEDs** auf Sensorträger.
- **Keine SMD-Widerstände**: nur bedrahtete Nutzerbauteile.
- Für RGB bevorzugt drei **einzelne** 220-Ω-Widerstände,
  nach Prüfung der vorhandenen LED.
- MLX90640 erhält den **eigenen vierpoligen Stecker `J_MLX`**.

## 4. Abgrenzung der Prüfungen

**Durchgeführt:** Anschlusszählung (38 + 40 + 26 = 104),
Ersetzen der 38 provisorischen 1,2-mm-Steckerbohrungen durch
1,4-mm-Bohrungen; Dokumentation der Netz-Lücken.

**Nicht durchgeführt:** KiCad-Parser-/ERC-/DRC-Lauf,
vollständiger funktionsfähiger Schaltplan,
Leiterbahnrouting, Versorgungslastmessung,
herstellerseitige Komplett-Footprint-Verifikation,
Gerber- und Excellon-Ausgabe.

**Nicht zur Herstellung einreichen.**

## 5. Neue Fertigungsprüfung: Kontaktgehäuse und Netzgrenzen (15.11.2026)

- Die 12 Phoenix-Stecker im KiCad-Vorentwurf haben jetzt **vorläufige
  12-mm-tiefe F.Fab-Konturen**; die **herstellerbestätigten** Gehäusebreiten
  sind je nach Polzahl 12,16 mm (2-polig), 17,24 mm (3-polig) und
  22,32 mm (4-polig). 5,08-mm-Raster und 1,4-mm-Bohrung sind bestätigt.
- Die Connector-Platzhalter wurden um 4 mm nach rechts verschoben,
  die sechs unteren Stecker in einen vorläufigen Innenbereich bei Y=114 mm.
  **PCB-Außenmaß bleibt ein Entwurf von 179 × 115 mm**; es wurde nicht
  vom Nutzer als Gehäusemaß freigegeben.
- Das mechanische Rechteck ist weiterhin **kein vollständiges Phoenix-
  Hersteller-Footprint**: Steckrichtung, Einsteckbereich und die tatsächliche
  Lage des Kunststoffgehäuses gegenüber der Lochreihe müssen mit
  Hersteller-CAD abgeglichen werden. DRC/3D-Kollisionsprüfung fehlt.

### Herstellerbelegte Sensorversorgung

Der echte **DFRobot A02YYUW (SEN0311)** ist laut Hersteller für
**3,3–5,0 V** mit typischen maximal **8 mA Durchschnittsstrom** vorgesehen.
Ausgang UART 9600 Baud mit vier Sensorkontakten VCC, GND, RX, TX;
**RX darf offen bleiben** (herstellerseitiger Modus für
verarbeitete, stabilisierte Werte). Die von uns geplante
3-polige Verbindung VCC/TX/GND ist damit auf **diesen
Herstellersensor** bezogen technisch plausibel. Bei baugleichen
Nachbauten ist die elektrische Identität noch zu prüfen.
Quelle: https://wiki.dfrobot.com/sen0311/

Die **MLX90640 ist nicht vergessen**: Stecker J_MLX vierpolig,
SDA GPIO21/SCL GPIO22, Sensoradresse 0x33. Für konkret untersuchte
Breakouts von Adafruit oder Pimoroni sind 3,3 V zulässig; die
tatsächliche Breakout-Variante des Nutzers bleibt zu identifizieren.
Quellen:
- https://learn.adafruit.com/adafruit-mlx90640-ir-thermal-camera/pinouts
- https://shop.pimoroni.com/products/mlx90640-thermal-camera-breakout

### Relevante ADS1115-Grenzwerte (TI-Datenblatt SBAS444E)

- **Empfohlener analoger Arbeitsbereich:** GND bis VDD.
- **Absolute Grenzwerte:** GND−0,3 V bis VDD+0,3 V.
- Bei ADS1115-VDD = 3,3 V sind also **0 bis 3,3 V im zulässigen
  Arbeitsbereich**, **3,6 V lediglich absolute Obergrenze**,
  ausdrücklich keine normale Zielspannung.
- Ein angezeigter PGA-Messbereich von ±4,096 V oder ±6,144 V ist **kein**
  Nachweis einer 5-V-toleranten Analogeingangsstufe.
- Der tatsächliche PH4502C-PO-Pegel und die Ausgangspegel des EC-Moduls
  unter allen Betriebsbedingungen müssen vor dem Anschluss nachgewiesen
  werden; **keine erdachte Spannungsteilerbestückung und kein
  Direktanschluss unbewiesener 5-V-Signale**.

TI-Quelle: https://www.ti.com/lit/ds/symlink/ads1115.pdf

### Status der elektrischen Verbindungen

**Noch nicht freigegeben:** 3,3-V-Gesamtstrom (ESP32-Boardregler
unbekannt), PH4502C-GND-Topologie, A0-Spannungsteiler,
EC-A1-Eingangsbeschaltung, Kamera-Breakout-Variante,
I²C-Pull-ups und die **routbaren** finalen Netze.
Die Einzelmodule und die provisorische Platinenkontur wurden
noch nicht am realen Aufbau gemessen.

## 6. Versorgungsfestlegung pH und EC

Nutzerangabe 15.11.2026: **pH- und EC-Module werden aus der Versorgung des Sensor-ESP32-Boards gespeist**, nicht durch separate externe Netzteile. Planungsabsicht: Sensor-ESP32 erhält 5 V ausschließlich über USB-C; PH4502C und EC/TDS-Module erhalten die für sie spezifizierte Betriebsspannung aus diesem Board-Versorgungszweig. Für bisherige Modulplanung sind 5-V-Versorgungsanschlüsse vorgesehen; deren tatsächliche elektrische Eignung und Stromreserve sind noch zu prüfen. **Die Datenmessung erfolgt weiterhin über den steckbaren ADS1115 (A0 pH, A1 EC/TDS)**, nicht über direkte ESP32-ADC-Pins.

**Fertigungskriterium offen:** Schaltungsprüfung des konkreten 30-Pin-DevKit: Ist sein 5-V/VIN-Pin bei Einspeisung über USB-C als belastbarer Spannungsausgang ausgeführt? Ist eine Diode/Schutzschaltung vorhanden? Wie groß ist die verfügbare Stromreserve, und ist keine unzulässige Rückspeisung möglich? Die Antwort ist aus der bisherigen Pinout-Zeichnung **nicht** ableitbar. Die beiden Analogeingangspegel bleiben unabhängig davon zu prüfen (ADS1115 3,3 V).
