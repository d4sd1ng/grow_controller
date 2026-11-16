# Sensor-PCB — M3-Montagebohrungen, Prototyp Rev A

**Gültiger Entwurf:** `sensor_pcb_routed_candidate.kicad_pcb`, Zweilagen-Platine 179 × 115 × 1,6 mm, Koordinaten aus `Edge.Cuts`: (5,5) bis (184,120).

| Referenz | PCB-Koordinate X/Y (mm) | Bohrung | Ausführung |
|---|---|---|---|
| H1 | 30 / 36 | Ø3,2 mm | NPTH, M3 |
| H2 | 29 / 84 | Ø3,2 mm | NPTH, M3 |
| H3 | 172 / 36 | Ø3,2 mm | NPTH, M3 |
| H4 | 174 / 97 | Ø3,2 mm | NPTH, M3 |

Die vier Bohrungen sind **kein Versuch, eine bereits existierende Sensor-Gehäusewand zu treffen**. Das aktuell vorhandene separate Gehäuse gehört zur **Relaissteuerung**, nicht zu dieser Platine.\n\nDie 4 Bohrungen wurden computergestützt auf bestehenden Kupferlagen, Footprint-Courtyards und Board-Edge positioniert. Alle vier sind eine **konkrete mechanische Prototyp-Auslegung**; sie sind **die Konstruktionsvorgabe für ein späteres Sensor-Gehäuse**, das aktuell nicht existiert. Für jede Schraubstelle wurde ein kreisförmiger Ø6,4-mm-Courtyard für den Platzbedarf vorgesehen. **Die reale Schraubkopf-/Unterlegscheibenabmessung muss innerhalb des vorgesehenen mechanischen Umrisses liegen.**

Prüfnachweis: https://github.com/d4sd1ng/homepage/actions/runs/43979793594. KiCad 9.0.2: 0 DRC-Meldungen, 0 offene Pads, 0 Footprintfehler; separate statische Prüfung: 0 Courtyard-Überschneidungen, 0 Elemente außerhalb der Außenkontur, exakt 4 NPTH-Montagebohrungen. 

**Unabhängig verbleibende Prüfung:** Der Zugang zu USB-C am steckbaren ESP32-Pro-Modul wird durch die gegenwärtige Geometrie der benachbarten Phoenix-Grundleisten/Gegenstecker nicht automatisch gewährleistet. Erst nach mechanischer Freigabe der realen Buchsenhöhe/Gegenstecker und nach elektrischem Funktionstest darf das Board als Live-Controller eingesetzt werden.

Keine 230-V-Führung auf der Sensorplatine. Kein automatisches Fertigungs-Release.
