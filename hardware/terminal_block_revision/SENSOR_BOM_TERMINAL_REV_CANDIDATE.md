# Sensorplatine – Stückliste Schraubklemmen-Revision (Entwurf)
Stand: 28.11.2026. Quelle: sensor_terminal_routing_CANDIDATE_NOT_FOR_ORDER.kicad_pcb, Blob 6bac77d93dcca548f4fe8717edc1c10dd2c1777d; Werte aus SENSOR_PCB_BOM_REV_A.md. Alle 23 Footprints abgeglichen. Keine Fertigungsfreigabe.

| Position | Menge | Ausführung | Status |
|---|---:|---|---|
| K1–K3 | 3 | Same Sky TB004-508-12BE, 12-polig, 5,08 mm | Kandidat, reale Passung offen |
| K4 | 1 | Same Sky TB004-508-02BE, 2-polig, 5,08 mm | Kandidat, reale Passung offen |
| U_ESP32 | 1 | Vorhandenes ESP32 DevKit, 30 Pins | Steckmodul |
| U_ESP32-Aufnahme | 2 | Buchsenleiste 1×15, 2,54 mm, THT | Steckhöhe prüfen |
| U_ADS1115 | 1 | Vorhandenes ADS1115-Modul, 10 Pins | Steckmodul |
| U_ADS1115-Aufnahme | 1 | Buchsenleiste 1×10, 2,54 mm, THT | Artikel offen |
| R_PH_TOP | 1 | 10 kΩ, axial THT, 7,62 mm | Wert laut bisheriger BOM |
| R_PH_BOTTOM | 1 | 20 kΩ, axial THT, 7,62 mm | Wert laut bisheriger BOM |
| R_DHT1–3 | 3 | 4,7 kΩ, axial THT, 7,62 mm | bedingt, Modul prüfen |
| R_DS18 | 1 | 4,7 kΩ, axial THT, 7,62 mm | Wert laut bisheriger BOM |
| R_RGB_R/G/B | 3 | 220 Ω, axial THT, 7,62 mm | LED-Daten prüfen |
| R_I2C0/1_SDA/SCL | 0 | 4 unbestückte Widerstandspositionen (DNP) | nicht bestellen |
| H1–H4 | 4 | NPTH Ø3,2 mm | Bohrungen, keine Bauteile |

19 elektrische Footprints, 4 Montagebohrungen; keine Phoenix-Stecker, keine SMD-Bestückung auf Trägerplatine. Die Stückliste enthält keinen aktualisierten Gerber-/Bohrdatensatz. Elektrische Verifikation und KiCad-DRC sind nicht durch die statische Prüfung ersetzt. Keine Preise oder Verfügbarkeit verifiziert. Die C3-Leiterplatte ist nicht Teil dieser Stückliste und nicht freigegeben.
