# C3-Steuerplatine – KiCad-Prüfstatus 27.11.2026

## Durchgeführte Arbeit
- Neu erzeugter, separater KiCad-Entwurf `c3_power_path_PARTIAL_NOT_FOR_ORDER.kicad_pcb` für **den 12-V-/5-V-Versorgungszweig**.
- Versorgungsnetzzuordnung `+12V_IN` → RECOM-Wandlerkandidat → `+5V_BUCK` → Diodenpfad → `+5V_PROTECTED` (C3).
- 17 Leiterbahnsegmente für die Stromversorgung; übrige Anschlüsse bleiben elektrisch unverbunden.
- Position des ESP32-C3 im KiCad-Datensatz an das gebräuchliche SuperMini-Pinout angepasst.
- Mechanische Footprint-Bibliothek mit dem Board abgeglichen; korrigierter, nicht rotierter Dioden-Footprint.
- **KiCad 9 strenger DRC ohne Verstöße**: 0 DRC-Verstöße, 0 unconnected pads, 0 Footprint-Fehler, Export SVG bestanden.
- Nachweis: https://github.com/d4sd1ng/homepage/actions/runs/45932197583

## Grenzen des Testergebnisses
**DRC=0 ist hier keine Aussage über Funktionsfähigkeit oder fertiges Gesamtlayout.**
Derzeit nicht vernetzte Anschlussplätze erzeugen ohne elektrische Netzzuordnung keine Unrouted-Fehler. Die vollständige Netzliste fehlt. Insbesondere:
- Kein vollständiger Schaltplan mit ERC / systematischem Pin-zu-Netz-Abgleich vorhanden.
- Weitere Baugruppen des Gesamtcontrollers sind nicht elektrisch ausgelegt oder geroutet.
- USB-Stromquellenumschaltung, Versorgung unter gleichzeitiger USB-Verbindung und reale 5-V-Last sind noch nicht am konkreten Modul gemessen bzw. geprüft.
- Schutzkomponenten, Entkopplungskondensatoren, tatsächliche Lieferteile und thermische Auslegung nicht abschließend spezifiziert.
- PCB-Außenmaße und mechanische Maße nicht für das konkrete Gehäuse bestätigt.
- Für den vollständigen Controller liegen keine freigegebenen Bohrdaten, Gerber- und Bestückungsunterlagen vor.

## Freigabe
**NICHT BESTELLFERTIG: keine Fertigungsgerber für den Gesamtcontroller ableiten.**
Eine unabhängige fachliche Prüfung sicherheitskritischer Lastschnittstellen und Chemikalienförderung ist erforderlich. Dieser Status dokumentiert ausschließlich die generische Kleinspannungsversorgung und den bestehenden mechanischen Entwurf.