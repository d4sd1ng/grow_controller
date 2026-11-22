# Sensorplatine Rev A — Fertigungsdatensatz für UNBESTÜCKTE Musterplatine

**Stand: 22.11.2026.**
**Freigabestatus: CAD-geprüfte Herstellung einer UNBESTÜCKTEN Leiterplatten-Musterplatine.**
Dies ist ausdrücklich **keine** Freigabe einer elektrisch geprüften, bestückten und einsatzfähigen Steuerung. Es ist keine Zulassung oder Konformitätsbescheinigung.

## Verbindlicher PCB-Stand

- Grow-Controller-Repository, **main**, Boardquellstand nach Merge PR #16: `ec0552865d4c09ae05dbb2f5d3f57e23cfa6f8c5`.
- KiCad: `hardware/cad/sensor_pcb_routed_candidate.kicad_pcb`.
- PCB: **179 × 115 mm**, **1,6 mm**, **2 Kupferlagen** (F.Cu/B.Cu).
- Mechanik: vier **Ø 3,2 mm NPTH**-M3-Aufnahmen (H1–H4).
- 27 elektrische Footprints, 4 mechanische Positionen; 12 externe Phoenix-Steckergruppen; steckbarer ESP32-Sensor-Controller und ADS1115.
- Bedrahtete Widerstände: 13 Footprintpositionen; pH: **PO — 10 kΩ — A0**, **A0 — 20 kΩ — GND**; EC/TDS: Signal direkt auf ADS1115 A1, entsprechend der zuletzt dokumentierten Steckbrettverbindung.
- Bauteile und Gegenstecker: `hardware/cad/SENSOR_PCB_BOM_REV_A.md`.

## Tatsächlich ausgeführte CAD-Fertigungsprüfung

**GitHub Actions auf dem vorhandenen nurovelle-vps-Runner, 22.11.2026; letzter erfolgreicher Durchlauf nach Änderung des Silkscreens:**

https://github.com/d4sd1ng/homepage/actions/runs/45051292786

Erfolgreich:
1. Statische Footprint-, Pad- und Netzkontrolle.
2. **KiCad 9.0.2 DRC: 0 Violations, 0 unconnected items, 0 Footprint-Fehler.**
3. Export von **7 Gerber-Filmen**, separat PTH und NPTH im Excellon-Format.
4. CAM-ZIP-Paketprüfung: genau **9 Fertigungsdateien**, nicht leer; NPTH umfasst vier Bohrungen.
5. Archiv wurde in einem **separaten GitHub-Branch** gespeichert; der reguläre Actions-Artifact-Speicher war voll.

**Download, nur dieser neue Gerberstand:**

https://github.com/d4sd1ng/homepage/raw/refs/heads/test/grow-sensor-ph-divider-drc-20261122/sensor-pcb-RevA-20261122-BarePrototype.zip

Das ZIP umfasst die sieben KiCad-Lagenfilme, die beiden Bohrdateien, `PROTOTYPE_ONLY.txt` und eine SHA256-Dateiliste.

**Alte Archive aus PR #13 sind durch die neue pH-Eingangsschaltung überholt. Nicht verwenden.**

## Übergabe an einen Leiterplattenhersteller

Für eine **unbestückte Musterleiterplatte** nur den verlinkten neuen Gerber-/Drill-Datensatz verwenden. Außenkontur, Bohrungen, Lagen und Dicke sind im CAD festgelegt. Standardparameter, die im CAD nicht festgelegt sind (z. B. Lötstopplackfarbe, Oberflächenfinish oder Menge), müssen vor dem Auftrag separat gewählt werden; sie sind **nicht** von diesem Dokument freigegeben.

## Keine elektrische Betriebs- oder Montagefreigabe

Die folgenden Prüfungen können mit KiCad-DRC und GitHub-Logs nicht nachgewiesen werden:

- **Passung und USB-C-Steckfreiheit:** ESP32-Modul mit allen Phoenix-Gegensteckern und Kabel; die dokumentierte erhöhte 1×15-Buchsenleiste (18,67 mm) ist noch nicht als konkrete Stücknummer/physischer Aufbau verifiziert.
- **ADS1115 mit 3,3 V:** pH/EC-Spannungen am tatsächlich aufgebauten Messpfad bei den realen Betriebsbedingungen; EC A1 ist direkt angeschlossen. Der 10k/20k-pH-Teiler ist kein allgemeiner Überspannungsschutz.
- **Versorgung und Steckmodule:** tatsächliche USB-5-V/3,3-V-Belastung, Modulorientierung, Pull-ups und elektrischer Funktionstest mit realen Bauteilen.
- Firmware-/Kalibrierungsabgleich und Live-Sensorvalidierung nach Bestückung.

Daher gilt: **Fertigung der unbestückten Musterplatine CAD-seitig freigegeben.** **Keine bestätigte Steckbarkeit oder elektrische Funktion** und keine Serienfreigabe. Eine nachträgliche Layoutkorrektur nach dem Muster ist möglich und würde neue CAM-Dateien erfordern.
