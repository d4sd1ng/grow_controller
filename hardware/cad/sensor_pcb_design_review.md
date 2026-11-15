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
Für die ausgewählten zwei- und vierpoligen Grundleisten gibt der
Hersteller einen **PCB-Bohrlochdurchmesser von 1,4 mm** an; bei der
dreipoligen Ausführung aus derselben Modellfamilie ist der Lochwert
noch gegen genau deren Datenblatt zu kontrollieren.

Im CAD sind nun **38 Kontaktbohrungen mit 1,4 mm Durchmesser** und
provisorisch **2,5-mm-Rundpads** hinterlegt. **Pad-Außendurchmesser
und Kupferring sind eigene Entwurfswerte**, keine Herstellerangabe.
Das Körperumriss-/Steckraumprofil im CAD bleibt ein **Platzhalter**.
Es muss durch passende Hersteller-Footprints bzw. kontrollierte
CAD-Konturen ersetzt werden.

Hersteller:
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-2-g-508-1757242
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
