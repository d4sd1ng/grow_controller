# Sensor PCB Rev A — USB-C-Zugang und gesteckte Phoenix-Klemmen

**Stand 17.11.2026:** CAD-Zeichnung mit 179×115 mm PCB und 4 M3-Bohrungen DRC-geprüft. Für die USB-Erreichbarkeit ist die **Z-Richtung** zusätzlich zur 2D-Geometrie maßgebend.

## Bestehende, festgelegte Hardware nicht ändern
- Sensor-ESP32 30 Pins, steckbar 2×15 mit 2,54-mm-Raster.
- Phoenix Contact MSTBA 2,5 mit 5,08-mm-Pitch, platinenseitige Grundleisten und abziehbare Gegenstecker, Steckrichtung parallel zum PCB.
- U_ESP32-Courtyard: x=53,20–82,20, y=50,50–102,50 mm.
- J_BH1750 bzw. J_MLX liegen jeweils nur 1 mm von der USB-seitigen ESP32-Courtyard-Kante entfernt. **Der flache Steckaufbau ist mechanisch nicht freigegeben.**

## Mechanisch realisierbare Alternative ohne Änderung der Leiterbahnen

Den steckbaren ESP32 über **zwei erhöhte, einreihige, bedrahtete 1×15-Buchsenleisten** führen. Die Footprint-Pads bleiben identisch; kein Rerouting nötig.

- Raster 2,54 mm, 15 Kontakte pro Leiste, 0,64-mm-Quadratstifte sind mit der recherchierten E-tec-Serie kompatibel.
- E-tec **BE1 Elevated Female Headers**, Style **S181**, nominale Elevation **18,67 mm**, sind im Herstellerprogramm dokumentiert. Die konkrete 15-polige Ausführung, Lieferbarkeit und Steckstiftlänge sind vor Bestellung über das genaue Hersteller-Datenblatt zu validieren. **Kein unbestätigter Artikelcode in die BOM.**
- Phoenix Contact Gegenstecker **MSTB 2,5/4-ST-5,08 / 1757035**: Körperhöhe 15 mm, Breite 20,32 mm, Länge 18,3 mm laut Hersteller.
- Theoretische Höhendifferenz zwischen erhöhtem Kontaktträger (18,67 mm) und einzelnem Phoenix-Steckerkörper (15 mm) beträgt **3,67 mm**. Das ist **keine gesicherte USB-Steckerfreiheit**: Lage der USB-Buchse am ESP32-Modul, Male-USB-C-Steckgehäuse, Drahtknickradius sowie das zusammengesetzte Phoenix-Steckpaar sind in dieser Differenz nicht enthalten.

**Die Variante ist ein mechanischer Lösungsansatz, keine freigegebene Bestückung.** Sie muss in einer realen Zusammenbauprobe mit allen gesteckten Phoenix-Steckern und dem vorgesehenen USB-C-Kabel oder anhand vollständig verifizierter 3D-Modelle auf Steckbarkeit, Belastung des ESP32-Steckmoduls und ausreichende Z-Höhe über der noch offenen Sensorplatine geprüft werden. Ein Sensor-Gehäuse existiert derzeit nicht.

## Berechnung und Datenquellen

- E-tec-Produktserie BE1 erhöht, Style 181 = 18,67 mm, Pinraster 2,54 mm:
  https://www.e-tec.com/v5/products/pcb-interconnect-products/female-headers/elevated-types/pitch-2_54mm/index.php
- Phoenix Contact 1757035, Gegenstecker, h=15 mm, w=20,32 mm, l=18,3 mm:
  https://www.phoenixcontact.com/de-de/produkte/leiterplattenstecker-mstb-25-4-st-508-1757035
- Phoenix Contact 1757268, Grundleiste, Steckrichtung parallel zur Leiterplatte:
  https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-4-g-508-1757268

## Fertigungsstatus

KiCad 9.0.2 meldet 0 Leiterplatten-DRC-Fehler am Rev-A-Montageentwurf. **Noch offen:** USB-/Stecker-Zugänglichkeit an der offenen Platine sowie Niederspannungs-Funktionstest nach Erhalt des Prototyps. Die **Gehäuseplanung folgt danach**. Die erste unbestückte PCB-Prototypfertigung benötigt keine Gehäusepassprobe; sie ist von der Live-Betriebsfreigabe zu trennen.
