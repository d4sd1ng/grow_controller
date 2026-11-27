# C3 12-V – Bestückungsstückliste aus dem aktuellen PCB

**CAD-Quelle:** `hardware/cad_c3/c3_12v_5signals_1fan_WIP_NOT_FOR_ORDER.kicad_pcb`  
**Blob-SHA:** `c80686158a4673c9c63ff8fc010b6f9c93359b61`  
**Stand:** 27.11.2026  
**Umfang:** 54 CAD-Footprints = **50 Bestückungspositionen + 4 M3-Montagebohrungen**.  
**Wichtig:** Die Liste bildet den tatsächlichen PCB-Bestand ab. Sie ist keine vollständige Freigabe von Lieferteilen oder Fertigungsdaten.

| Referenzen | Wert / Bauteil | Menge 1 PCB | Menge 20 PCB | Footprint / Bauform | Prüfhinweis |
|---|---|---:|---:|---|---|
| U_C3 | ESP32-C3 SuperMini | 1 | 20 | 2×8-Modulsockel, USB-C | Modul-Variante prüfen |
| U_PCA | PCA9685PW | 1 | 20 | TSSOP-28 | Chip, kein PCA9685-Breakout-Modul |
| J1 | Same Sky PJ-037AH | 1 | 20 | DC-Hohlbuchse, THT links | Steckpassung und Polarität prüfen |
| J2–J12 | Phoenix Contact 1757242 | 11 | 220 | 2-polig, Raster 5,08 mm | Gegenstecker separat |
| J13 | Phoenix Contact 1757255 | 1 | 20 | 3-polig, Raster 5,08 mm | Pin 3 im PCB unbeschaltet |
| U_BUCK | RECOM R-78E5.0-1.0 | 1 | 20 | SIP-3 | 12 V auf 5 V; tatsächliche Belastung prüfen |
| D1 | 1N5819 | 1 | 20 | DO-41, P10.16 | Hersteller und Polarität prüfen |
| QM1–QM6, QFAN | AO3400A | 7 | 140 | SOT-23 | 6 Schaltkanäle + 1 Lüfterkanal |
| RG1–RG6, RGFAN | 100 Ω | 7 | 140 | 0805 | Gate-Serienwiderstände |
| RPD1–RPD6, RPDFAN | 100 kΩ | 7 | 140 | 0805 | Gate-Pulldown |
| DM1–DM6, DFAN | SS34 | 7 | 140 | SMA | Dioden |
| R_SDA, R_SCL | 4,7 kΩ | 2 | 40 | 0805 | I²C-Pull-ups |
| R_OE | 10 kΩ | 1 | 20 | 0805 | Ausgangssperre |
| C_PCA | 100 nF | 1 | 20 | 0805, generisches R0805-Pad im CAD | Keramik, MPN/Spannung offen |
| JP1 | 2-polige Stiftleiste | 1 | 20 | Raster 2,54 mm | Kurzschlussbrücke extra |
| **Summe** | | **50** | **1000** | | |

## Zusätzlich benötigt – nicht als bestückte PCB-Bauteile gezählt

| Bauteil / Zubehör | Menge für 1 | Menge für 20 | Hinweis |
|---|---:|---:|---|
| Unbestückte PCB | 1 | 20 | Fertigungsdaten noch freizugeben |
| C3-Buchsenleisten, 1×8 / 2,54 mm | 2 | 40 | Steckhöhe mit Modul prüfen |
| Phoenix-Gegenstecker 1757019 (2-polig) | 11 | 220 | Für J2–J12 |
| Phoenix-Gegenstecker 1757022 (3-polig) | 1 | 20 | Für J13 |
| Kurzschlussbrücke 2,54 mm | 1 | 20 | Für JP1 |
| 12-V-Netzteil GST60A12-P1J (Kandidat) | 1 | 20 | Steckpassung und Lastreserve prüfen |
| M3-Montageschrauben (optional) | 4 | 80 | Einbaulänge prüfen |
| M3-Abstandshalter (optional) | 4 | 80 | Einbaulänge prüfen |

## Offene Punkte vor verbindlichem Einkauf

- J13-Pad 3 besitzt im aktuellen CAD kein Netz.
- Einige Pins an U_C3 und U_PCA sind nicht zugeordnet (möglicherweise bewusst ungenutzt; Schaltplanabgleich fehlt).
- Bei mehreren Widerständen, dem Kondensator, den Dioden und dem Steckjumper ist noch kein eindeutiger Hersteller-/Distributor-Code festgelegt.
- Die PJ-037AH-Buchse wird mit 2,0-mm-Mittelstift angegeben; mechanische Steckpassung zum vorgesehenen Netzteil prüfen.
- 12-V-Stromreserve, Eingangsschutz, USB-Serviceversorgung und Platinen-Fertigungsunterlagen vor Bestellung verifizieren.

Die **20er-Spalte ist eine reine Mengenumrechnung**. Bei einer anderen Platinenzahl entsprechend multiplizieren.
