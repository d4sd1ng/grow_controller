# Grow-Controller – Wiring Plan V2: Anschlüsse und Widerstände
**Stand:** 15.10.2026 · **Gültiger Firmware-Branch:** feature/ph-calibration-service-esp

## 0. Geltungsbereich und aktuelle Bestätigung
- **Sensor-ESP32 mit 0,96-Zoll-OLED** auf der 26×30-Lochrasterplatine: hier werden ausschließlich Kleinspannungs-Sensoren und das kleine Bedienfeld angeschlossen.
- **ESP32-C3 SuperMini** im separaten Leistungsgehäuse: fünf Relaisfunktionen sind geplant, **GPIO-Zuordnung/Relais-Firmware und Netzinstallation noch nicht freigegeben**.
- **Die pH-Kalibrierung funktioniert laut Nutzer. Die bestehende pH-Spannungsteiler-Verdrahtung mit 10 kΩ + 20 kΩ ist beizubehalten.** Die zuvor im Plan notierte Angabe „kein Spannungsteiler“ war falsch.
- Ein Widerstand steht **immer direkt beim betroffenen Modul** in dessen Tabelle. „Keiner“ bedeutet ausdrücklich kein zusätzlicher externer Widerstand an dieser Verbindung; integrierte Modulbauteile sind davon unberührt.
- **Keine 230-V-Anschlussanleitung.** Die Netzspannungsseite und die Schaltlasten sind von einer Elektrofachkraft zu planen und zu prüfen. Nur im spannungsfreien Zustand an der Kleinspannungsverdrahtung arbeiten.

## 0a. Exakte Pinbeschriftung des jetzt gezeigten ESP32-OLED-Boards

**Bildbezug:** Platine von **vorn**, USB-Buchse **unten**, OLED sichtbar. Das Bild zeigt das **Sensor-ESP32-OLED-Board**, **nicht** den separat geplanten ESP32-C3-SuperMini der Leistungsbox. Die farbige Legende und der aufgedruckte Pinname widersprechen sich rechts ganz unten: **Auf der Leiterplatte steht 3V3, in der farbigen Legende fälschlich VIN. Hier gilt 3V3.** Links unten steht tatsächlich VIN. **Diese Pins niemals verwechseln.**

| Position von oben | Links – tatsächlich aufgedruckt | Rechts – tatsächlich aufgedruckt |
|---|---|---|
| 15 | EN | **D23** (GPIO23) |
| 14 | VP (GPIO36) | **D22** (GPIO22) |
| 13 | VN (GPIO39) | TX0 (GPIO1) |
| 12 | D34 (GPIO34) | RX0 (GPIO3) |
| 11 | D35 (GPIO35) | **D21** (GPIO21) |
| 10 | D32 (GPIO32; **nicht verwenden für RGB**) | **D19** (GPIO19; Phasentaster) |
| 9 | **D33** (GPIO33; LED Rot) | **D18** (GPIO18; **ist herausgeführt**) |
| 8 | **D25** (GPIO25; DHT22-1) | D5 (GPIO5) |
| 7 | **D26** (GPIO26; DHT22-2) | TX2 (GPIO17) |
| 6 | **D27** (GPIO27; DHT22-3) | **RX2 (GPIO16; Buzzer-Signal)** |
| 5 | **D14** (GPIO14; BH1750 SCL) | **D4** (GPIO4; LED Grün) |
| 4 | D12 (GPIO12) | D2 (GPIO2) |
| 3 | **D13** (GPIO13; BH1750 SDA) | D15 (GPIO15) |
| 2 | GND | GND |
| 1 (unten) | **VIN** | **3V3** – Leiterplattenaufdruck; **nicht VIN!** |

**Keine GPIO6-Buchse** auf diesem Board. Das Foto liefert **keine** verlässliche Pinbelegung für den separaten ESP32-C3 in der Leistungsbox. **Pumpentaster und fünf Relais dort nicht anschließen, bis das konkrete Leistungsboard identifiziert ist.**

