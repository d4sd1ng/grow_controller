# Statische PCB-Dateiprüfung — Sensorträger (15.11.2026)

**Nur statische Konsistenzprüfung, kein KiCad ERC/DRC und keine
elektrische Funktionsprüfung. Keine Fertigungsfreigabe.**

Prüfgrundlagen:
- `sensor_pcb_unrouted_prototype.kicad_pcb`
- `../sensor_pcb_external_pins.csv`
- `../sensor_pcb_module_pins.csv`
- Hersteller-Steckermasse Phoenix Contact MSTBA 2,5/..-G-5,08.

## 1. Dokumentierte Prüfergebnisse

| Test | Ergebnis |
| --- | --- |
| Bauteilpositionen/Footprints in Vorentwurf | 27 |
| Externe Stecker | 12 |
| Externe Steckerkontakte laut CSV/PCB | 38 / 38 |
| Interne Modul-Steckkontakte laut CSV/PCB | 40 / 40 |
| Bedrahtete Widerstandspositionen | 13 × 2 = 26 Kontakte |
| Gesamt-PCB-Pads | 104 |
| Fehlende oder doppelte Pads in statischer Auswertung | 0 |
| Abweichungen zwischen CSV-Netznamen und KiCad-Padnetzen | **0** |
| Phoenix-Bohrungen | 38 × 1,4 mm |
| Vorläufige Footprint-Konturrechtecke außerhalb Board | 0 |
| Überschneidungen der 12 Steckerrechtecke miteinander | 0 |
| Überschneidungen der Steckerrechtecke mit ESP32-F.Fab-Rechteck | 0 |
| Klammerbalance der .kicad_pcb-Datei | 0 Rest |
| Geroutete Leiterbahnsegmente | **81**, plus **1 Durchkontaktierung (Via)** – Teilbereich pH/EC/ADS1115 |
| Endgültige Platinenabmessung | **Nicht vom Nutzer freigegeben** |

Die anfänglich gefundenen **30 unterschiedlichen Netzbezeichnungen**
in den beiden CSV-Dateien wurden mit den in der
`.kicad_pcb`-Datei verwendeten Netznamen vereinheitlicht:
9 externe Kontaktbezeichnungen und 21 Modulbezeichnungen.

**Kein elektrischer Validierungsnachweis:** Bei der Prüfung wurden
gleichlautende **Namen** von Kontakt zu Netz abgeglichen.
Das prüft weder, ob die analogen Eingangsschaltungen geeignet sind,
noch, ob sämtliche Bauteile auf dem echten PCB physisch verbunden sind.

## 2. Mechanischer Konstruktionsstand

Die vorläufige Außenkontur des Entwurfs lautet
X = 5–184 mm, Y = 5–120 mm, also **179 × 115 mm**.
Die Steckverbinder sind aktuell auf zwei Reihen
oben Y = 13 mm, unten Y = 114 mm verteilt.
Deren vorläufiger F.Fab-Gehäusebereich ist jeweils
12 mm tief. Das **vereinfachte Prüfmodell** benutzt die
Herstellerbreiten:

- MSTBA 2-polig: 12,16 mm
- MSTBA 3-polig: 17,24 mm
- MSTBA 4-polig: 22,32 mm

Es wurde geprüft, dass diese schematisierten Rechtecke
innerhalb der provisorischen Boardkontur liegen.
**Nicht geprüft:** echte Einsteckrichtung/Plug-Auskragung,
Drahtabgang, Steckerbetätigung, tatsächliches Phoenix-3D-Modell,
Lötstift-Padanschlüsse, Gehäusefreiraum, Platzhalter-ADS1115-Bauhöhe,
Bohrungen für PCB-Befestigung, Design Rules.

## 3. Offene elektrische Prüfstellen (bewusst nicht verbunden)

- **ADS1115 A0**: pH-Spannungsteiler / tatsächlich kalibriertes
  Teilungsverhältnis noch unbekannt.
- **ADS1115 A1**: realer EC/TDS-Ausgangspegel über gesamten Bereich
  ungeprüft; keine verbindliche Eingangsschaltung.
- **PH4502C**: die beiden Modul-GND-Kontakte müssen am vorhandenen
  Modul identifiziert werden, bevor sie gemeinsam ausgeführt werden.
