# C3-12-V-Platine – Fertigungsunterlagen R1

**Stand: 27.11.2026**

## Fertig für die Bestellung einer UNBESTÜCKTEN Prototyp-Leiterplatte

Die aktuelle PCB-Quelle ist ausschließlich:
`hardware/cad_c3/c3_12v_5signals_1fan_WIP_NOT_FOR_ORDER.kicad_pcb`

Der vollständige, aus genau dieser Quelle generierte Fertigungsdatensatz steht als ZIP bereit:
https://github.com/d4sd1ng/homepage/blob/ci/c3-kicad-floorplan-20261125/fabrication/grow_controller_c3_12v_r1/Grow_Controller_C3_12V_R1_Gerber_Drill.zip

Zugehörige Teile- und Anschlussübersicht:
https://github.com/d4sd1ng/homepage/blob/ci/c3-kicad-floorplan-20261125/fabrication/grow_controller_c3_12v_r1/C3_12V_R1_BOM_AND_PINOUT.md

## Nachweise

- KiCad-9-DRC: **0 Regelverletzungen**
- KiCad-9: **0 nicht angeschlossene Modell-Pads**
- KiCad-9: **0 Footprint-Abweichungen**
- Zusätzlicher Test der PCB-Netzbelegung: bestanden
- Gerber-Lagen (Kupfer, Maske, Bestückungsdruck, Kontur), Excellon-Bohrdaten und Quelldatei aus dem gültigen Layout exportiert
- Archiv-Konsistenzprüfung `check_12v_prototype_fabrication.py`: ZIP ohne Fehler, alle notwendigen Fertigungslagen vorhanden, SHA-256 des Layouts identisch, DRC-Bericht fehlerfrei
- Prüflauf: https://github.com/d4sd1ng/homepage/actions/runs/46036663987

## Platine im Layout

- 160 × 116 mm, zwei Kupferlagen, 1,6 mm Platinenstärke
- J1 DC-Hohlbuchse als 12-V-Eingang
- Sechs separate Pumpenausgänge J7–J12 mit MOSFET-Schaltstufen
- Fünf Steueranschlüsse J2–J6 (GND + 3,3-V-GPIO-Signal)
- J13: 12 V, geschaltete Masse und ein absichtlich **nicht angeschlossener** Reservestift
- JP1: abziehbare manuelle Servicebrücke des 5-V-Versorgungspfads

## Freigabegrenze

**Fertigungsdaten für eine unbestückte Prototyp-Platine: vorhanden und automatisiert geprüft.**

Die **bestückte, betriebsfertige Steuerung** ist nicht elektrisch am realen Aufbau geprüft: konkrete Steckpassungen, 12-V-Eingangsschutz, reale Einschalt-/Blockierströme und Service-Strompfad bleiben vor Inbetriebnahme zu verifizieren. Die Vorlage ist kein Nachweis einer fertigen Serienbaugruppe.

Die Fertigungsdaten lassen sich ohne lokale KiCad-Installation an einen Platinenhersteller übergeben. Die Stückliste dient der Bestückungsplanung.
