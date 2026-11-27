# C3-Trägerplatine – Pumpen EIN/AUS (Hardware-Scope)
Stand 27.11.2026

## Verbindliche Klarstellung
Diese Platinenversion wird als **Niederspannungs-Aktorplatine mit sechs unabhängigen EIN/AUS-Pumpenausgängen** betrachtet. Keine pH-Regelung, keine automatische Dosierberechnung, keine Mengen- oder Zeitautomatik auf der Leiterplatte. Diese Funktionen gehören nicht zur Hardware-Bestellfreigabe.

## Vorgaben aus bestehendem Projekt
- 6 reservierte Pumpenkanäle mit 12 V DC, laut bisherigen Angaben je 3 W Nennleistung.
- Elektrische Ausführung: sechs eigene Niederspannungs-Schaltstufen **auf** der C3-Trägerplatine, nicht sechs externe Treiberboards.
- Ansteuerung durch vorhandenen PCA9685: OUT0 … OUT5; die Ausgangsbausteine treiben keine Motoren direkt.
- Jede Pumpenklemme zweipolig, steckbare Phoenix-MSTB-Serie mit 5,08 mm Raster.
- Vorhandener C3-Logikteil und 12-V-Einspeisung; die Versorgung des SuperMini erfolgt über 12→5-V-Step-down.
- Eine physische Motor-Rückstrom-/Überspannungsschutzstufe pro Kanal erforderlich.
- Gate-Pulldowns müssen Ausgänge bei Reset, unversorgtem PCA9685 und hochohmigem OE sicher AUS halten.
- Vorhandene Firmware sperrt die Pumpenansteuerung bis zur gesonderten Inbetriebnahme. Dies ist keine Leiterplatten-Funktion.

## Stücklisten-Ergänzung (zu präzisieren)
| Gruppe | Menge | Ausführung | Status |
|---|---:|---|---|
| Q_M1–Q_M6 | 6 | N-Kanal-Logic-Level-MOSFET, direkt mit 3,3-V-Steuersignal betreibbar; AO3400A/SOT-23 als Kandidat | Bauteil-Datenblatt vorhanden; Kauf/Footprint noch nicht endgültig freigegeben |
| R_GATE1–6 | 6 | Gate-Serienwiderstand | Endwert vor Freigabe festzulegen |
| R_PD1–6 | 6 | Gate-Pulldown für definiertes AUS | Endwert vor Freigabe festzulegen |
| D_M1–D_M6 | 6 | Induktive Rückstrombegrenzung/Freilaufbauteil | Typ nach realem Motor-Start-/Blockierstrom |
| J_MOTOR1–6 | 6 | zweipolige steckbare Anschlussklemme | Bereits im 13-Steckplatz-Kontingent enthalten; nicht doppelt zählen |
| Versorgungsschutz | nach Auslegung | Absicherung / Verpolschutz / Störunterdrückung im 12-V-Zweig | Auslegung offen |

**Kandidatenquelle:** Alpha & Omega Semiconductor AO3400A, 30-V-MOSFET mit RDS(on)-Angabe auch bei VGS=2,5 V: https://www.aosmd.com/sites/default/files/res/datasheets/AO3400A.pdf

## Fertigungsgrenze
Die Schnittstellen für fünf vorhandene SSR-Eingänge und einen gemeinsamen Lüftertreiber bleiben getrennte Aufgaben. Auf dieser Platine selbst werden keine Netzspannungsleitungen geführt. Der derzeitige KiCad-Logikentwurf hat **noch keine** sechs bestückten und gerouteten Motor-Leistungsstufen; die Bestellfreigabe ist deshalb offen.

Für die konkrete Dimensionierung fehlen weiterhin die realen Anlauf-/Blockierströme der Pumpen, der konkrete MOSFET-Aufbau, Schutz- und Leitungsauslegung sowie die vollständige DRC-/ERC-Prüfung. Die bisher bekannte Angabe 3 W pro Motor beschreibt allein Nennleistung.
