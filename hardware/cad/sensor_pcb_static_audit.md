# Sensor-PCB — aktueller statischer Prüfstand (15.11.2026)

**Maßgebliche PCB-Datei:** `sensor_pcb_routed_candidate.kicad_pcb`.
Ausführlicher aktueller Fertigungsstatus:
`sensor_pcb_routed_review.md`.

Der ursprüngliche Teilrouting-Stand mit 81 Segmenten ist **veraltet**.
Die damals angenommene pH-Spannungsteilerbestückung wurde
korrigiert; **pH PO → ADS1115 A0 direkt**.

## Ergebnis der eigenen statischen Kontrollen

| Kennzahl | Aktueller Stand |
|---|---:|
| PCB-Footprints | 25 |
| Externe Anschlüsse | 12 |
| Kontakte extern | 38 |
| ESP32-/ADS1115-Kontakte | 40 |
| Bedrahtete Widerstands-Pads | 22 (11 × 2) |
| Pads gesamt | 100 |
| Leiterbahnsegmente | 1213 |
| Durchkontaktierungen / Vias | 64 |
| Netze mit mehr als einem Pad | 22 |
| Unterbrochene Mehrfachnetze | **0** |
| Statisch erkannte Kurzschluss-/Abstandsverstöße unter 0,2 mm | **0** |

Die Prüfung basiert auf dem Auslesen der tatsächlichen
KiCad-S-Expression-Pad-, Segment- und Via-Daten und dem
geometrischen Abstandstest der unterschiedlichen Netze.
Zusätzlich wurden alle 22 Mehrfachnetze über
Pad-/Track-/Via-Verbindungsgruppen auf Durchgängigkeit geprüft.
**Keine KiCad-Projektdatei, kein KiCad-DRC/ERC und kein Gerber-Export
wurden ausgeführt.**

**Offene Freigabepunkte:**
- Endgültige Modul-/Phoenix-Footprints und Steckhöhen abgleichen.
- Befestigungslochbild, Gehäuse und Außenkontur freigeben.
- KiCad-DRC ausführen; auftretende Fehler beheben.
- Erst danach Gerber/Excellon exportieren und visuell kontrollieren.
- Bestehende Sensorsignale und Versorgung an aufgebauter PCB prüfen.

Ausgeführt wird der KiCad-DRC später mit dem beigefügten
`check_and_export_sensor.sh`-Prüfskript in einer KiCad-Umgebung.
**Diese Datei ist kein Fertigungsfreigabeprotokoll.**
