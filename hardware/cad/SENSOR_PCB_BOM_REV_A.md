# Sensor-PCB — Stückliste Prototyp Rev A (17.11.2026)

**PCB:** 179 × 115 × 1,6 mm, zweilagig, 27 elektrische Footprints und vier nichtmetallisierte M3-Befestigungsbohrungen. **Keine** Bauteile der separaten ESP32-C3-/SSR-/Dosierpumpenplatine enthalten.

## Phoenix-Stecksystem — fertig zugeordnet
| Bezeichnung | Artikel | Menge | Gegenstück | Menge | Steckrichtung |
|---|---:|---:|---:|---:|---|
| MSTBA 2,5/2-G-5,08 | Phoenix **1757242** | 2 | MSTB 2,5/2-ST-5,08 **1757019** | 2 | parallel zur Leiterplatte |
| MSTBA 2,5/3-G-5,08 | Phoenix **1757255** | 6 | MSTB 2,5/3-ST-5,08 **1757022** | 6 | parallel zur Leiterplatte |
| MSTBA 2,5/4-G-5,08 | Phoenix **1757268** | 4 | MSTB 2,5/4-ST-5,08 **1757035** | 4 | parallel zur Leiterplatte |

**12 Grundleisten und 12 passende Gegenstecker.** Alle 12 Positionen sind im PCB bezeichnet. Herstellerkataloge der ausgewählten Artikel bestätigen 5,08 mm Anschlussraster und passende Gegenstücke.

## Module und Buchsenleisten
| Ref | Menge | Festgelegte Konstruktion | Beschaffungs-/Bestückungsstatus |
|---|---:|---|---|
| U_ESP32 | 1 | Vorhandenes ESP32-Pro-DevKit, 30 Pins (2×15), Modulkörper 28×51 mm | vorhandenes Modul; tatsächliche Versorgung über dessen USB |
| Aufnahme U_ESP32 | 2 | einreihige Buchsenleisten **1×15, 2,54 mm**, idealerweise erhöhte Variante | 18,67-mm-Elevation der E-tec-BE1-Serie, Style 181, dokumentiert; **exakte 15-polige Artikelvariante vor Bestellung bestätigen** |
| U_ADS1115 | 1 | Vorhandenes ADS1115-Modul, **1×10, 2,54 mm** | vorhandenes Modul |
| Aufnahme U_ADS1115 | 1 | Buchsenleiste **1×10, 2,54 mm**, THT | Hersteller-/Bestellnummer noch nicht festgelegt |

**Wichtig:** Die erhöhte ESP32-Steckaufnahme ist eine gezielte Lösung für den eng begrenzten USB-C-Steckraum, kein Nachweis der mechanischen Steckbarkeit bei installiertem Gegenstecker. Für das **spätere** Sensor-Gehäuse die erforderliche Höhe aus dem realen Prototyp übernehmen.

## Bedrahtete Widerstände

**pH-Signalweg gemäß früher dokumentierter Steckbrett-Verdrahtung:**
`J_PH.4 (PO) → R_PH_TOP (10 kΩ) → U_ADS1115.7 (A0)`;
`A0 → R_PH_BOTTOM (20 kΩ) → GND`. EC/TDS an `J_EC.3`
bleibt direkt mit `U_ADS1115.8 (A1)` verbunden, entsprechend der
bisherigen A1-Festlegung. Der Spannungsteiler erzeugt nominell
`V_A0 = V_PO × 20/(10+20) = (2/3) × V_PO`;
er ersetzt keinen überprüften Eingangsschutz und erfordert einen
neuen Abgleich der pH-Kalibrierung nach Bestückung.

| Ref | Menge | Wert | Status |
|---|---:|---:|---|
| R_PH_TOP | 1 | 10 kΩ | in PCB als pH-Serienwiderstand geroutet; realen Aufbau abgleichen |
| R_PH_BOTTOM | 1 | 20 kΩ | in PCB von A0 nach GND geroutet; realen Aufbau abgleichen |
| R_DHT1, R_DHT2, R_DHT3 | 3 | 4,7 kΩ | Bestückung abhängig von tatsächlichen Pull-ups der Module |
| R_DS18 | 1 | 4,7 kΩ | vorgesehen als 1-Wire-Pull-up |
| R_RGB_R/G/B | 3 | 220 Ω je Kanal | Wert bevorzugt, vor Bestückung gegen vorhandene RGB-LED-Daten prüfen |
| R_I2C0_SDA/SCL, R_I2C1_SDA/SCL | 4 | noch unbestimmt | **DNP**, keine unkontrollierte Parallelbestückung |

**Alle dreizehn Positionen:** bedrahteter Axial-Footprint `Axial_THT_DIN0207_P7p62`, Raster 7,62 mm. Keine SMD-Widerstände.

## Befestigung
Vier Ø3,2-mm-NPTH-Bohrungen H1–H4 im KiCad-Board, M3 vorgesehen. Die M3-Koordinaten definieren das **spätere** Sensor-Gehäuse-Lochbild; derzeit existiert nur das getrennte Relaisgehäuse. Schraubenköpfe/Standoffs später passend auswählen.

## Technische Prüfung
- Vor der pH-Widerstandsänderung: KiCad 9.0.2 mit 0 DRC-Warnungen/Fehlern, 0 unverbundenen Pads (https://github.com/d4sd1ng/homepage/actions/runs/43980832773). **Für die neue Rev-A-pH-Leiterbahnänderung ist ein aktueller KiCad-DRC noch nachzuholen.**
- Alte Fertigungsdateien zu Review-Zwecken: getrennte **PTH/NPTH**-Excellon-Dateien und Gerber, https://github.com/d4sd1ng/grow_controller/pull/13. **Diese sind vor der pH-Leiterbahnänderung erzeugt und dürfen für die neue PCB-Version nicht bestellt werden.**
- Offene Montage-/Funktionstests: USB-C- und Phoenix-Gegenstecker gemeinsam, 3,3-/5-V-Strombudget sowie ADS1115-A0/A1-Pegel im realen Betrieb.

**Prototyp-Reihenfolge:** Platine (unbestückt) als Prototyp herstellen → bestücken und elektrische Funktion prüfen → erst danach passendes Sensor-Gehäuse konstruieren. Diese Stückliste allein löst keine Bestellung aus und erteilt keine Live-Betriebsfreigabe.