**Fürs Löten an der Kleinspannungs-Sensorplatine:** Pinbezeichnungen aus dieser Tabelle benutzen und die Widerstände jeweils aus den unten stehenden **Bauteiltabellen** übernehmen. Nicht auf Grundlage der früheren generischen Pinzeichnungen oder der falschen VIN-Legende arbeiten.

## 1. ADS1115 + PH4502C-pH-Sensor – BESTEHENDEN Spannungsteiler beibehalten
Der ADS1115 hängt mit dem OLED am I²C-Bus **D21/D22**, Adresse **0x48**. Seine Versorgung ist **3,3 V**, nicht 5 V. pH wird an **A0**, EC an **A1** gemessen.

| Von | Über Widerstand | Nach | Hinweis |
|---|---|---|---|
| ESP32 **3V3** | keiner | ADS1115 **VDD** | ADS-Versorgung 3,3 V |
| ESP32 **GND** | keiner | ADS1115 **GND** | Gemeinsame Signalmasse |
| ESP32 **D21 (GPIO21, SDA)** | keiner in Reihe | ADS1115 **SDA** | Gemeinsamer I²C-Bus mit internem OLED |
| ESP32 **D22 (GPIO22, SCL)** | keiner in Reihe | ADS1115 **SCL** | Gemeinsamer I²C-Bus mit internem OLED |
| ADS1115 **ADDR** | keiner | **GND** | Geräteadresse 0x48 |
| **PH4502C PO** | **10 kΩ in Reihe** | **ADS1115 A0** | **OBERER** Widerstand des Spannungsteilers, bereits festgelegt/verbaut |
| **ADS1115 A0** | **20 kΩ** | **GND** | **UNTERER** Widerstand des Spannungsteilers, vom Nutzer zuvor als eingesetzt bestätigt |
| **CQRobot EC/TDS-Analogausgang** | **kein 10k/20k-Teiler** | **ADS1115 A1** | Nur verwenden, sofern die Analogspannung im ADS-Versorgungsbereich liegt |
| ADS1115 **A2**, **A3**, **ALRT/RDY** | keiner | **offen** | Nicht gebraucht |

**Exakter pH-Spannungsteiler:** PH4502C PO → 10 kΩ → KNOTEN A0 → 20 kΩ → GND. Vom **KNOTEN A0** geht **ohne weiteren Widerstand** die Messleitung zum Pin **ADS A0**. PO und ADS müssen die gleiche Masse besitzen. Der ADC sieht nominell **2/3 der PO-Spannung**: z. B. 3,0 V PO → 2,0 V an A0. Die Software-Kalibrierpunkte beziehen sich **direkt auf die gemessene ADS-A0-Spannung**, deshalb den Teiler nicht entfernen und in der Firmware nicht noch einmal pauschal mit 1,5 multiplizieren.

**Spannungsgrenze:** Bei ADS-VDD = 3,3 V ist der spezifizierte **Betriebsbereich** der Analogeingänge **GND bis VDD**. Der 10k/20k-Teiler würde bei **5,0 V an PO theoretisch 3,33 V an A0** ergeben – knapp **oberhalb** dieses Betriebsbereichs. Dass der Teiler derzeit funktioniert, ist kein Freibrief für alle denkbaren PO-Spannungen; den maximal tatsächlich an A0 auftretenden Wert vor verändertem Betrieb prüfen. **Nicht ohne neue Messung die Widerstände tauschen**, denn dadurch würden die gespeicherten pH-Kalibrierpunkte nicht mehr passen.

**I²C-Pull-ups nur prüfen, nicht blind doppelt einbauen:** Falls der gemeinsame Bus D21/D22 durch die vorhandenen OLED-/ADS-Breakouts **noch keine ausreichenden Pull-ups** besitzt, kommt jeweils **ein 4,7-kΩ-Widerstand D21 → 3V3 und D22 → 3V3**. Beide liegen **zwischen Datenleitung und 3V3**, niemals in Reihe zu SDA/SCL. Vorhandene Pull-ups mitberücksichtigen; deren Bestückung am konkreten Breakout ist noch zu überprüfen. Ein Abblockkondensator von 100 nF VDD–GND ist im TI-Datenblatt vorgesehen; prüfen, ob er bereits auf dem ADS-Breakout bestückt ist.

