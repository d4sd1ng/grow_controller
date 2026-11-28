# Schraubklemmen-Revision – beide Leiterplatten (28.11.2026)

Status: verbindliche mechanische Zielvorgabe des Nutzers; **Layout, Routing und Fertigungsprüfung stehen noch aus.** Nicht zur Fertigung.

| Platine | 12-polig | 2-polig | Kapazität | Bisherige Kontakte |
|---|---:|---:|---:|---:|
| Sensor Rev A | 3 | 1 | 38 | 38 |
| C3 R1 | 2 | 1 | 26 | 25 |
| Gesamt | 5 | 2 | 64 | 63 |

Die vorhandenen 24 steckbaren MSTBA-Grundleisten und 24 Gegenstecker sollen vollständig entfallen. Fest verlötete THT-Schraubklemmen, einreihig, horizontale Kabeleinführung, Raster 5,08 mm. Keine Änderung der bestehenden Anschlussfunktionen. Ein freier Kontakt auf der C3-Platine ist als Reserve zu kennzeichnen.

## Beschaffungskandidat (noch keine Bauteilfreigabe)
Same Sky TB004-508-12BE und TB004-508-02BE. Herstellerzeichnung: https://www.sameskydevices.com/product/resource/tb004-508.pdf (S. 2). Gehäusenennmaße 60,96 bzw. 10,16 x 10,20 mm, Höhe 14,10 mm; Lochreihe Ø1,60 mm, Raster 5,08 mm, Abstand erster/letzter Kontakt 55,88 bzw. 5,08 mm. Seitliche Verbindungsnase zusätzlich berücksichtigen.

Vorhandene CAD-/CAM-Dateien bleiben als historische Revision unverändert; sie enthalten noch die alte Phoenix-Variante. Keine neue Gerber-Freigabe ohne neue CAD-Prüfung. Die THT-Überarbeitung der übrigen C3-Bauteile bleibt eine getrennte Aufgabe. Keine Änderungen an Firmware oder elektrischer Funktion.

Die Bibliothek `TerminalBlock_Candidate.pretty` enthält eigenständig gezeichnete, ungeprüfte mechanische Footprint-Kandidaten für diese beiden Polzahlen. Pad-Außendurchmesser und Courtyard sind vorläufige CAD-Parameter und keine Herstellerfreigabe. Vor Verwendung mit Original-Footprint und Muster abgleichen.
