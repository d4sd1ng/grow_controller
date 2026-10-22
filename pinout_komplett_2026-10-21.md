# Grow-Controller – eindeutige Pinout-Tabelle (Stand 21.10.2026)

**Geltungsbereich:** zwei getrennte Boards. Sensor-ESP32-OLED fuer Kleinspannungs-Messung/Bedienfeld; ESP32-C3 SuperMini fuer geplante 3,3-V-SSR-Steuersignale in einer gesonderten Leistungsbox. **Keinerlei Netzspannungs-/SSR-Lastverdrahtung auf der Sensorplatine.**

**Nummerierung:** Die erste Spalte zaehlt **Loecher je Seitenreihe VON OBEN**, also unabhaengig von der GPIO-Nummer. Der Nutzer hat Taster und Buzzer dagegen als Kontakte **VON UNTEN** benannt: rechts 6 von unten = GPIO16 (Wuchs/Bluete), rechts 10 von unten = GPIO19 (Wasserpumpentaster), rechts 7 von unten = GPIO17 (RGB Blau), rechts 8 von unten = GPIO5 (Buzzer). *Links* und *rechts* sind Ansichten auf die **Bauteilseite**, **nicht** die Rueckseite. Die aufgedruckte Beschriftung ist beim realen Board zu pruefen.

## A. Sensor-ESP32 mit OLED – alle 30 Kontakte

**Ansicht: OLED sichtbar, USB UNTEN**; jeweils 15 Kontakte pro Seite.

| Kontakt von oben | Linke Reihe: Aufdruck / GPIO / Funktion | Rechte Reihe: Aufdruck / GPIO / Funktion |
|---:|---|---|
| 1 | EN / – / Reset | D23 / GPIO23 / DS18B20-DATA |
| 2 | VP / GPIO36 / frei, nur Eingang | D22 / GPIO22 / I²C SCL (ADS/OLED/MLX) |
| 3 | VN / GPIO39 / frei, nur Eingang | TX0 / GPIO1 / UART |
| 4 | D34 / GPIO34 / frei, nur Eingang | RX0 / GPIO3 / UART |
| 5 | D35 / GPIO35 / frei, nur Eingang | D21 / GPIO21 / I²C SDA (ADS/OLED/MLX) |
| 6 | D32 / GPIO32 / derzeit unbenutzt | D19 / GPIO19 / **Wasserpumpen-Wartungstaster (vom Nutzer an rechts 10 von unten angeschlossen; Firmware noch veraltet)** |
| 7 | D33 / GPIO33 / RGB Rot | D18 / GPIO18 / **frei; alte Pumpentaster-Zuordnung aufgehoben** |
| 8 | D25 / GPIO25 / DHT22 1 | D5 / GPIO5 / **Buzzer-Signal (vom Nutzer rechts 8 von unten; Treiber noch zu prüfen)** |
| 9 | D26 / GPIO26 / DHT22 2 | TX2 / GPIO17 / RGB Blau |
| 10 | D27 / GPIO27 / DHT22 3 | RX2 / GPIO16 / **Wuchs/Blüte-Taster (vom Nutzer rechts 6 von unten; Firmware noch veraltet)** |
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
| Taster Phase | Signal | GPIO16 (rechts 10 von oben / rechts 6 von unten) | Taster gegen GND; interner INPUT_PULLUP vorgesehen; Firmware noch auf altem Pin |
| Taster Pumpenwartung | Signal | GPIO19 (rechts 6 von oben / rechts 10 von unten) | Taster gegen GND; Firmware noch nicht nachgezogen und keine Pumpenabschaltung aktiv |
| Buzzer optional | Signalleitung reserviert | GPIO5 D5 (rechts 8 von oben / unten) | 2-Pin-Buzzer-Typ und Treiber ungeklärt, Firmware deaktiviert |

**Versorgungen:** ADS1115/DHT22/BH1750 benutzen 3V3, gemeinsame Signalmasse. PH4502C behält die bestehende externe Sensorversorgung; EC- und MLX-VCC nicht auf Verdacht zuordnen. 3V3 und VIN niemals zusammenschalten.