### PH4502C separat
| PH4502C-Pin | Verbindung | Zusätzlicher Widerstand |
|---|---|---|
| **PO** | → **10 kΩ** → ADS **A0**; von A0 **20 kΩ → GND** | **10 kΩ + 20 kΩ wie oben** |
| **GND** | → gemeinsame Masse mit ESP32 und ADS | keiner |
| **VCC** | → **bestehende, funktionierende Sensorversorgung nicht verändern** (zuvor ca. 4,7 V gemessen) | keiner neu vorgesehen |
| **TO** | **nicht anschließen** | keiner |
| **DO** | **nicht anschließen** | keiner |

### CQRobot EC/TDS separat
| Modulanschluss | Verbindung | Zusätzlicher Widerstand |
|---|---|---|
| **Analoger Signalausgang** | → ADS **A1** | **kein 10k/20k-Spannungsteiler**; zulässige Maximalspannung vor Änderungen prüfen |
| **GND** | → gemeinsame Masse | keiner |
| **Versorgung** | → entsprechend Aufdruck/Datenblatt der tatsächlichen CQRobot-Modulvariante | **keine unbestätigte Versorgungsspannung oder Beschaltung erfinden** |

EC wird in der Firmware mit der tatsächlichen DS18B20-Wassertemperatur kompensiert. An A1 keine pH-Kalibrierwerte verwenden.

## 2. DS18B20 – ein Wassertemperatursensor
| Von | Über Widerstand | Nach |
|---|---|---|
| ESP32 **3V3** | keiner | DS18B20 **VDD** |
| ESP32 **GND** | keiner | DS18B20 **GND** |
| DS18B20 **DATA** | keiner in Reihe | ESP32 **D23 / GPIO23** |
| **D23 / DATA** | **4,7 kΩ (Pull-up)** | **ESP32 3V3** |

Der **4,7-kΩ-Widerstand liegt zwischen DATA und 3V3**, nicht zwischen DATA und dem ESP32-Pin. Genau **ein** Pull-up auf diesem 1-Wire-Bus.

## 3. Drei DHT22 – Widerstand je Sensor direkt an DATA
| Sensor | Versorgung | Datenverbindung | Pull-up direkt am Sensor-DATA | Masse |
|---|---|---|---|---|
| **DHT22 Nr. 1** | 3V3 | **DATA → D25 / GPIO25** | **4,7 kΩ DATA → 3V3**, **nur wenn nicht auf Modul vorhanden** | GND |
| **DHT22 Nr. 2** | 3V3 | **DATA → D26 / GPIO26** | **4,7 kΩ DATA → 3V3**, **nur wenn nicht auf Modul vorhanden** | GND |
| **DHT22 Nr. 3** | 3V3 | **DATA → D27 / GPIO27** | **4,7 kΩ DATA → 3V3**, **nur wenn nicht auf Modul vorhanden** | GND |

Keine 4,7 kΩ in Reihe zum Datenpin. Bei Breakout-Modulen mit integriertem Pull-up **keinen weiteren** Widerstand ohne Prüfung hinzufügen.

## 4. BH1750 – separater I²C-Bus, noch nicht montiert
| BH1750-Pin | ESP32 | Externer Widerstand |
|---|---|---|
| **VCC** | **3V3**, soweit konkrete Modulvariante 3,3 V unterstützt | keiner |
| **GND** | **GND** | keiner |
| **SDA** | **D13 / GPIO13** | ggf. **4,7 kΩ D13 → 3V3** nur wenn Bus-Pull-up fehlt |
| **SCL** | **D14 / GPIO14** | ggf. **4,7 kΩ D14 → 3V3** nur wenn Bus-Pull-up fehlt |
| **ADDR** | **GND** für 0x23 | keiner |

Die Pull-ups liegen **parallel zum Bus**, nicht in Reihe. Vorhandene Widerstände auf dem BH1750-Breakout zuerst prüfen.

