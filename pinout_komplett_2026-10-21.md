# Grow-Controller – eindeutige Pinout-Tabelle (Stand 21.10.2026)

**Geltungsbereich:** zwei getrennte Boards. Sensor-ESP32-OLED fuer Kleinspannungs-Messung/Bedienfeld; ESP32-C3 SuperMini fuer geplante 3,3-V-SSR-Steuersignale in einer gesonderten Leistungsbox. **Keinerlei Netzspannungs-/SSR-Lastverdrahtung auf der Sensorplatine.**

**Nummerierung:** Die erste Spalte zaehlt **Loecher je Seitenreihe VON OBEN**, also unabhaengig von der GPIO-Nummer. *Links* und *rechts* sind Ansichten auf die **Bauteilseite**, **nicht** die Rueckseite. Die aufgedruckte Beschriftung ist beim realen Board zu pruefen.

## A. Sensor-ESP32 mit OLED – alle 30 Kontakte

**Ansicht: OLED sichtbar, USB UNTEN**; jeweils 15 Kontakte pro Seite.

| Kontakt von oben | Linke Reihe: Aufdruck / GPIO / Funktion | Rechte Reihe: Aufdruck / GPIO / Funktion |
|---:|---|---|
| 1 | EN / – / Reset | D23 / GPIO23 / DS18B20-DATA |
| 2 | VP / GPIO36 / frei, nur Eingang | D22 / GPIO22 / I²C SCL (ADS/OLED/MLX) |
| 3 | VN / GPIO39 / frei, nur Eingang | TX0 / GPIO1 / UART |
| 4 | D34 / GPIO34 / frei, nur Eingang | RX0 / GPIO3 / UART |
| 5 | D35 / GPIO35 / frei, nur Eingang | D21 / GPIO21 / I²C SDA (ADS/OLED/MLX) |
| 6 | D32 / GPIO32 / derzeit unbenutzt | D19 / GPIO19 / Phasentaster |
| 7 | D33 / GPIO33 / RGB Rot | D18 / GPIO18 / Pumpen-Wartungstaster (Firmware aktuell deaktiviert) |
| 8 | D25 / GPIO25 / DHT22 1 | D5 / GPIO5 / frei |
| 9 | D26 / GPIO26 / DHT22 2 | TX2 / GPIO17 / RGB Blau |
| 10 | D27 / GPIO27 / DHT22 3 | RX2 / GPIO16 / Buzzer-Signal nur reserviert |
| 11 | D14 / GPIO14 / BH1750 SCL | D4 / GPIO4 / RGB Gruen |
| 12 | D12 / GPIO12 / frei (Boot-Pin beachten) | D2 / GPIO2 / frei (Boot-Pin beachten) |
| 13 | D13 / GPIO13 / BH1750 SDA | D15 / GPIO15 / frei (Boot-Pin beachten) |
| 14 | GND / – / Masse | GND / – / Masse |
| 15 | VIN / – / Versorgung; keine gesicherte 5V-Quelle | 3V3 / – / 3,3 V; Silkscreen ist maßgeblich |

Die farbige Pinlegende des tatsaechlichen Sensorboards bezeichnet unten rechts irrtuemlich VIN: **der PCB-Aufdruck lautet dort 3V3 und ist massgeblich**. GPIO34/35/36/39 sind **ausschliesslich Eingaenge**, nicht fuer Schaltausgaenge einsetzen.

## B. Sensorische Anschluesse nach Baugruppe

| Baugruppe | Leitung(en) | Zugeordnete Sensor-ESP-Pins | Mechanik/weitere Hinweise |
|---|---|---|---|
| OLED integriert | SDA / SCL | GPIO21 (rechts 5) / GPIO22 (rechts 2) | kein weiterer Stecker |
| ADS1115 | SDA / SCL | GPIO21 (rechts 5) / GPIO22 (rechts 2) | VDD=3V3, GND=GND, ADDR=GND, Adresse 0x48 |
| pH PH4502C | PO | ADS1115 A0, **nicht ESP-GPIO direkt** | PO→10 kΩ→ADS A0; ADS A0→20 kΩ→GND, bestehende kalibrierte Verdrahtung; separate bisherige Modulspeisung erhalten; TO/DO frei |
| CQRobot EC/TDS | Analog | ADS1115 A1, **nicht ESP-GPIO direkt** | VCC/Sensorausfuehrung noch nicht verifiziert |
| DS18B20 Wasser | DATA | GPIO23 (rechts 1) | 4,7 kΩ zwischen DATA und 3V3 |
| DHT22 Nr. 1 | DATA | GPIO25 (links 8) | JST-XH-3-Pin: 3V3, DATA, GND |
| DHT22 Nr. 2 | DATA | GPIO26 (links 9) | JST-XH-3-Pin: 3V3, DATA, GND |
| DHT22 Nr. 3 | DATA | GPIO27 (links 10) | JST-XH-3-Pin: 3V3, DATA, GND |
| BH1750 | SDA / SCL | GPIO13 (links 13) / GPIO14 (links 11) | ADDR=GND (0x23), 3V3, GND |
| MLX90640 optional | SDA / SCL | GPIO21 (rechts 5) / GPIO22 (rechts 2) | Versorgung VCC noch NICHT festgelegt |
| RGB Rot | Anode | GPIO33 (links 7) | 2×220 Ω in Reihe |
| RGB Gruen | Anode | GPIO4 (rechts 11) | 2×220 Ω in Reihe |
| RGB Blau | Anode | GPIO17 TX2 (rechts 9) | 2×220 Ω in Reihe; bleibt ohne Pumpen-AUS-Bestaetigung AUS |
| RGB gemeinsam | Kathode | GND (links oder rechts 14) | gemeinsame Kathode |
| Taster Phase | Signal | GPIO19 (rechts 6) | Taster gegen GND; interner INPUT_PULLUP |
| Taster Pumpenwartung | Signal | GPIO18 (rechts 7) | Taster gegen GND; aktuelle Firmware ohne aktive Pumpenabschaltung |
| Buzzer optional | Signalleitung reserviert | GPIO16 RX2 (rechts 10) | 2-Pin-Buzzer-Typ und Treiber ungeklärt, Firmware deaktiviert |

