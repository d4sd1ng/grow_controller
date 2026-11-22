# Sensor PCB Rev A — USB-C-Zugang nach Stecker-Relayout

**Aktueller PCB-Konstruktionsstand: 22.11.2026.**
Die ursprüngliche Steckerblockade im USB-C-Ausgangsbereich des ESP32 wurde
**durch Verlegen der beiden vierpoligen Phoenix-Anschlüsse J_BH1750 und J_MLX**
beseitigt. Das ursprüngliche 179 × 115 mm Platinenmaß bleibt erhalten.
Der ESP32 wird nicht gedreht; alle Pin-/Netzzuordnungen bleiben gleich.

## Lage im konkreten KiCad-Board (Koordinaten mm)

| Bauteil | Pad-1 (x, y) | Courtyard x | Courtyard y | Status |
| --- | --- | --- | --- | --- |
| U_ESP32 | (55, 54) | 53,20–82,20 | 50,50–102,50 | unverändert; USB-C an Y+ |
| J_BH1750 | **(114, 80)** | 109,96–133,28 | 69,50–82,50 | vom unteren Rand verlegt |
| J_MLX (Kamera) | **(146, 80)** | 141,96–165,28 | 69,50–82,50 | vom unteren Rand verlegt |
| J_PH | (13, 114) | 8,96–32,28 | 103,50–116,50 | unverändert |
| J_RGB | (100, 114) | 95,96–119,28 | 103,50–116,50 | unverändert |

Der gesamte nach unten projizierte ESP32-Modulkorridor
**x=53,20–82,20 mm; y=102,50–120 mm** ist jetzt frei von externen
Phoenix-Footprint-Courtyards. In der ursprünglichen Version lagen sowohl
J_BH1750 als auch J_MLX in diesem Korridor. Die beiden versetzten
Anschlüsse behalten ihre **nach Norden (−Y)** gerichtete Steckrichtung,
also weg von der USB-Seite des ESP32.

Die acht Versorgungs-/I²C-Anschlüsse wurden neu geroutet
(3V3/GND und je zwei Signalleitungen pro Stecker). Die acht
Silkscreen-Pinbeschriftungen wurden mit versetzt.
Ein automatischer CAD-Audit prüft, dass der USB-Korridor künftig nicht
wieder von einem externen Steckverbinder überdeckt wird.

## Ergebnis und Freigabegrenze

**Der ursprüngliche USB-C-Steckerkonflikt im Leiterplattenlayout ist behoben.**
Die reservierte USB-Austrittsfläche beträgt geometrisch 29,00 mm in
X-Richtung (53,20–82,20 mm) und **17,50 mm bis zur Platinenkante**
in Y-Richtung (102,50–120,00 mm). Im CAD ragt kein externer
Phoenix-Steckverbinder mehr in diese Fläche.

**Für die Bestellung der unbestückten Leiterplatte ist die USB-C-Frage
damit abgeschlossen.** Ein USB-C-Stecker muss nicht vollständig *innerhalb*
der Platinenfläche liegen; der Korridor führt offen bis zur Außenkante.
Die konkreten Abmessungen eines vorhandenen Kabelsteckers sind für diese
CAD-Leiterplattenfreigabe nicht mehr als Blocker zu behandeln.

**Grenze der Aussage:** Eine körperliche Steckprobe bei vollständig
bestückter Platine wurde nicht durchgeführt; sie gehört zur späteren
Montage-/Funktionsprüfung, nicht zur CAD-Fertigung der Leerplatine.

Die ehemals erwogene **18,67 mm erhöhte E-tec-Buchsenleiste** war
ein möglicher Workaround für die alte Blockade. Durch das Relayout
ist sie **nicht länger als Maßnahme zur Beseitigung des 2D-Konflikts
erforderlich**. Die tatsächlich vorhandenen Buchsenleisten können erst
am Aufbau mechanisch geprüft werden.

## Fertigungsprüfung und Versionsbindung

- KiCad-Board: `hardware/cad/sensor_pcb_routed_candidate.kicad_pcb` auf `main`,
  integriert über [PR #17](https://github.com/d4sd1ng/grow_controller/pull/17).
- 27 elektrische Footprints, vier M3-NPTH, zwei Kupferlagen.
- **KiCad 9.0.2: finaler DRC bestanden, 0 Verstöße, 0 offene Pads,
  0 Footprint-Fehler.**
- Abschlussprüfung und CAM-Ausgabe:
  https://github.com/d4sd1ng/homepage/actions/runs/45062015261
- Bestellfähiger Leerplatinen-Datensatz:
  `sensor-pcb-RevA-USB-C-corrected-20261122.zip`, im
  Fertigungsfreigabevermerk verlinkt.
- Früherer Gerber-Datensatz vor diesem Relayout darf **nicht** als
  aktuelle Musterplatine bestellt werden.

Die USB-Korridorfreigabe und eine elektrische Freigabe des bestückten
Controllers sind unterschiedliche Prüfgegenstände.