## C. ESP32-C3 SuperMini – alle 16 Kontakte + 4 Dosierpumpen

**Ansicht des vom Nutzer gelieferten Fotos: Bauteilseite, USB-C OBEN.** Je 8 Kontakte links/rechts, Position **von oben**. Achtung: Eine fruehere Tabelle hatte die **linke und rechte C3-Pinreihe vertauscht** – korrigiert auf die Aufdrucke des tatsaechlich abgebildeten Boards!

| Pin von oben | Links am Board: GPIO / Aufgabe | Rechts am Board: GPIO / Aufgabe |
|---:|---|---|
| 1 | **D3 / GPIO5** – SSR10 Wasserpumpe | **5V** – geregelte Versorgung |
| 2 | **D4 / GPIO6** – Lampenkuehlung 4× 12-V-Luefter (eigene DC-Treiberstufe) | **GND** – Steuermasse |
| 3 | **D5 / GPIO7** – **SDA Erweiterungs-I²C** (Dosierpumpen + spaetere optionale Tastatur) | **3V3** – Logikversorgung |
| 4 | D8 / GPIO8 – **Boot-/Strappingpin, frei lassen** | **D2 / GPIO4** – SSR10 Heizung |
| 5 | D9 / GPIO9 – **Boot-/Strappingpin, frei lassen** | **D1 / GPIO3** – SSR10 Umluft |
| 6 | **D10 / GPIO10** – **SCL Erweiterungs-I²C** | D0 / GPIO2 – **Boot-/Strappingpin, frei lassen** |
| 7 | **D7 / GPIO20** – **DOS_OE Dosierpumpen-Gesamtfreigabe (aktiv LOW; bei Reset Hardware-Sperre)**; UART-RX hierfuer nicht verwenden | **GPIO1** – SSR10 Abluft |
| 8 | D6 / GPIO21 – Reserve / UART-TX | **GPIO0** – SSR25 LED-Stromversorgung |

## D. ESP32-C3 – Baugruppen und feste Signale

| Bauteil / Widerstand / Funktion | Nummer (von oben) | C3-PIN / Ausgang | Anschluss |
|---|---|---|---|
| SSR25 LED gesamt | 8 rechts | GPIO0 | Logiksignal an zu dimensionierende SSR-Ansteuerstufe |
| SSR10 Abluft | 7 rechts | GPIO1 | SSR-Ansteuerstufe |
| SSR10 Umluft | 5 rechts | GPIO3 | SSR-Ansteuerstufe |
| SSR10 Heizung | 4 rechts | GPIO4 | SSR-Ansteuerstufe |
| SSR10 Wasserpumpe | 1 links | GPIO5 | SSR-Ansteuerstufe |
| Vier 12-V-Lampenkuehler | 2 links | GPIO6 | Ein eigener DC-MOSFET-Treiber (NICHT ein sechstes SSR) |
| **PCA9685 SDA (I²C)** | **3 links** | **GPIO7** | I²C SDA, 4,7 kΩ Pull-up nach 3V3 nur einmal auf dem Bus |
| **PCA9685 SCL (I²C)** | **6 links** | **GPIO10** | I²C SCL, 4,7 kΩ Pull-up nach 3V3 nur einmal auf dem Bus |
| **PCA9685 OE** | **7 links** | **GPIO20** | Freigabe **LOW=aktiv, HIGH=gesperrt**; OE mit 10 kΩ nach 3V3, Ausgangszustand AUS |
| **Dosierpumpe 1** | **PCA9685 Kanal 0** | kein weiterer ESP-GPIO | PCA OUT0 → MOSFET-Treiber 1 → 2-polige PUMP1-Klemme |
| **Dosierpumpe 2** | **PCA9685 Kanal 1** | kein weiterer ESP-GPIO | PCA OUT1 → MOSFET-Treiber 2 → 2-polige PUMP2-Klemme |
| **Dosierpumpe 3** | **PCA9685 Kanal 2** | kein weiterer ESP-GPIO | PCA OUT2 → MOSFET-Treiber 3 → 2-polige PUMP3-Klemme |
| **Dosierpumpe 4** | **PCA9685 Kanal 3** | kein weiterer ESP-GPIO | PCA OUT3 → MOSFET-Treiber 4 → 2-polige PUMP4-Klemme |
| PCB-Kleinspannungsversorgung | 1 rechts | 5V | stabilisierte 5 V |
| Steuermasse | 2 rechts | GND | GND Kleinspannung; keine Verbindung zu Netzleitern |
| Logik | 3 rechts | 3V3 | Versorgung I²C-Logik |
| Pumpen-Leistungsversorgung | separater PCB-Stecker | V_PUMP+/GND | **eigener Niedervolt-DC-Eingang**, NICHT an 3V3/5V anschliessen; zulässige Spannung/Strom nach Pumpenauswahl dimensionieren |

