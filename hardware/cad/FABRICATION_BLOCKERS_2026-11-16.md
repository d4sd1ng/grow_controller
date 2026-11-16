> **KORREKTUR 17.11.2026:** Diese Datei enthält einen historischen Prüfstand. Die Sensorplatine hat **noch kein Gehäuse**; das einzige vorhandene Gehäuse gehört zur separaten Relaissteuerung. Eine Gehäuse-Passprobe darf **nicht** Voraussetzung der ersten unbestückten Prototypplatine sein. Aktueller PCB-Stand: 179×115×1,6 mm, 4 M3-NPTH, 25 elektrische Footprints, KiCad-DRC bestanden und PTH/NPTH-Daten vorhanden. Für einen Prototyp-Gerberexport gibt es `check_and_export_sensor.sh --export-prototype`; dieser erteilt **keine** elektrische Betriebs- oder Serienfreigabe.\n\n> **Fortgeschriebener Stand:** Die unten aufgeführten früheren DRC-/Bibliotheksfehler sind historisch. Am 16.11.2026 wurden alle 25 Footprints konkret zugeordnet und KiCad 9.0.2 lieferte **0 DRC-Meldungen / 0 unverbundene Pads / 0 Footprint-Fehler**. Nachweis: https://github.com/d4sd1ng/homepage/actions/runs/43962759317 und `hardware/cad/FOOTPRINT_ASSIGNMENTS_25.md`. Die mechanische/elektrische Freigabe bleibt offen.

# Sensor-PCB — Fertigungsprüfung, 16.11.2026

**Historischer Stand:** keine Freigabe für ein live betriebenes, bestücktes System. Der erste unbestückte PCB-Prototyp ist davon zu unterscheiden.

## Nachweisbare Dateiprüfung
- Arbeitslayout: `hardware/cad/sensor_pcb_routed_candidate.kicad_pcb`
- 25 Footprints, 12 externe Steckverbinder, 2 Steckmodule, 11 Widerstandspositionen.
- 1213 Kupfersegmente, 64 Vias.
- 42 deklarierte Netze; keine in der statischen Quellprüfung unbekannten oder widersprüchlich benannten Netzreferenzen.
- Doppelte GND-Deklarationen wurden entfernt.
- Das Skript `audit_sensor_pcb.py` kontrolliert Netzdeklarationen, Footprint-Referenzen und den Außenumriss.
- Die statische Prüfung **ersetzt weder ERC noch KiCad-DRC**.

## CI-Blocker (kein ausgeführter DRC nachgewiesen)
- GitHub Actions Run 43794609293: failure; laut Actions-API `runner_id=0`, `steps=[]`, Laufzeit ca. 2 Sekunden.
- GitHub Actions Run 43795064182: ebenfalls `runner_id=0`, `steps=[]`, Laufzeit ca. 2 Sekunden.
- Damit ist **kein einziger KiCad-Befehl** in diesen Jobs nachgewiesen. Die Runs können deshalb nicht als gescheiterter elektrischer DRC interpretiert werden. Ursache für die Runner-/Job-Vorabfehler ist noch ungeklärt.
- Der Prüflauf ist in `.github/workflows/sensor-pcb-check.yml` definiert, aber die Ausführung ist weiterhin ungeprüft.

## Unabhängige technische Freigabesperren
1. **Nicht vermessene Steckmodule:** ESP32 DevKit 30 Pin (2×15, Reihenmitte vorläufig 25,40 mm) und ADS1115 (1×10). Maßhaltigkeit, Orientierung, USB-Zugang und Steckhöhe vor Serienfertigung prüfen.
2. **Unverifizierte Footprints:** alle eingebetteten Bibliotheksnamen `Grow_Placeholder:...` müssen geometrisch gegen reale Bauteile verglichen werden. Das Vorhandensein einer Padgruppe ist kein Footprint-Nachweis.
3. **Mechanik (historisch):** 179×115 mm ist jetzt CAD-fixiert, 4 M3-Bohrungen gesetzt. Gehäuse und dessen Kabelabgänge folgen erst nach der Prototypplatine; USB-Steckfreiheit an der offenen Platine bleibt gesondert zu prüfen.
4. **Analoge Sicherheit:** ADS1115 mit 3,3 V; pH/EC-Ausgangsspannungen sind für alle Betriebszustände zu prüfen, einschließlich Überspannungsfällen. Ein direkter Anschluss darf ohne Verifikation nicht freigegeben werden.
5. **Versorgung:** 5-V-USB-Abgriff, 3,3-V-Reglerreserve und tatsächlich verwendete Module nicht gemessen.
6. **DRC/ERC und Gerber/Excellon:** keine erfolgreiche fachliche Prüfung und keine freigegebenen Fertigungsdaten.
7. **Funktionstest:** reale bestückte Sensorplatine nicht getestet.

## Bestellung
Keine Produktionsgerber als fertiges Produkt ausgeben. Nur nach Freigabe aller relevanten Punkte durch reale Maß-/Elektroprüfung und bestandenen KiCad-DRC in die Fertigung geben. Keine 230-V-Lastpfade auf diesem Sensorträger.

## Echter KiCad-9.0.2-DRC (VPS-Runner, 16.11.2026, 03:10 UTC)
- GitHub Actions: https://github.com/d4sd1ng/homepage/actions/runs/43796471420
- Lauf auf bestehendem `nurovelle-vps`-Runner; KiCad 9.0.2 isoliert in Debian-Trixie-Dockercontainer.
- Originalresultat nach Routingkorrekturen: **25 Warnungen**, ausschließlich `lib_footprint_issues` für alle 25 `Grow_Placeholder`-Bibliothekseinträge.
- **Keine unverbundenen Pads**, **keine ausgewiesenen Footprint errors**, keine aktuellen `track_dangling`-/`holes_co_located`-Meldungen.
- Der KiCad-CLI-Lauf liefert wegen der Warnungen weiterhin Exit-Code 5. **Keine Fertigungsfreigabe**.
- Beseitigt: 7 doppelte Vias, zwei versehentlich eingeführte kurze GND-Stubs sowie eine fehlende GND-Durchkontaktierung an (41,83 mm; 55,80 mm).
- Die 25 Library-Fehler dürfen nicht durch blindes Umbenennen der Platzhalter kaschiert werden; ihre Geometrie bleibt unbestätigt.
