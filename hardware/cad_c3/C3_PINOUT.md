# C3-SuperMini-Pinbelegung im CAD
Quelle: vorhandenes 2×8-Maßbild und `firmware/C3/C3.ino`. Die Orientierung gilt **Draufsicht auf das C3-Modul, USB-C nach oben**. Padnummern gehören zum neu angelegten KiCad-Footprint.

| C3-Sockel-Pad | Beschriftung | Firmware-Nutzung |
|---:|---|---|
| 1 | 5V | **keine 5-V-Zuleitung über Trägerplatine**, Strom nur USB-C |
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