- **5 V**: USB-C ist als einzige Controller-Einspeisung festgelegt;
  tatsächliche Belastbarkeit von 5-V-Pin und internem 3,3-V-Regler
  des ESP32-Breakouts nicht bestätigt.
- **A02YYUW**: Hersteller-DFRobot SEN0311 erlaubt 3,3–5 V und RX offen;
  tatsächlich eingesetzter Modul-/Nachbau-Typ noch zu verifizieren.
- **MLX90640**: der vierpolige Steckplatz `J_MLX` bleibt vorhanden;
  echte Breakout-Variante und deren Pull-ups noch zu verifizieren.
- **I²C**: Pull-ups beider Leitungen je Bus nicht durch unkontrollierte
  Bestückung duplizieren.

**ADS1115-Quelle:** Texas Instruments, Datenblatt SBAS444E,
Arbeitsbereich analog GND bis VDD, absolute Grenze
GND−0,3 V bis VDD+0,3 V.
https://www.ti.com/lit/ds/symlink/ads1115.pdf

**A02YYUW-Quelle:** https://wiki.dfrobot.com/sen0311/

**Phoenix-Quellen:**
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-2-g-508-1757242
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-3-g-508-1757255
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-4-g-508-1757268

## 4. Fertigungssperren

Die zur Produktion benötigten Schritte sind **nicht abgeschlossen**:

1. Funktionsfähigen vollständigen Schaltplan mit belastbaren
   Analog- und Versorgungsnetzen erstellen.
2. Mechanische Baugruppen und echte Footprints gegen Bauteile abgleichen.
3. Alle weiteren Sensornetze fertig routen (pH/EC/ADS1115 ist teilweise geroutet).
4. KiCad ERC/DRC tatsächlich ausführen und Findings bereinigen.
5. Gerber und Excellon aus genau dem geprüften Board exportieren
   und visuell prüfen.

**KiCad-CLI steht in der aktuellen Bearbeitungsumgebung nicht zur
Verfügung. Daher keine Behauptung einer erfolgreichen KiCad-Prüfung.**


## Nachprüfung nach ADC-Layoutänderungen am 15.11.2026

Der KiCad-Vorentwurf enthält jetzt **81 echte `segment`-Objekte**
auf F.Cu/B.Cu und **ein `via`**. Dafür wurden statisch überprüft:

- **78/78** Kontakt-Netz-Einträge aus beiden CSV-Tabellen passen zu den
  PAD-Netzen im KiCad-Entwurf (38 externe, 40 interne Kontaktpositionen).
- **15/15** exemplarisch abgefragte pH-/EC-/ADS1115-Verbindungspaare
  sind auf Segment- und Lagenebene elektrisch zusammenhängend.
  Darunter EC→ADS A1 direkt, PO→R_PH_TOP, R_PH_TOP/R_PH_BOTTOM→ADS A0,
  PH/EC 5-V-Versorgung→ESP32-Pin15, PH-GND1/GND2→GND,
  ADS VDD→ESP32 3V3, ADS GND→ESP32 GND, SDA→GPIO21, SCL→GPIO22,
  ADDR→ADS GND.
- **0** geometrisch erkannte fremde Pad-/Leiterbahn-Kollisionen
  für den teilgerouteten Bereich (0,2-mm-Prüfabstand plus Kupferradien).
- **0** geometrisch erkannte Fremdnetz-Leiterbahn-Abstandsverletzungen
  auf derselben Kupferlage, **0** erkannte Via-Kollisionen.

**Grenzen:** Das sind eigene geometrische/netzbezogene statische
Prüfungen, **kein KiCad-DRC/ERC**; sie ersetzen weder den Schaltplan
noch die reale Prüfung der Bauteile und des pH-Widerstandsnetzwerks.
`R_PH_TOP` und `R_PH_BOTTOM` sind nur **physisch geroutete,
aber noch ohne bestätigte Bestückungswerte** vorgesehene Positionen.
Der alte Plan (10/20 kΩ) ist als veraltet gekennzeichnet und keine
verlässliche Freigabe der Ist-Werte. Die übrigen Sensoranschlüsse sind
noch nicht vollständig geroutet. **Nicht fertigen.**
