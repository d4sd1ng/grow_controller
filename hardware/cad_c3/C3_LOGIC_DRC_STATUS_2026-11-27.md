# C3: isolierter PCA9685-Logikteil – Prüfbericht 27.11.2026

## Prüfgegenstand (Niederspannung, keine Aktorlasten)
- Kandidat: `hardware/cad_c3/c3_logic_interconnect_PARTIAL_NOT_FOR_ORDER.kicad_pcb`.
- C3-SuperMini an PCA9685-Pins für 3,3 V, GND, I²C SDA/SCL, /OE.
- PCA9685-Adresse 0x40: Adresspins an GND (entsprechend Firmware, Datenblatt).
- PCA9685-EXTCLK statisch auf GND; ohne externen Takt.
- Die zuvor geprüfte Niederspannungsversorgung wird unverändert fortgeführt.
- **PCA9685-Ausgänge und sämtliche externen Leistungs-/Steueranschlüsse bleiben ohne elektrischen Anschluss.**
- Kein Firmware-Update und keine Änderung der vorhandenen Ausgabesperre.

## Prüfung
- KiCad 9 Strict DRC: **0 DRC-Verstöße**, 0 gemeldete unverbundene Pads, 0 Footprint-Fehler.
- Nachweis: https://github.com/d4sd1ng/homepage/actions/runs/45932965421.
- Diese Prüfung bestätigt ausschließlich Geometrie/Netzzuordnung der vorhandenen modellierten Signale. Nicht vernetzte Pads sind weiterhin ungetestet.

## Produktionsfreigabe
**KEINE Fertigungsfreigabe.** Noch fehlen die vollständige elektrische Auslegung, I²C-Pull-ups, Lokalentkopplung der PCA9685-Versorgung, verifizierte Strombilanz, vollständig überprüfte Schaltpläne/ERC, Gehäuse- und Steckverbinderfreigabe. Externe Netzlast- oder Chemikaliendosier-Ansteuerungen sind nicht Bestandteil dieser sicheren Logikteilprüfung.

Primärquelle PCA9685: https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf
