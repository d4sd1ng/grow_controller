# Sensorplatine — Footprint-Zuordnung aller 25 Positionen

Quelle: bestehendes Sensor-Layout und verbindliches Maßbild. **Alle 25 Positionen sind zugeordnet**; die Padpositionen wurden für das bestehende Routing erhalten. Die Bibliothek ist lokal im Projekt hinterlegt und enthält mechanische Konturen/Courtyards. Nicht mit einer bestückungsgeprüften Baugruppe verwechseln.

| Ref | Projekt-Footprint | Funktion | Bestückung |
|---|---|---|---|
| `J_DHT1` | `Grow_Controller_Sensor:MSTBA_3P_5p08_to_SOUTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_DHT2` | `Grow_Controller_Sensor:MSTBA_3P_5p08_to_SOUTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_DHT3` | `Grow_Controller_Sensor:MSTBA_3P_5p08_to_SOUTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_DS18` | `Grow_Controller_Sensor:MSTBA_3P_5p08_to_SOUTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_LEVEL` | `Grow_Controller_Sensor:MSTBA_3P_5p08_to_SOUTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_EC` | `Grow_Controller_Sensor:MSTBA_3P_5p08_to_SOUTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_PH` | `Grow_Controller_Sensor:MSTBA_4P_5p08_to_NORTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_BH1750` | `Grow_Controller_Sensor:MSTBA_4P_5p08_to_NORTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_MLX` | `Grow_Controller_Sensor:MSTBA_4P_5p08_to_NORTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_RGB` | `Grow_Controller_Sensor:MSTBA_4P_5p08_to_NORTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_PHASE` | `Grow_Controller_Sensor:MSTBA_2P_5p08_to_NORTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `J_SERVICE` | `Grow_Controller_Sensor:MSTBA_2P_5p08_to_NORTH` | Steckverbinder | Phoenix MSTBA 5,08 mm |
| `U_ESP32` | `Grow_Controller_Sensor:ESP32_30Pin_25p40_28x51` | ESP32-Steckmodul | Steckbar |
| `U_ADS1115` | `Grow_Controller_Sensor:ADS1115_Module_1x10_P2p54` | ADS1115-Steckmodul | Steckbar |
| `R_DHT1` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | 4,7 kΩ |
| `R_DHT2` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | 4,7 kΩ |
| `R_DHT3` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | 4,7 kΩ |
| `R_DS18` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | 4,7 kΩ |
| `R_RGB_R` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | 220 Ω geplant |
| `R_RGB_G` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | 220 Ω geplant |
| `R_RGB_B` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | 220 Ω geplant |
| `R_I2C0_SDA` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | DNP bis Pull-up-Abgleich |
| `R_I2C0_SCL` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | DNP bis Pull-up-Abgleich |
| `R_I2C1_SDA` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | DNP bis Pull-up-Abgleich |
| `R_I2C1_SCL` | `Grow_Controller_Sensor:Axial_THT_DIN0207_P7p62` | THT-Widerstand | DNP bis Pull-up-Abgleich |

## Verbindliche Kontakt-/Körpergeometrie

- Phoenix 2p/3p/4p: 5,08-mm-Pitch, Bohrung 1,4 mm, 2,5-mm-Padring; Gehäuseumrisse auf Grundlage der Herstellergeometrie, Nord-/Süd-Ausrichtung entsprechend der vorhandenen Leiterplatte.
- ESP32: 2×15, längs 2,54 mm, Reihen 25,40 mm, Körperkontur 28×51 mm gemäß Maßbild.
- ADS1115: 1×10 mit 2,54-mm-Pitch; vorgegebene Pins von 1 bis 10.
- THT-Widerstände: 7,62 mm Anschlussabstand, Bohrung 1,0 mm. DIN0207-Körperkontur 6,3×2,5 mm **als verwendete Footprint-Bauform**, nicht als Behauptung über unbekannte Lagerteile.

## Fertigungsprüfung

Eine Zuordnung ist kein Ersatz für KiCad-DRC, korrekte Montage- und Gehäusefreigabe, Spannungsgrenzenprüfung und einen Test der fertigen Baugruppe. Insbesondere überprüft die fertigungstechnische Sichtprüfung die Steckrichtung und Zugriff auf alle abziehbaren Gegenstecker.

**Nicht eigenständig zur Produktion freigegeben.**