**Versorgungen:** ADS1115/DHT22/BH1750 benutzen 3V3, gemeinsame Signalmasse. PH4502C behält die bestehende externe Sensorversorgung; EC- und MLX-VCC nicht auf Verdacht zuordnen. 3V3 und VIN niemals zusammenschalten.

## C. ESP32-C3 SuperMini – alle 16 Kontakte

**Ansicht: Bauteilseite, USB-C OBEN.** Jeweils 8 Kontakte pro Seite. Diese Darstellung entspricht dem verbreiteten ESP32-C3-SuperMini V1601 16-Pin-Layout, nicht automatisch jedem Nachbau.

| Kontakt von oben | Linke Reihe: Aufdruck / GPIO / Funktion | Rechte Reihe: Aufdruck / GPIO / Funktion |
|---:|---|---|
| 1 | 5V / – / geregelte 5 V Eingang | GPIO5 / 5 / SSR10 Pumpe |
| 2 | GND / – / Steuermasse | GPIO6 / 6 / 12-V-Lampenkuehlung reserviert; externer DC-Treiber noetig |
| 3 | 3V3 / – / 3,3 V interne Versorgung | GPIO7 / 7 / frei |
| 4 | GPIO4 / 4 / SSR10 Heizung | GPIO8 / 8 / BOOT-Strapping/LED; frei lassen |
| 5 | GPIO3 / 3 / SSR10 Umluft | GPIO9 / 9 / BOOT-Strapping/Taster; frei lassen |
| 6 | GPIO2 / 2 / BOOT-Strapping; frei lassen | GPIO10 / 10 / frei |
| 7 | GPIO1 / 1 / SSR10 Abluft | GPIO20 / 20 / UART RX; frei |
| 8 | GPIO0 / 0 / SSR25 LED-Licht | GPIO21 / 21 / UART TX; frei |

**Gemaess aktuellem Projekt-Pinplan vorgesehene 5 SSR-Signale** (noch keine funktionierende C3-Firmware, noch keine gepruefte Steuerendstufe):
 
| Funktion | SSR | C3 GPIO | Board-Pin: Kontakt von oben |
|---|---|---:|---|
| Beleuchtung beider LED-Treiber | SSR25 | 0 | **links 8** |
| Abluft | SSR10 | 1 | **links 7** |
| Umluft | SSR10 | 3 | **links 5** |
| Heizung | SSR10 | 4 | **links 4** |
| Pumpe | SSR10 | 5 | **rechts 1** |
| Lampenkuehlung: 4× 120-mm-PC-Luefter, 12 V | externe DC-Schaltstufe, **kein SSR vorgesehen** | **6 reserviert** | **rechts 2** |

**C3-Versorgung:** 5V **links 1**, GND **links 2** aus passender stabilisierter Kleinspannungsversorgung. Keinesfalls 12V an 5V/3V3/GPIO. Externe USB-Versorgung gleichzeitig mit 5V-Einspeisung nur nach Rueckspeisepruefung. SSR-Eingaenge benoetigen eine zum Typ passende Steuerschnittstelle und geeignete Ausfallsicherung. **GPIO2/8/9 (Boot-Strapping) unbenutzt**, GPIO20/21 UART. C3-GPIO18/19 sind dem USB zugeordnet und nicht in den seitlichen Leisten vorhanden.

**Dimmer:** auf ausdruecklichen Nutzerwunsch aktuell **nicht** einplanen; weder GPIO noch Optokoppler bestuecken. **RTC:** derzeit nicht erforderlich. **4×4-Tastatur/PCF8574:** nicht verdrahten, bis Einbauposition geklaert. **CYD:** eigener WLAN-Touchscreen, keine Steckkabelverbindung zum Sensor-ESP/C3.

**Quellen fuer das physische C3-SuperMini-Pinout:** [Apache NuttX SuperMini V1601](https://nuttx.apache.org/docs/latest/platforms/risc-v/esp32c3/boards/esp32c3-supermini/index.html) und [ESP32-C3 SuperMini 16-pin schematic](https://smartstream-electronics.co.uk/assets/datasheets/ESP32-C3-SuperMini.pdf). Die tatsaechlichen Leiterplattenaufdrucke haben Vorrang.

**Stand/Freigabe:** Sensoranschlussplan projektseitig dokumentiert, aber die Sensor-PCB v0.6 weiterhin nicht DRC-/Produktionsfreigegeben; die C3-GPIO-Funktionen sind nur verplant, nicht physisch geprueft oder geflasht. Keine SSR-Leistungsseite und keine Netzspannungsverdrahtung aus diesem Dokument ableiten.