## 5. RGB-LED – zwei Vorwiderstandsketten
| Von | Widerstandskette | Nach |
|---|---|---|
| ESP32 **D4 / GPIO4** | **220 Ω → 220 Ω in Reihe = 440 Ω** | **RGB Grün (Anode)** |
| ESP32 **D33 / GPIO33** | **220 Ω → 220 Ω in Reihe = 440 Ω** | **RGB Rot (Anode)** |
| **Gemeinsame Kathode** | keiner | **GND** |
| **RGB Blau** | keiner | **nicht angeschlossen** |

D32 wird **nicht mehr** für RGB benutzt, weil dort laut Nutzer kein Signal ankam. **Grün = WUCHS; Rot + Grün = Gelb = BLÜTE.** D4 ist in beiden aktuellen Sketches eingetragen; Hardwarefunktion erst nach dem Umstecken und Flashen bestätigen.

## 6. Taster 1 (Phasenwechsel) und Buzzer
| Bauteil | Verbindung | Externer Widerstand |
|---|---|---|
| **Phasentaster / Schließer** | Sensor-ESP32 **D19 / GPIO19 → Taster → GND** | **kein externer Widerstand**, Firmware verwendet **INPUT_PULLUP**; GPIO19-Impulse werden jetzt per Interrupt erfasst |
| **Buzzer-Steuersignal** | ESP32 **D16 / GPIO16** | kein Widerstand/Treiber pauschal festgelegt; **Buzzer deaktiviert** bis Bauart, Strom und Versorgung geprüft sind |

**Fehlersuche am D19-Taster:** Nach Flashen der aktualisierten Firmware meldet der Serial-Monitor bei 115200 Baud beim Start den Ruhezustand **HIGH (offen)** und bei Tastendruck die Flankenmeldung **TASTER D19 ...** sowie **PHASE**. Im Kalibriersketch liefert außerdem **`t`** den augenblicklichen Status **HIGH (offen) / LOW (gedrückt)**. Wird der Eingang beim Drücken nicht LOW, sind Steckkontakt, GND-Verbindung und die zwei tatsächlich schaltenden Kontakte des vierpoligen Tasters zu prüfen (nicht zwei intern dauerverbundene Kontakte verwenden). Nicht andere Sensorpins umlegen. Einen unbekannten zweipoligen Buzzer nicht direkt als GPIO-Last behandeln.

## 7. MLX90640 – vorbereitet, nicht montiert
| Modulanschluss | Verbindung | Widerstand |
|---|---|---|
| **SDA** | ESP32 **D21**, gemeinsam mit ADS1115 und OLED | Kein zusätzlicher Pull-up, solange Busbestückung ungeprüft |
| **SCL** | ESP32 **D22**, gemeinsam mit ADS1115 und OLED | Kein zusätzlicher Pull-up, solange Busbestückung ungeprüft |
| **GND** | Gemeinsame Masse | keiner |
| **VCC/VIN** | **Noch nicht festlegen** – Versorgung der tatsächlichen Breakout-Variante prüfen | keiner festgelegt |

MLX90640 ist in der Firmware deaktiviert. Geplante I²C-Adresse 0x33. I²C-Bus nicht ohne Prüfung mit 5-V-Pull-ups betreiben.

## 8. Separate Leistungsbox – fünf Funktionskanäle, KEINE 230-V-Verdrahtung
| Kanal | Funktion | ESP32-C3-GPIO |
|---|---|---|
| 1 | **Beide Lampen gemeinsam** | offen |
| 2 | **Abluft** | offen |
| 3 | **Umluft** | offen |
| 4 | **Heizstrahler** | offen |
| 5 | **Filter-/Umwälzpumpe (geplant)** | offen |

### Zweiter Taster – Pumpenabschaltung, eigener Eingang am ESP32-C3

| Vom C3 | Widerstand | Zum Taster | Gegenstelle |
|---|---|---|---|
| **Leistungscontroller: PIN OFFEN** | **Widerstand/Pull-up OFFEN – Board und Eingang noch nicht geprüft** | **Tasterkontakt 1: nicht anschließen** | **Tasterkontakt 2: nicht anschließen** |

