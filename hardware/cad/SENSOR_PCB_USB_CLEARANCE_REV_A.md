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

## Grenzen der mechanischen Aussage

Der verifizierte **2D-Courtyard-Korridor** ist ein konkreter, anhand
der KiCad-Datei prüfbarer Befund. Er ersetzt keine dreidimensionale
Passprobe mit dem tatsächlich vorhandenen USB-C-Stecker, den
Phoenix-Gegensteckern und den Kabeln. Insbesondere sind exakte
Steckerhüllenmaße und Leitungsknickradien nicht in den Footprints modelliert.

Die ehemals erwogene **18,67 mm erhöhte E-tec-Buchsenleiste** war
ein möglicher Workaround für die alte Blockade. Durch das Relayout
ist sie **nicht länger als Maßnahme zur Beseitigung des 2D-Konflikts
erforderlich**. Die tatsächlich vorhandenen Buchsenleisten können erst
am Aufbau mechanisch geprüft werden.

## Fertigungsprüfung und Versionsbindung

- KiCad-Board: `hardware/cad/sensor_pcb_routed_candidate.kicad_pcb`
  auf `fix/sensor-usb-c-corridor-20261122` (bis zum Merge).
- 27 elektrische Footprints, vier M3-NPTH, zwei Kupferlagen.
- KiCad 9.0.2: nach dem ersten Routingcheck **0 unverbundene Pads**;
  zwei redundante GND-Durchkontaktierungen wurden danach mit den
  bereits vorhandenen Vias vereinigt.
- Der **abschließende** DRC einschließlich der versetzten
  Silkscreen-Beschriftung ist im zugehörigen CI-Prüfbericht nachzuweisen.
- Früherer Gerber-Datensatz vor diesem Relayout darf **nicht** als
  aktuelle Musterplatine bestellt werden.

Die USB-Korridorfreigabe und eine elektrische Freigabe des bestückten
Controllers sind unterschiedliche Prüfgegenstände.
