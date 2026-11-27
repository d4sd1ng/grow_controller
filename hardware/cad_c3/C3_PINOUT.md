# C3-SuperMini-Pinbelegung im CAD
Quelle: vorhandenes 2×8-Maßbild und `firmware/C3/C3.ino`. Die Orientierung gilt **Draufsicht auf das C3-Modul, USB-C nach oben**. Padnummern gehören zum neu angelegten KiCad-Footprint.

| C3-Sockel-Pad | Beschriftung | Firmware-Nutzung |
|---:|---|---|
| 1 | 5V | **5 V vom 12-V→5-V-Buck über rückspeisegeschützte Versorgung**; USB-C nur Service mit Entkopplung |
| 2 | GND | Masse |
| 3 | 3V3 | Strombudget für Zusatzlogik prüfen |
| 4 | GPIO4 | logischer Ausgang 4 |
| 5 | GPIO3 | logischer Ausgang 3 |
| 6 | GPIO2 | strapping; **reserviert** |
| 7 | GPIO1 | logischer Ausgang 2 |
| 8 | GPIO0 | logischer Ausgang 1 |
| 9 | GPIO5 | logischer Ausgang 5 |
| 10 | GPIO6 | Reserve |
| 11 | GPIO7 | I²C SDA |
| 12 | GPIO8 | strapping / Board-LED; reserviert |
| 13 | GPIO9 | strapping / BOOT; reserviert |
| 14 | GPIO10 | I²C SCL |
| 15 | GPIO20 | PCA /OE; UART RX-Bootkonflikt prüfen |
| 16 | GPIO21 | UART TX / Reserve |

**Hinweis:** Der CAD-Footprint ist aus dem festgelegten Maßbild und dem gebräuchlichen SuperMini-Pinout erstellt, **nicht** an einem tatsächlichen aufgesteckten Modul geprüft. Die Beschriftungen sind für die Padorientierung, kein elektrischer Anschlussnachweis.

PCA9685PW (TSSOP28) Padzuordnung nach NXP-Datenblatt:
- 1–5 A0–A4; 6–13 OUT0–OUT7; 14 GND
- 15–22 OUT8–OUT15; 23 /OE; 24 A5; 25 EXTCLK; 26 SCL; 27 SDA; 28 VDD

Die Frontend-/SSR-/Motorverbindungen sind **nicht geroutet**. Nicht an eine Last anschließen.

## Versorgungsvorgabe 27.11.2026
- C3-Trägerplatine bekommt nur einen **12-V-Betriebseingang an J1**.
- Der **auf der Trägerplatine vorzusehende Step-down-Konverter** erzeugt daraus geregelte 5 V für C3 (Pin 1); Masse an C3 Pin 2.
- 5 V nicht an C3-3V3 oder GPIOs legen; die Spannung/Versorgung für den PCA9685 beträgt weiterhin 3,3 V.
- Der ehemalige Plan „C3 nur über USB-C versorgen“ ist überholt. USB-C muss für Service erreichbar bleiben. Bei gleichzeitig anliegender externer 5-V-Logikversorgung darf keine Rückspeisung zwischen USB und Step-down entstehen: geprüfter Versorgungswahl-/Sperrpfad zwingend.
- Im KiCad-Floorplan sind J1 Pad 1 (+12V_IN), J1 Pad 2 (GND), C3 Pad 1 (+5V_BUCK) und C3 Pad 2 (GND) als **noch ungeroutete Netze** markiert.