**Dosierpumpen sind auf derselben C3-Leistungscontroller-Platine wie SSR-Ansteuerung und Lampenkuehlung zu integrieren – kein Zusatz-Board!** Physisch vorsehen: PCA9685-IC auf dem PCB (I²C-Adresse 0x40), vier Leistungs-MOSFETs, je eine Freilaufdiode passend zum Motor, je ein Gate-Widerstand und Gate-Pulldown, vier eigene 2-polige Pumpenklemmen, gemeinsame externe Pumpen-DC-Versorgung mit passend bemessener Absicherung und 3V3-I²C-Pullups. Ein getrennter Leistungs-DC-Zweig ist zu planen; keine Spannung aus C3-5V/3V3 fuer Motoren ziehen. PWM und Dosiermenge erst nach Pumpendaten bestimmen.

**Sichere Ausgangsruhe:** OE des PCA9685 durch externen 10-kΩ-Pull-up standardmaessig HIGH (=Ausgaenge gesperrt). MOSFET-Gate-Pulldowns machen die Motoren stromlos, solange der C3 nicht initialisiert ist. GPIO20 senkt OE nur bei initialisiertem Controller ab. **Das ersetzt keinen unabhängigen Hardware-Schutz gegen Software-Haenger oder Ueberdosierung**; entsprechende Abschaltung/Timeout vor Inbetriebnahme implementieren.

**I²C-Zweitnutzer:** Fuer die zuvor nur optional geplante 4×4-Tastatur kann spaeter ein PCF8574 auf **demselben C3-I²C-Bus GPIO7/10** bei 0x20 vorgesehen werden; keine zusaetzlichen GPIOs und keine zusaetzliche Adapterplatine noetig. Tastaturanschluss und Montage sind weiterhin nicht beschlossen. I²C-Adressen und Pull-up-Bestueckung vor Montage ueberpruefen.

**Herstellungsstand:** Das ist die feste logische Pin-/Netzzuordnung **fuer die spaetere eine Leistungscontroller-PCB**, aber noch **kein fertig geroutetes/fertigungstaugliches KiCad-Layout**. Endgueltige Leiterbahnbreiten, Stecker-Nennstroeme, MOSFET-Typen, Freilaufdioden, Sicherungen, Temperatur und Bauteilabstaende haengen von den tatsaechlichen Motor-/SSR-Daten ab. SSR-Leistungsseite mit Netzspannung gehoert zur Elektrofachkraft; die C3-Platine enthaelt nur Steuerelektronik und DC-Motortreiber.

**Dimmer:** auf ausdruecklichen Nutzerwunsch aktuell **nicht** einplanen; weder GPIO noch Optokoppler bestuecken. **RTC:** derzeit nicht erforderlich. **CYD:** eigener WLAN-Touchscreen, keine Steckkabelverbindung zum Sensor-ESP/C3.

**Datenblattgrundlagen:** [Espressif ESP32-C3 GPIO](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/peripherals/gpio.html), [NXP PCA9685](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf).

