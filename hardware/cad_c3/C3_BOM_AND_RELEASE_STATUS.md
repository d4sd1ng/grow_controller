# C3-Trägerplatine – Bauteile und Freigabestand
Stand: 25.11.2026 · Quelle: `hardware/pcb_c3_super_mini_plan.md`, `firmware/C3/C3.ino`.
**Dies ist der CAD-Aufstellungsentwurf, NICHT die bestellfähige Platine.** Keine Leiterbahnen, Schutzstufen oder elektrisch freigegebenen Lastanschlüsse vorhanden.

## Bereits festgelegt
| Bauteil/Anschluss | Menge | Spezifikation | Entscheidung |
|---|---:|---|---|
| U_C3 | 1 | ESP32-C3 SuperMini, 2×8 Kontakte | Steckbar, USB-C 5 V als **einzige** C3-Versorgung |
| Buchsenleisten für C3 | 2 | THT 1×8, 2,54 mm, passende Steckhöhe | Pin-/Sockelgeometrie mit vorhandenem Modul abgleichen |
| U_PCA | 1 | PCA9685, im CAD als **PCA9685PW / TSSOP28** gezeichnet | CHIP-Variante **noch nicht** als verbindliches Kaufteil bestätigt |
| J1–J12 Leiterplatten-Grundleisten | 12 | Phoenix MSTBA 2,5/2-G-5,08, Art. **1757242** | im mechanischen CAD-Platzierungsstand |
| J1–J12 Gegenstecker | 12 | Phoenix MSTB 2,5/2-ST-5,08, Art. **1757019** | passend zur Grundleiste |
| J13 Leiterplatten-Grundleiste | 1 | Phoenix MSTBA 2,5/3-G-5,08, Art. **1757255** | im mechanischen CAD-Platzierungsstand |
| J13 Gegenstecker | 1 | Phoenix MSTB 2,5/3-ST-5,08, Art. **1757022** | passend zur Grundleiste |
| H1–H4 | 4 | M3-Platinenbohrung, Ø 3,2 mm | Positionen im *vorläufigen* Floorplan |

**Steckverbinder gesamt:** 13 Grundleisten + 13 Gegenstecker. Anschlüsse J1–J12 sind im CAD derzeit **ohne Netz**; der Druck "reserved" ist keine Betriebserlaubnis.

## Nicht kaufen, bevor elektrisch geprüft
- Motortreiber/MOSFET-Stufen samt Freilauf-/Schutzbauteilen, Sicherungen und Netzteil: **Typen und Dimensionierung nicht festgelegt**.
- Eingangsstufen für 5 externe SSR-Steuersignale: Strom-/Pegeldaten, Entkopplung und Schutz noch ungeprüft; **keine Netzspannungsverdrahtung auf dieser Platine**.
- Lüftertreiber-Eingang: gemeinsamer Kanal festgelegt, reale Steuerart (PWM, Enable, etc.) unbekannt.
- PCA9685-Stützkondensator, Pull-ups und Freigabeschaltung: Werte/Leistungsbudget/Pinbelegung vor Bestückungsfreigabe als Schaltplan mit ERC festzulegen.
- Unabhängige Verriegelung der gefährdungsrelevanten Ausgänge: noch nicht nachgewiesen; der PCA-/OE-Pin sperrt **alle** PCA-Ausgänge gemeinsam, nicht nur eine Lastgruppe.

## Festgelegte Softwareanschlüsse (kein Routing im Floorplan)
| Funktion | C3-GPIO | Firmware |
|---|---:|---|
| Logiksignal 1 | 0 | PIN_SSR[0] |
| Logiksignal 2 | 1 | PIN_SSR[1] |
| Logiksignal 3 | 3 | PIN_SSR[2] |
| Logiksignal 4 | 4 | PIN_SSR[3] |
| Logiksignal 5 | 5 | PIN_SSR[4] |
| I²C SDA | 7 | PCA9685 |
| I²C SCL | 10 | PCA9685 |
| PCA9685 /OE | 20 | globales Sperrsignal |
| PCA OUT0…OUT5 | — | sechs reservierte Kanäle, Firmware gesperrt |
| PCA OUT6, OUT7 | — | historisch zwei Lüftergruppen; Hardwareforderung aktuell ein gemeinsamer Lüftertreiber |

## Mechanischer CAD-Stand
- `c3_floorplan_UNROUTED_NOT_FOR_ORDER.kicad_pcb`: **160 × 116 mm ausschließlich als Aufstellungsstudie**, nicht festgelegtes Fertigungsmaß.
- ESP32-C3-Körper 18 × 22,52 mm, 2×8 Pads; Antennen- und USB-Zonen sichtbar markiert.
- PCA9685PW als SMT-Footprint vorgeschlagen (TSSOP28); noch keine Verbindung zu C3.
- Alle 13 Phoenix-Anschlussplätze mechanisch angeordnet; vier M3-Kandidatenpositionen eingezeichnet.
- **0 Leiterbahnsegmente.** Es gibt weder einen vollständigen Schaltplan/ERC noch einen Routing-/DRC-Freigabenachweis. **Keine Gerber-Dateien zur Bestellung ableiten**.

## Für eine echte Fertigungsfreigabe fehlen
1. Bestätigung der tatsächlich vorhandenen PCA9685-Chip- oder Modulvariante.
2. Konkrete, geprüfte Niederspannungs-Treiber und Schutzbeschaltung.
3. 5-V/3,3-V-Strombilanz ohne Rückspeisung vom USB-C-Modul.
4. Steckverbinderabgänge, USB-Zugang und tatsächliche Befestigungsmaße.
5. Schaltplan mit ERC; vollständiges Routing, KiCad-DRC und vollständige PTH/NPTH-Fertigungsdaten.
6. Verifikation auf realer Niederspannungshardware vor Betrieb.

**Es ist ausdrücklich keine Bau- oder Anschlussanleitung für Netzspannung oder konzentrierte Säure enthalten.**
