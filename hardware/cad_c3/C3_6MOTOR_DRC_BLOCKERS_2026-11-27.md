# C3: 6 motor-channel placement / NOT FOR ORDER
**Stand:** 27.11.2026 · **Status:** DRAFT, **kein** Fertigungsdatensatz.

Der generische KiCad-Platinenentwurf `c3_6motor_onoff_DRAFT_NOT_FOR_ORDER.kicad_pcb` hat 6 Kandidaten für unabhängige 12-V-Motorschalter als Footprints im Layout. Die 6 Pumpenklemmen wurden zugeordnet. Weitere Schnittstellen, insbesondere netzbetriebene Lasten, sind unverbunden.

## Neue Bauteilpositionen im Entwurf
- 6× `QM1..QM6` AO3400A-Kandidat (N-MOSFET, SOT-23)
- 6× `RG1..RG6` R0805-Kandidat (Gate)
- 6× `RPD1..RPD6` R0805-Kandidat (Pulldown)
- 6× `DM1..DM6` SS34-Kandidat (induktive Lastbegrenzung)
- 6 schon reservierte Phoenix-Motoranschlüsse (J7..J12; nicht doppelt zählen)
- Entwurf insgesamt 45 Footprints und 170 Leiterbahnsegmente; **keine Gesamtfreigabe**.

## Echte KiCad-9-Prüfung
CI: https://github.com/d4sd1ng/homepage/actions/runs/45935698134

**DRC fehlgeschlagen: 44 Verstöße/Warnungen**. Darunter:
- 6 Via-Bohrungen 0,20 mm gegenüber im Board eingestelltem Minimum 0,30 mm;
- 6 Vias mit Durchmesser 0,40 mm gegenüber Minimum 0,50 mm;
- 24 Silkscreen-Referenztexte zu klein (<0,80 mm);
- 6 Silkscreen-Überlagerungen;
- 2 frei endende Leiterbahnen;
- 1 ungeklärte Verbindung (PCA9685 /OE).
Der DRC-Bericht enthält diese Verstoßkategorien und weitere Zählungen. Diese Datei darf nicht als Gerber-Quelle zum Bestellen benutzt werden.

## Weitere Freigabegrenzen
- Der generische Motorentwurf ist bislang nicht am realen Motor geprüft (unbekannter Anlauf-/Blockierstrom).
- Keine vollständige Schaltung mit ERC, kein vollständiger Last- und Sicherheitsnachweis, keine freigegebene Stückliste.
- Keine Validierung für chemisch gefährliche Fördermedien oder Netzspannung.
