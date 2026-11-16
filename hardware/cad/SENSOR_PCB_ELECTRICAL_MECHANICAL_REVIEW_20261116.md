# Sensor PCB — elektrische und mechanische Prüfung
**Datum:** 16.11.2026 — **Prüfstatus:** CAD-Prüfung erfolgt, physische Fertigungsfreigabe noch nicht erteilt.

## Reproduzierbare Prüfergebnisse
- Prüflauf des vorhandenen VPS-Runners: https://github.com/d4sd1ng/homepage/actions/runs/43964647375
- **KiCad 9.0.2:** 0 DRC-Meldungen, 0 unverbundene Pads, 0 Footprint-Fehler.
- `sensor_pcb_mechanical_electrical_audit.py`: Die Netz-/Pinzuordnung **aller 25 Footprints** entspricht der projektseitigen Anschlussliste.
- **25/25 Footprints** liegen innerhalb der 179 × 115 mm Edge.Cuts und ihre **Courtyards überschneiden sich nicht**.
- 2 Kupferlagen. Gemeldete Bestückungslöcher für mechanische Befestigung: **0**.

## Kritischer mechanischer Prüfpunkt: Steckraum und USB
Die KiCad-Footprints bilden bisher die Grundleisten, **nicht** den ganzen Einsteckkörper der zugehörigen MSTB-Schraubstecker und die angeschlossenen Kabel ab. Deshalb reicht ein fehlerfreier DRC nicht als Steckraumprüfung.

**Konkrete Positionen im bestehenden Board (globale mm-Koordinaten):**

| Teil | Courtyard x | Courtyard y | Nahegelegenes Teil | Trennung |
|---|---|---|---|---:|
| U_ESP32 | 53,20–82,20 | 50,50–102,50 | — | — |
| J_BH1750 | 37,96–61,28 | 103,50–116,50 | U_ESP32 | **1,00 mm** |
| J_MLX | 66,96–90,28 | 103,50–116,50 | U_ESP32 | **1,00 mm** |

Das ESP32-Modul besitzt seinen USB-Anschluss an der **unteren Seite** der gezeichneten Modulaufnahme. Die zwei genannten Steckverbinder befinden sich unmittelbar davor. **Die Stecker und der USB-Stecker können in diesem CAD-Stand nicht gemeinsam als mechanisch passend bestätigt werden.**

Der Phoenix-Contact-Hersteller nennt für die MSTBA-Grundleiste 12 mm Baulänge, 5,08 mm Kontaktabstand, 1,4 mm PCB-Bohrung, und die Zeichnung mit gestecktem Gegenstück zeigt ein weiteres Einsteckmaß/den 22-mm-Gesamtbereich. Der für einen sicheren Kabelabgang erforderliche freie Raum ist in den vorhandenen Courtyards nicht modelliert.

Quellen:
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-3-g-508-1757255
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-4-g-508-1757268
- https://media.digikey.com/pdf/data%20sheets/phoenix%20contact%20pdfs/1757255.pdf

**Nötige CAD-Korrektur:** Zugang zum USB-C-Anschluss ohne Hindernis durch J_BH1750/J_MLX samt Gegensteckern und Kabeln herstellen. Dafür ist eine explizite Änderung von Steckeranordnung, Boardkontur oder Modulposition im Layout nötig. Die genaue Lösung kann nicht ohne Festlegung des Gehäuseeinbaus konstruktiv freigegeben werden.

## Elektrischer Anschlussabgleich — bestanden auf Netzebene
Alle 25 Footprints mit sämtlichen Pins gegen die Projekt-Netzliste geprüft. Wesentliche Ergebnisse:
- DHT22 ×3: 3V3 / GPIO25,26,27 / GND; je 4,7-kΩ-Pull-up nach 3V3.
- DS18B20: 3V3 / GPIO23 / GND, 4,7-kΩ-Pull-up.
- ADS1115: VDD = 3V3, GND, SCL = GPIO22, SDA = GPIO21, ADDR = GND, A0 = pH-PO, A1 = EC/TDS.
- MLX90640: 3V3/GND/GPIO21/GPIO22.
- BH1750: 3V3/GND/GPIO13/GPIO14.
- RGB-LED: Rot = GPIO33, Grün = GPIO4, Blau = GPIO17, je mit separater THT-Widerstandsposition; gemeinsame Kathode GND.
- Taster: GPIO16/GND und GPIO19/GND.
- Kein von der bisherigen Beschaltung abweichender pH-Spannungsteiler erfunden.

**Grenze des Prüfverfahrens:** Ein Netzvergleich zeigt keine realen Stromwerte, keine Signalqualität und keine sicheren Pegel an. Die beiden direkt angeschlossenen ADS1115-Analogeingänge benötigen bei 3,3 V ADS1115-VDD eine verifizierte maximale Eingangsspannung und sichere Betriebsbedingungen. Das echte 5-V-/3,3-V-Lastbudget und die Sensor-Signalpegel sind hier nicht gemessen.

## Fertigungsfreigabe — offene Punkte
1. **Montage:** keine Befestigungsbohrungen oder abgestimmtes Einbaukonzept im PCB.
2. **Stecker/USB:** 1 mm CAD-Freiraum vor USB-Bereich; der gesteckte Phoenix-Gegenstecker samt Kabelabgang wurde nicht mit dem ESP32 und Gehäuse abgeglichen.
3. **Elektrik:** Realmessung der Versorgungs- und ADC-Pegel sowie Sensor-Funktionsprobe auf dem realen Aufbau ist nicht über GitHub möglich.

**Fazit:** Die Software-/CAD-Netzprüfung ist bestanden. Ein real bestückter, mechanisch zugänglicher und elektrisch nachgewiesener Prototyp ist damit noch **nicht** festgestellt. Keine Freigabedatei `FABRICATION_RELEASE_APPROVED` erzeugen und keine vorliegenden Vorschau-Gerber als Bestellfreigabe behandeln.