**Funktion:** 1× drücken = Pumpe AUS/Wartungssperre, erneut drücken = Pumpe EIN/normaler Betrieb freigegeben. Entprellung in der noch zu entwickelnden C3-Firmware; automatische Pumpensteuerung darf eine aktive Wartungssperre nicht überstimmen. Für den Taster **keine Versorgung 3V3/5V auf einen Kontakt** und keine Verbindung zum Sensor-ESP32-D19 vorsehen. Bei einem vierbeinigen Taster müssen die zwei gewählten Pins **nur während des Drückens leitend sein**; die zwei intern dauerhaft verbundenen Pins sind dafür ungeeignet.

**Wichtig zum Stand:** Die vorige Zuordnung auf **GPIO6 war unbestätigt und wurde zurückgenommen**, weil dieser Anschluss am tatsächlichen Board laut Nutzer nicht vorhanden ist. **Keinen Pin anschließen, bis die reale Beschriftung des Leistungsteil-ESP32-C3 geprüft ist.** Der Pumpenkanal und dieser Taster sind derzeit nur geplant; eine C3-Firmware dafür ist nicht implementiert. Keine pH-/EC-Sensor-Firmware auf den C3 flashen.

**Wartungssicherheit:** Die elektronische Pumpenpause ersetzt **keine sichere elektrische Trennung** vor Eingriffen an Filter oder Pumpe. Die Netzversorgung muss für die Reinigung fachgerecht getrennt und gegen ungewollten Wiederanlauf gesichert werden.

Die **4×4-Matrix-Tastatur** bleibt lediglich für später vorgemerkt (keine Befestigung vorhanden). PCF8574 und Tastenpins sind **noch nicht** zu verdrahten. Die geplante C3-WLAN-Anbindung, Relaislogik und Pumpensperre sind noch nicht implementiert oder getestet.

## 9. Widerstands-Einkauf-/Bestückungsübersicht
| Bauteil | Widerstände unmittelbar am Bauteil | Status |
|---|---|---|
| **ADS A0 / PH4502C PO** | **1 × 10 kΩ PO→A0 und 1 × 20 kΩ A0→GND** | **Vorhandene Verdrahtung beibehalten** |
| **ADS A1 / CQRobot EC** | **kein zusätzlicher Teiler** | Analogpegel prüfen |
| **DS18B20** | **1 × 4,7 kΩ DATA(D23)→3V3** | vorgesehen/bereits bestätigt |
| **RGB Grün** | **2 × 220 Ω in Reihe, D4→LED** | Firmware auf D4, physisch prüfen |
| **RGB Rot** | **2 × 220 Ω in Reihe, D33→LED** | vorgesehen |
| **DHT22 Nr. 1/2/3** | ggf. **je 1 × 4,7 kΩ DATA→3V3** | nur bei fehlendem Modul-Pull-up |
| **ADS/OLED-I²C** | ggf. **1 × 4,7 kΩ D21→3V3 + 1 × 4,7 kΩ D22→3V3** | nur wenn Pull-ups fehlen |
| **BH1750-I²C** | ggf. **1 × 4,7 kΩ D13→3V3 + 1 × 4,7 kΩ D14→3V3** | nur wenn Pull-ups fehlen |
| **Taster 1: Phasenwechsel am Sensor-ESP32** | **kein externer Widerstand** (interner Pull-up) | **D19 → Taster → GND** |
| **Taster 2: Pumpenpause am Leistungscontroller** | **Widerstand/Pull-up OFFEN bis Boardprüfung** | **Beide Kontakte NOCH NICHT VERDRAHTEN; PIN OFFEN, Firmware fehlt** |
| **Buzzer / geplante C3-Relais** | **Widerstände/Treiber erst nach Prüfung der konkreten Module festlegen** | nicht erraten |

**Wichtig:** Der frühere Widerspruch „ADS A0 ohne Spannungsteiler“ ist hiermit korrigiert. Die bestehende **10-kΩ-/20-kΩ-Teilerverdrahtung** und die auf ihr beruhende pH-Kalibrierung sind maßgeblich. Nicht ohne Messungen verändern.
