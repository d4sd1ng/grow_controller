> **Aktueller Stand Rev A — 17.11.2026:** PCB mit **25 elektrischen Footprints und 4 M3-NPTH-Montagebohrungen (29 Gesamtpositionen)** auf `main`. **0 KiCad-DRC-Fehler, 0 unverbundene Pads**. Reproduzierbarer Check über bestehendem `nurovelle-vps`-Runner: https://github.com/d4sd1ng/homepage/actions/runs/43981496756 . Getrennte Excellon-Dateien für PTH und NPTH nachgewiesen: https://github.com/d4sd1ng/homepage/actions/runs/43980832773 . Neues Rev-A-CAM-Reviewarchiv unter https://github.com/d4sd1ng/grow_controller/pull/13 . **Endgültige Gehäuse-, USB- und elektrische Freigabe weiterhin offen.**

# Sensorplatine — gerouteter Prüfstand, 15.11.2026

**Verbindliche Arbeitsdatei:** `sensor_pcb_routed_candidate.kicad_pcb`.

**Status: Alle 25 Footprint-Positionen mit Projektbibliothek zugeordnet; KiCad 9.0.2 DRC erfolgreich mit 0 Meldungen. Eine mechanische und elektrische Freigabe ist davon getrennt.**

## Umfang der Sensorplatine

| Element | Stand |
|---|---:|
| Kupferlagen | 2: F.Cu / B.Cu |
| Vorläufige Außenkontur | 179 × 115 mm |
| Externe steckbare Phoenix-Klemmen | 12 (2×2, 6×3, 4×4) |
| Externe Kontaktpositionen | 38 |
| Steckmodul ESP32 DevKit, 30 Pin | 2 × 15, Reihenmitte 25,40 mm, Pinraster 2,54 mm |
| Steckmodul ADS1115, 10 Pin | 1 × 10, Pinraster 2,54 mm |
| Bedrahtete Widerstandspositionen | 11: 4 Daten-Pull-ups, 3 RGB, 4 optionale I²C |
| Elektrische Footprints | 25 |
| M3-NPTH-Montagebohrungen | 4 |
| Gesamt-Footprints Rev A | 29 |
| Leiterplattenpads | 100 |
| Geroutete Kupfersegmente | 1211 |
| Vias | 58 |
| Netze mit mindestens 2 Pads | 22 |
| Statisch unverbundene Mehrfachnetze | **0** |
| Statisch erkannte Fremdnetzkollisionen (0,2-mm-Mindestabstand) | **0** |

Alle Zahlen aus der Auswertung der konkreten
`sensor_pcb_routed_candidate.kicad_pcb`-Quelle (2026-11-15).
Eine solche statische geometrische Prüfung **ist kein
KiCad-DRC/ERC** und schließt weder falsch modellierte Footprints
noch Montageprobleme aus.

## Elektrische Konstruktionszuordnung

| Signal | Verbindung im PCB |
|---|---|
| pH Versorgung | Sensor-ESP32 USB-5-V-Netz → J_PH.1 |
| pH Masse | GND → J_PH.2/J_PH.3 |
| **pH Messung** | **PH4502C PO J_PH.4 → ADS1115 A0, direkt** |
| EC Versorgung | Sensor-ESP32 USB-5-V-Netz → J_EC.1 |
| EC Masse | GND → J_EC.2 |
| **EC Messung** | **J_EC.3 → ADS1115 A1, direkt** |
| ADS1115 Logik | 3V3, GND, SDA GPIO21, SCL GPIO22, ADDR an GND |
| MLX90640 | J_MLX: 3V3, GND, SDA GPIO21, SCL GPIO22 |
| A02YYUW | J_LEVEL: 3V3, UART TX → GPIO18, GND |
| RGB (gemeinsame Kathode) | externe LED, GND + Rot GPIO33/Grün GPIO4/Blau GPIO17 jeweils über einen einzelnen bedrahteten Widerstand |
| Phasentaster / Servicetaster | GPIO16/GND und GPIO19/GND |

### Korrektur der historischen pH-Schaltung

