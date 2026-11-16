# Grow Controller — verbindliche Konstruktionsparameter, 16.11.2026

**Geltungsbereich:** Sensorträger mit 3,3/5-V-Sensorik. Kein Schaltplan für Netzspannung, keine Freigabe für chemische Dosierpumpen. **Nicht als Fertigungsfreigabe verwenden.**

## 1. Aus dem bestehenden Board bereits eindeutig festgelegt

| Punkt | Konstruktionswert | Nachweis |
|---|---|---|
| Sensor-PCB-Außenkontur | 179 × 115 mm | KiCad Edge.Cuts von (5,5) bis (184,120) |
| Platinendicke | 1,6 mm | KiCad-(general thickness 1.6) |
| Kupferlagen | zwei, F.Cu/B.Cu | PCB-Datei |
| Sensor-ESP32-Sockel | 2 × 15, Raster 2,54 mm, Abstand der Reihenmitten 25,40 mm | Maßbild des Nutzers vom 15.11.2026; als verbindliche Konstruktionsvorgabe übernommen |
| ADS1115-Sockel | 1 × 10, Raster 2,54 mm; erster bis letzter Kontakt 22,86 mm | Nutzerfestlegung vom 15.11.2026 |
| Externe Sensor-/Bedienverbinder | 2 × 2-polig, 6 × 3-polig, 4 × 4-polig, Phoenix MSTBA 2,5, Raster 5,08 mm | dokumentierte Produktauswahl |
| Platinenseitige Klemmen, 2-polig | Phoenix Contact **1757242**, MSTBA 2,5/2-G-5,08 | Herstellerkatalog |
| Platinenseitige Klemmen, 3-polig | Phoenix Contact **1757255**, MSTBA 2,5/3-G-5,08 | Herstellerkatalog |
| Platinenseitige Klemmen, 4-polig | Phoenix Contact **1757268**, MSTBA 2,5/4-G-5,08 | Herstellerkatalog |
| Gegenstecker | 2p **1757019**, 3p **1757022**, 4p **1757035** | dokumentierte Auswahl |
| Sensor-ESP32-Außenkontur | 28 × 51 mm | vom Nutzer geliefertes Maßbild, als PCB-Vorgabe übernommen |
| C3-Super-Mini-Modul (separate Platine) | 18 × 22,52 mm, 2 × 8 Kontakte, Raster 2,54 mm, Reihenmitten 15,24 mm | im Projekt festgelegtes Maßbild |
| Versorgung Sensor-ESP32 | 5 V über USB-C | Projektvorgabe |
| ADS1115 | 3,3 V, A0=pH, A1=EC/TDS, SDA GPIO21, SCL GPIO22 | Projektvorgabe |
| Externe Status-LED | genau eine RGB-LED, gemeinsame Kathode | Projektvorgabe |
| Vorwiderstandspositionen | 11 bedrahtete, 4 DHT/DS-Pull-ups, 3 RGB, 4 optionale I2C | Projektvorgabe |

## 2. Phoenix-Grundleisten, verifizierte Herstellergeometrie

| Pole | Artikel | Körperbreite | Höhe laut aktuellem Katalog | Länge | Raster | PCB-Bohrung |
|---|---|---:|---:|---:|---:|---:|
| 2 | 1757242 | 12,16 mm | 11,8 mm | 12 mm | 5,08 mm | 1,4 mm |
| 3 | 1757255 | 17,24 mm | 11,8 mm | 12 mm | 5,08 mm | 1,4 mm |
| 4 | 1757268 | 22,32 mm laut PCB-Dokumentation; Herstellerkatalog vor endgültigem Vergleich nachprüfen | 11,8 mm der Baureihe | 12 mm | 5,08 mm | 1,4 mm |

Herstellerquelle: https://www.phoenixcontact.com/en-us/products/pcb-header-mstba-25-2-g-508-1757242

**Wichtig:** Stecker/Leitung müssen *parallel* zur Leiterplatte ausgesteckt werden können. Die vorhandenen Platzhalter zeichnen nicht den tatsächlich nötigen Aussteck- und Kabeleinführraum. KiCad führt passende offizielle Footprints:
- `Connector_Phoenix_MSTB:PhoenixContact_MSTBA_2,5_2-G-5,08_1x02_P5.08mm_Horizontal`
- `Connector_Phoenix_MSTB:PhoenixContact_MSTBA_2,5_3-G-5,08_1x03_P5.08mm_Horizontal`
- `Connector_Phoenix_MSTB:PhoenixContact_MSTBA_2,5_4-G-5,08_1x04_P5.08mm_Horizontal`

Quelle für Footprints: https://kicad.github.io/footprints/Connector_Phoenix_MSTB.html

## 3. Nachweisbares DRC-Ergebnis — keine kosmetische Umbenennung

KiCad 9.0.2 meldet nach Einbindung der sechs lokalen Footprint-Bauformen **0 DRC-Meldungen, 0 unverbundene Pads und 0 Footprint-Fehler** für alle 25 Positionen. Quelle: https://github.com/d4sd1ng/homepage/actions/runs/43962759317

Die Bibliotheksnamen sind auf `Grow_Controller_Sensor` umgestellt; Pads, bestehende Netze, Hersteller-Fab-Konturen und Courtyards sind in sechs projektlokalen Footprints abgebildet. Die komplette Zuordnung steht in `FOOTPRINT_ASSIGNMENTS_25.md`. Steckrichtung/USB-Zugang sind an der offenen Platine zu prüfen. Ein Sensor-Gehäuse ist noch nicht vorhanden und erst nach der Platine vorgesehen.

## 4. Aktuelle Arbeitspunkte — bereits festgelegte Modulmaße NICHT erneut abfragen

Das bemaßte ESP32-Maßbild und die dokumentierten Kontaktgeometrien gelten für diesen Prototyp als **freigegebene Design-Eingabewerte**. Erneutes Erfragen oder Zurückstufen dieser Angaben zu einer bloßen Schätzung ist kein zulässiger Arbeitsblocker.

1. **Erledigt:** alle 25 Platzhalter durch sechs definierte Bibliotheks-Footprints ersetzt. Steckorientierung vor Montageabnahme prüfen.
2. ADS1115: belegte Kontaktfolge aus dem vorliegenden Modul und der bisherigen Pinout-Dokumentation in den Schaltplan übernehmen; Verpolung vermeiden.
3. Bestückungsaufnahme der vorhandenen bedrahteten Widerstände und Einbau-/Aussteckraum der Phoenix-Klemmen auf dem Board prüfen.
4. **PCB abgeschlossen:** 4 M3-NPTH-Bohrungen auf der Sensorplatine gesetzt. Das spätere Sensor-Gehäuse wird anhand des realen PCB-Prototyps entworfen; keine Passprobe als Fertigungsvoraussetzung.
5. Strombudget und pH/EC-Analogpegel unabhängig von der mechanischen Bemaßung kontrollieren.
6. **Erledigt:** KiCad-DRC nach Fußabdruckwechsel erfolgreich. Prüfdaten Gerber/Excellon separat vorhanden; Produktionsfreigabe weiterhin getrennt.

**Unbestückte Prototypplatine und Live-Betriebsfreigabe sind getrennt.** Ein noch nicht vorhandenes Sensor-Gehäuse blockiert die erste Leerplatine nicht; **die 25 Footprints sind zugeordnet und DRC-geprüft**. Die CAM-Vorschau unter PR #13 bleibt bis zur Prüfung gesperrt.
