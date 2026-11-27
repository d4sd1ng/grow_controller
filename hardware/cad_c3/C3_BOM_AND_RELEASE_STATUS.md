# C3-Trägerplatine – Bauteile und Freigabestand
Stand: 25.11.2026 · Quelle: `hardware/pcb_c3_super_mini_plan.md`, `firmware/C3/C3.ino`.
**Dies ist der CAD-Aufstellungsentwurf, NICHT die bestellfähige Platine.** Keine Leiterbahnen, Schutzstufen oder elektrisch freigegebenen Lastanschlüsse vorhanden.

## Bereits festgelegt
| Bauteil/Anschluss | Menge | Spezifikation | Entscheidung |
|---|---:|---|---|
| U_C3 | 1 | ESP32-C3 SuperMini, 2×8 Kontakte | Steckbar, 5 V vom platineninternen 12→5-V-Step-down, USB-C weiterhin für Service (Strompfad-Entkopplung erforderlich) |
| Buchsenleisten für C3 | 2 | THT 1×8, 2,54 mm, passende Steckhöhe | Pin-/Sockelgeometrie mit vorhandenem Modul abgleichen |
| U_PCA | 1 | PCA9685, im CAD als **PCA9685PW / TSSOP28** gezeichnet | CHIP-Variante **noch nicht** als verbindliches Kaufteil bestätigt |
| J1–J12 Leiterplatten-Grundleisten | 12 | Phoenix MSTBA 2,5/2-G-5,08, Art. **1757242** | im mechanischen CAD-Platzierungsstand |
| J1–J12 Gegenstecker | 12 | Phoenix MSTB 2,5/2-ST-5,08, Art. **1757019** | passend zur Grundleiste |
| J13 Leiterplatten-Grundleiste | 1 | Phoenix MSTBA 2,5/3-G-5,08, Art. **1757255** | im mechanischen CAD-Platzierungsstand |
| J13 Gegenstecker | 1 | Phoenix MSTB 2,5/3-ST-5,08, Art. **1757022** | passend zur Grundleiste |
| H1–H4 | 4 | M3-Platinenbohrung, Ø 3,2 mm | Positionen im *vorläufigen* Floorplan |
| U_BUCK | 1 | **Vorgeschlagener** RECOM R-78E5.0-1.0 (8–28 V Eingang, 5 V/1 A, SIP3 2,54 mm) | Footprint im KiCad platziert; **Kandidat**, noch nicht als Einkaufsteil freigegeben; Logikstrombedarf prüfen |
| 12-V-Leistungszweig | 1 | interne Einspeisung mit getrenntem Logik- und Lastpfad | Keine externe 5-V-Versorgung mehr erforderlich; Sicherung/Schutz vor Routing zu prüfen |

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
3. 12-V→5-V-Step-down (Modell, Pinout, mechanisches Footprint, Wärmeabfuhr und Strombilanz); USB-C und eingespeiste 5 V müssen elektrisch gegeneinander verriegelt/entkoppelt werden. Die 3,3-V-Logiklast bleibt ebenfalls zu prüfen.
4. Steckverbinderabgänge, USB-Zugang und tatsächliche Befestigungsmaße.
5. Schaltplan mit ERC; vollständiges Routing, KiCad-DRC und vollständige PTH/NPTH-Fertigungsdaten.
6. Verifikation auf realer Niederspannungshardware vor Betrieb.

**Es ist ausdrücklich keine Bau- oder Anschlussanleitung für Netzspannung oder konzentrierte Säure enthalten.**

## Verbindliche Änderung vom 27.11.2026: 12 V als einzige Betriebseinspeisung
- Die frühere Entscheidung „C3 nur über USB-C mit 5 V versorgen“ ist **aufgehoben**.
- Ein **12-V-Eingang** auf der C3-Platine versorgt zwei getrennte funktionale Bereiche: den vorgesehenen 12-V-Lastzweig und einen **auf der Platine befindlichen Step-down-Wandler 12 V → geregelte 5 V**.
- Der C3-SuperMini erhält im regulären Betrieb 5 V **ausschließlich vom Step-down-Ausgang über einen geeigneten geschützten Versorgungspfad**; USB-C bleibt zum Flashen/Service zugänglich, darf aber nicht ungeschützt parallel mit dem Step-down betrieben werden.
- Der PCA9685 benötigt weiterhin 3,3 V Logik (nicht 5 V oder 12 V), die Stromquelle ist an der realen Modul-/Reglerversion zu überprüfen.
- J1 ist ab jetzt **12V IN** (+12V / GND), keine externe 5-V-Einspeisung.
- Zu prüfen: DC/DC-Eingangsbereich, tatsächliche Dauerlast bei der Einbauumgebung, Startstrom, Überstrom-/Verpolschutz, Masseführung, USB-Rückspeiseschutz und thermische Abstände. Ohne ausgewählten realen Konverter kein seriöses Montage-Footprint.
- Diese Änderung ist im CAD-Floorplan an J1 und C3 als elektrische **Netzzuordnung** ergänzt, bleibt aber **unroutet**. Ein provisorischer SIP3-Step-down-Footprint ist integriert, verbindliche Bauteilwahl offen.

## Nachtrag 27.11.2026 – KiCad-Kandidat eingearbeitet
- Angelegtes Footprint `RECOM_R-78E5.0-1.0_SIP3` (Pin 1 = Vin, 2 = GND, 3 = Vout), 2,54 mm Raster, Package 11,6 × 8,5 mm (Datenblatt RECOM, Revision 2019). Herstellerseite: https://recom-power.com/en/products/switching-regulators/switching-regulators-sip/rec-p-R-78E5.0-1.0.html
- Vorläufig bei (48, 52) mm platziert. Alle drei Pads als **ungeroutete** Versorgungsnetze benannt.
- Physikalisches C3-Pinout korrigiert: USB-C oben, **rechts 5V/GND/3V3/GPIO4..GPIO0**, links GPIO5..GPIO21. Beide Kontakte im Footprint und im eingebetteten Board gespiegelt.
- Die 5-V-Versorgung ist noch **nicht mit USB-VBUS entkoppelt**; deshalb keine Bestell-/Betriebsfreigabe und keine Leiterbahnverbindungen.
- Die Quelle weist 5 V bei max. 1 A aus; daraus folgt **nicht**, dass die gesamte spätere Schaltung zuverlässig versorgt werden kann. Verbraucherlast und Umgebung erst bestimmen.

## Pumpenausgänge – Hardware-Scope 27.11.2026
Die C3-Platine benötigt **sechs unabhängig EIN/AUS schaltbare 12-V-Ausgänge**. Auf dieser Platine wird keine Regel- oder Dosierlogik umgesetzt. Die sechs Kanäle sind reine elektrische Niederspannungs-Schaltausgänge.

Zusätzlich zur schon gezählten Anschlussleiste sind für die sechs Schaltstufen vorgesehen: **6× N-Kanal-Logic-Level-MOSFET, 6× Gate-Serienwiderstand, 6× Gate-Pulldown, 6× Motorschutzbauteil**. Das sind Bauteilpositionen, noch **keine endgültige Bestellliste**; Werte, Footprints und Stromtragfähigkeit müssen geprüft werden. Die sechs bereits vorgesehenen Phoenix-Motorklemmen dürfen in der Stückliste **nicht doppelt** gezählt werden.

Siehe: `C3_MOTOR_ON_OFF_REQUIREMENTS_2026-11-27.md`. Die Firmware-Konfiguration mit gesperrten Pumpenkanälen bleibt unverändert.
