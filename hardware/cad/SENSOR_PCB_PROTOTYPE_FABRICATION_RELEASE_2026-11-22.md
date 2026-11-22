# Sensorplatine Rev A — Fertigungsdatensatz für UNBESTÜCKTE Musterplatine

**Stand: 22.11.2026.**
**Freigabestatus: CAD-geprüfte Herstellung einer UNBESTÜCKTEN Leiterplatten-Musterplatine.**
Dies ist ausdrücklich **keine** Freigabe einer elektrisch geprüften, bestückten und einsatzfähigen Steuerung. Es ist keine Zulassung oder Konformitätsbescheinigung.

## Verbindlicher PCB-Stand

- Grow-Controller-Repository, **main**, Boardquellstand nach USB-C-Stecker-Relayout und Pin-Silkscreen: `5269072b60df3a5b5d99eb9d411287cb5f0c70e7` (nach Merge auf `main`).
- KiCad: `hardware/cad/sensor_pcb_routed_candidate.kicad_pcb`.
- PCB: **179 × 115 mm**, **1,6 mm**, **2 Kupferlagen** (F.Cu/B.Cu).
- Mechanik: vier **Ø 3,2 mm NPTH**-M3-Aufnahmen (H1–H4).
- 27 elektrische Footprints, 4 mechanische Positionen; 12 externe Phoenix-Steckergruppen; steckbarer ESP32-Sensor-Controller und ADS1115.
- Bedrahtete Widerstände: 13 Footprintpositionen; pH: **PO — 10 kΩ — A0**, **A0 — 20 kΩ — GND**; EC/TDS: Signal direkt auf ADS1115 A1, entsprechend der zuletzt dokumentierten Steckbrettverbindung.
- Bauteile und Gegenstecker: `hardware/cad/SENSOR_PCB_BOM_REV_A.md`.
- USB-C-Freiraum korrigiert: `J_BH1750` bei (114,80) und `J_MLX` bei (146,80) mm; beide Steckverbinder sind aus dem USB-seitigen ESP32-Korridor verlegt. Alle acht 3V3-/GND-/I²C-Pins sind neu geroutet und neu beschriftet. Der 2D-Courtyard-Bereich x=53,20–82,20 mm und y=102,50–120 mm bleibt frei von externen Steckverbindern.

## Tatsächlich ausgeführte CAD-Fertigungsprüfung

**GitHub Actions auf dem vorhandenen nurovelle-vps-Runner, 22.11.2026; vollständig erfolgreicher Durchlauf für das USB-C-korrigierte Layout:**

https://github.com/d4sd1ng/homepage/actions/runs/45062015261

Erfolgreich:
1. Statische Footprint-, Pad- und Netzkontrolle.
2. **KiCad 9.0.2 DRC: 0 Violations, 0 unconnected items, 0 Footprint-Fehler.**
3. Export von **7 Gerber-Filmen**, separat PTH und NPTH im Excellon-Format.
4. CAM-ZIP-Paketprüfung: genau **9 Fertigungsdateien**, nicht leer; NPTH umfasst vier Bohrungen.
5. Archiv wurde in einem **separaten GitHub-Branch** gespeichert; der reguläre Actions-Artifact-Speicher war voll.

**Download, nur dieser neue Gerberstand:**

https://github.com/d4sd1ng/homepage/raw/refs/heads/test/grow-sensor-ph-divider-drc-20261122/sensor-pcb-RevA-USB-C-corrected-20261122.zip

Das ZIP umfasst die sieben KiCad-Lagenfilme, die beiden Bohrdateien, `PROTOTYPE_ONLY.txt` und eine SHA256-Dateiliste.

**Alle früheren Archive ohne das USB-C-Relayout sind überholt. Nur die oben verlinkte `USB-C-corrected`-Version verwenden.**

## Übergabe an einen Leiterplattenhersteller

Für eine **unbestückte Musterleiterplatte** nur den verlinkten neuen Gerber-/Drill-Datensatz verwenden. Außenkontur, Bohrungen, Lagen und Dicke sind im CAD festgelegt. Standardparameter, die im CAD nicht festgelegt sind (z. B. Lötstopplackfarbe, Oberflächenfinish oder Menge), müssen vor dem Auftrag separat gewählt werden; sie sind **nicht** von diesem Dokument freigegeben.

## Keine elektrische Betriebs- oder Montagefreigabe

Die folgenden Prüfungen können mit KiCad-DRC und GitHub-Logs nicht nachgewiesen werden:

- **USB-C bei der späteren Bestückung:** Der konkrete Layoutkonflikt wurde durch Versetzen von J_BH1750 und J_MLX behoben. Der USB-Austrittskorridor x=53,20–82,20 mm bis zur Platinenkante bei y=120 mm (17,50 mm ab ESP32-Courtyard) ist im CAD frei. **Die USB-C-Frage blockiert die Fertigung der unbestückten Leiterplatte nicht.** Die tatsächliche Kabelsteckprobe ist eine Montageprüfung am fertigen Prototyp und keine Voraussetzung für die Leerplatinenbestellung.
- **ADS1115 mit 3,3 V:** pH/EC-Spannungen am tatsächlich aufgebauten Messpfad bei den realen Betriebsbedingungen; EC A1 ist direkt angeschlossen. Der 10k/20k-pH-Teiler ist kein allgemeiner Überspannungsschutz.
- **Versorgung und Steckmodule:** tatsächliche USB-5-V/3,3-V-Belastung, Modulorientierung, Pull-ups und elektrischer Funktionstest mit realen Bauteilen.
- Firmware-/Kalibrierungsabgleich und Live-Sensorvalidierung nach Bestückung.

**Fertigungsentscheidung:** Die **unbestückte Sensorplatine Rev A ist CAD-seitig zur Musterbestellung freigegeben**, einschließlich des korrigierten USB-C-Austrittsbereichs. Der bereitgestellte Gerber-/Excellon-Datensatz ist der aktuelle Bestellstand. Die Funktionsprüfung mit tatsächlich bestückten Modulen (insbesondere Versorgung und Analogspannung) ist ein **eigener, nachfolgender Prüfschritt** und kein Hindernis für die Herstellung dieser Leerplatine. Eine Serien- oder Betriebsfreigabe ist damit nicht behauptet.