Ein früherer Plan hatte einen **10-kΩ/20-kΩ-Spannungsteiler**
nur vermutet. Der Nutzer hatte bereits klargestellt, dass dieser
nicht als tatsächlich verbaut bestätigt ist; für die vorhandene
Schaltung wurden **pH PO um 3,0 V** und
**PO → ADS1115 A0** dokumentiert. Die
`R_PH_TOP`-/`R_PH_BOTTOM`-Footprints und deren Schaltung wurden
deshalb aus dem neuen gerouteten Board **entfernt**.
Die bestehende Firmware/Kalibrierlogik wird durch diese
PCB-Arbeit nicht verändert. Die bestehende Sensorfunktion bleibt
am realen Gerät zu prüfen; frühere Einzelmessungen sind kein
Abschlusstest der neuen PCB.

## Herstellergeometrie der Klemmen

| Phoenix MSTBA | Artikelnummer | Breite | Pinraster | Leiterplattenbohrung |
|---|---:|---:|---:|---:|
| 2-polig | 1757242 | 12,16 mm | 5,08 mm | 1,4 mm |
| 3-polig | 1757255 | 17,24 mm | 5,08 mm | 1,4 mm |
| 4-polig | 1757268 | 22,32 mm | 5,08 mm | 1,4 mm |

Datenquelle jeweils Phoenix Contact, Originalproduktseiten:
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-2-g-508-1757242
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-3-g-508-1757255
- https://www.phoenixcontact.com/de-de/produkte/leiterplattengrundleiste-mstba-25-4-g-508-1757268

Die Board-Klemmenflächen haben 1,4-mm-Bohrungen und
5,08-mm-Raster. Die F.Fab-Konturen bilden die äußere
Körperbreite und angenähert 12-mm-Tiefe ab; **Steckerraum und
Kabelabgang sind nicht anhand eines exakten Hersteller-3D-Modells
überprüft**.

## Bestückung ohne SMD-Widerstände

- 4 × 4,7 kΩ für DHT22 ×3 / DS18B20 (bereits vorhandene
  Breakout-Pull-ups vorher prüfen).
- 3 × einzelne 220 Ω bevorzugt für die **eine vorhandene externe
  RGB-LED**, elektrische Eignung für die tatsächliche LED vor Bestückung
  prüfen.
- 4 × zusätzliche optionale I²C-Pull-up-Footprints, **DNP**,
  sofern auf den Modulen Pull-ups vorhanden sind.
- Keine weiteren LEDs auf der Trägerplatine.
- **Nur bedrahtete Widerstandspositionen.** Gehäuse-/Drahtmaße
  des vorhandenen Bestandes für endgültige Footprintfreigabe prüfen.

## KiCad-DRC vom 16.11.2026

Mit KiCad 9.0.2 auf dem bestehenden VPS-Runner tatsächlich geprüft (Run 43796471420). Nach Entfernung doppelt liegender Vias und Korrektur der GND-Durchkontaktierung: **0 unverbundene Pads, 0 akute Routing-Warnungen** beim damaligen Placeholder-Stand. **Aktuell: 0 DRC-Meldungen nach expliziter Belegung aller 25 Projekt-Footprints** (Run 43962759317). Noch keine Fertigungsfreigabe.

## Echte Restpunkte vor Bestellung

1. **KiCad-DRC bestanden:** 0 Warnungen/Fehler, 0 unverbundene Pads (KiCad 9.0.2, Runner 43962759317). Alle 25 Footprints verwenden `Grow_Controller_Sensor`.
2. **Footprints:** sechs definierte Projekttypen, vollständig dokumentiert in `FOOTPRINT_ASSIGNMENTS_25.md`; vor der konkreten Bestückung Steckorientierung und Einsteckraum abgleichen.
3. **Mechanik:** 179 × 115 mm ist eine **vorläufige** Außenkontur;
   Gehäuse, Befestigungslochbild und Montagehöhen sind nicht
   freigegeben. Der Entwurf enthält keine ungefragt erfundenen
   Befestigungslöcher.
4. **Betrieb:** Modultypen und Versorgungspfade bei der
   realen Bestückung abgleichen, insbesondere tatsächliches
   MLX90640-Breakout und Sensor-ESP32-3,3-V-Ausgang.
5. **Test:** Arduino-/Hardwaretest der gefertigten Platine
   einschließlich pH/EC, Kamera und der I²C-Busse fehlt.
6. **Gerber/Excellon:** Prüfdaten wurden bereits erzeugt und als `NOT_FOR_ORDER`-Archiv archiviert (PR #13). Freigegebene Produktionsdaten erst nach Mechanik- und Elektrikprüfung.

**Kein automatischer Fertigungsauftrag, kein Gerber-Freigabevermerk.**
