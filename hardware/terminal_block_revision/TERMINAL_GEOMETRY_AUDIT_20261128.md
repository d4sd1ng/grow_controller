# Schraubklemmen-Revision: statische mechanische Prüfung

Ausgeführt am 28.11.2026 direkt gegen die gespeicherten PCB-Dateien; beide Quell-Blob-SHAs unten dokumentiert.

Herstellerreferenz: https://www.sameskydevices.com/product/resource/tb004-508.pdf (09/12/2024, S. 2). Nennraster 5,08 mm, Bohrung 1,60 mm, Gehäuse-Nennlänge 60,96 / 10,16 mm und Tiefe 10,20 mm. Seitliche Rastnase/Toleranzen, reale Bauteilpassung und KiCad-Bibliotheksgleichheit sind nicht abschließend geprüft. PDF-Screenshot konnte nicht abgerufen werden.

Geprüft: Anzahl und Polzahl der Klemmleisten, Pad-Raster und Bohrung jedes einzelnen Pads, Nennkontur, vorhandene PCB-Außenkontur, Abwesenheit alter Phoenix-Footprints, SMD-Inventur und Vorhandensein von Leiterbahnen/Vias. Keine elektrische Funktionsprüfung und kein KiCad-DRC. Die ursprünglichen Fertigungsdaten werden nicht überschrieben. Beide Arbeitsdateien bleiben **nicht bestellfähig**.

```json
[
  {
    "board": "sensor",
    "sourceBlob": "56f5fc235e36d4111285c1575622c492e438e154",
    "countsPass": true,
    "sizePass": true,
    "noPhoenix": true,
    "dimensions": [
      {
        "ref": "K1",
        "poles": 12,
        "footprintGeometryMatches": true
      },
      {
        "ref": "K2",
        "poles": 12,
        "footprintGeometryMatches": true
      },
      {
        "ref": "K3",
        "poles": 12,
        "footprintGeometryMatches": true
      },
      {
        "ref": "K4",
        "poles": 2,
        "footprintGeometryMatches": true
      }
    ],
    "manualSmdPositions": 0,
    "traces": 0,
    "vias": 0,
    "mechanicalNominalCheck": true,
    "release": "BLOCKED"
  },
  {
    "board": "c3",
    "sourceBlob": "2fd52efa8730aa6b9f505feb242de346fec7fe4a",
    "countsPass": true,
    "sizePass": true,
    "noPhoenix": true,
    "dimensions": [
      {
        "ref": "K1",
        "poles": 12,
        "footprintGeometryMatches": true
      },
      {
        "ref": "K2",
        "poles": 12,
        "footprintGeometryMatches": true
      },
      {
        "ref": "K3",
        "poles": 2,
        "footprintGeometryMatches": true
      }
    ],
    "manualSmdPositions": 33,
    "traces": 0,
    "vias": 0,
    "mechanicalNominalCheck": true,
    "release": "BLOCKED"
  }
]
```
