# Grow Controller — verbindliche Konstruktionsparameter, 16.11.2026

**Geltungsbereich:** Sensorträger mit 3,3/5-V-Sensorik. Kein Schaltplan für Netzspannung, keine Freigabe für chemische Dosierpumpen. **Nicht als Fertigungsfreigabe verwenden.**

## 1. Aus dem bestehenden Board bereits eindeutig festgelegt

| Punkt | Konstruktionswert | Nachweis |
|---|---|---|
| Sensor-PCB-Außenkontur | 179 × 115 mm | KiCad Edge.Cuts von (5,5) bis (184,120) |
| Platinendicke | 1,6 mm | KiCad-(general thickness 1.6) |
| Kupferlagen | zwei, F.Cu/B.Cu | PCB-Datei |
| Sensor-ESP32-Sockel | 2 × 15, Raster längs 2,54 mm, **vorläufige Reihenmitte 25,40 mm** | vorhandener Entwurf, noch nicht am Modul vermessen |
| ADS1115-Sockel | 1 × 10, Raster 2,54 mm | vorhandener Entwurf / Nutzerangabe |
| Externe Sensor-/Bedienverbinder | 2 × 2-polig, 6 × 3-polig, 4 × 4-polig, Phoenix MSTBA 2,5, Raster 5,08 mm | dokumentierte Produktauswahl |
| Platinenseitige Klemmen, 2-polig | Phoenix Contact **1757242**, MSTBA 2,5/2-G-5,08 | Herstellerkatalog |
| Platinenseitige Klemmen, 3-polig | Phoenix Contact **1757255**, MSTBA 2,5/3-G-5,08 | Herstellerkatalog |
| Platinenseitige Klemmen, 4-polig | Phoenix Contact **1757268**, MSTBA 2,5/4-G-5,08 | Herstellerkatalog |
| Gegenstecker | 2p **1757019**, 3p **1757022**, 4p **1757035** | dokumentierte Auswahl |
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

KiCad 9.0.2 meldete 25 Warnungen, alle `lib_footprint_issues`, nachdem elektrische Routing-Warnungen bereinigt waren. Keine unverbundenen Pads. Quelle: https://github.com/d4sd1ng/homepage/actions/runs/43796471420

Die Bibliotheksnamen werden **nicht** blind geändert: Erst müssen die offiziellen Footprints an Pads, Orientierung, Körperkontur, 3D-Modell und Steckraum ausgerichtet und erneut geroutet/geprüft werden.

## 4. Offen — nicht durch Schätzwerte ersetzen

1. Exaktes Maß zwischen **realen** ESP32-Pinreihen, Modul-USB-C-Lage, Breite und Höhe; vorläufig 25,40 mm ist nicht gemessen.
2. ADS1115-Pinreihenfolge, realer Header-Versatz zur Platinenkante und Modulaußenmaß.
3. THT-Widerstandsabmessungen/Bohrungen für die bereits vorhandenen Teile.
4. 3D/Einbauraum aller Phoenix-Gegenstecker, Steckerorientierung am Gehäuse, Abgangszugentlastung.
5. Befestigungslochbild und Einbaufläche im konkreten Gehäuse, inklusive Steckerfreiraum.
6. Ausreichende 3,3-V-Reglerreserve und 5-V-Versorgung am tatsächlich gelieferten ESP32.
7. Eingangsspannungen pH/EC gegenüber ADS1115-Grenzwerten für alle Betriebszustände.

**Ohne diese realen Maße und Versorgungstests ist eine fertig bestückbare Platine technisch nicht nachgewiesen.** Die CAD-Vorschau- und CAM-Dateien unter PR #13 sind ausdrücklich nicht zur Bestellung bestimmt.
