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

| Tabellenreihe (15 = oben, 1 = unten) | Links – tatsächlich aufgedruckt | Rechts – tatsächlich aufgedruckt |
|---|---|---|
| 15 | EN | **D23** (GPIO23) |
| 14 | VP (GPIO36) | **D22** (GPIO22) |
| 13 | VN (GPIO39) | TX0 (GPIO1) |
| 12 | D34 (GPIO34) | RX0 (GPIO3) |
| 11 | D35 (GPIO35) | **D21** (GPIO21) |
| 10 | D32 (GPIO32; **nicht verwenden für RGB**) | **D19** (GPIO19; Phasentaster) |
| 9 | **D33** (GPIO33; LED Rot) | **D18** (GPIO18; Pumpentaster 2) |
| 8 | **D25** (GPIO25; DHT22-1) | D5 (GPIO5) |
| 7 | **D26** (GPIO26; DHT22-2) | **TX2 (GPIO17; RGB Blau fuer bestaetigte Pumpen-Wartung)** |
| 6 | **D27** (GPIO27; DHT22-3) | **RX2 (GPIO16; Buzzer-Signal)** |
| 5 | **D14** (GPIO14; BH1750 SCL) | **D4** (GPIO4; LED Grün) |
| 4 | D12 (GPIO12) | D2 (GPIO2) |
| 3 | **D13** (GPIO13; BH1750 SDA) | D15 (GPIO15) |
| 2 | GND | GND |
| 1 (unten) | **VIN** | **3V3** – Leiterplattenaufdruck; **nicht VIN!** |

**Pumpentaster 2 wird ausdrücklich am hier abgebildeten Sensor-ESP32 mit OLED angeschlossen (D18, rechte Seite, 7. Pin von oben (Tabellenposition 9 vom unteren Ende)).** Die fünf Relais verbleiben im separaten Leistungsgehäuse; das Foto liefert keine Pinbelegung für dessen Controller.

**Fürs Löten an der Kleinspannungs-Sensorplatine:** Pinbezeichnungen aus dieser Tabelle benutzen und die Widerstände jeweils aus den unten stehenden **Bauteiltabellen** übernehmen. Nicht auf Grundlage der früheren generischen Pinzeichnungen oder der falschen VIN-Legende arbeiten.

## 0b. GND-, 3V3- und Sensorversorgungsverteilung vor dem Loeten

**ESP32-OLED-Board (USB unten, Frontansicht):** Unten links **VIN**, links unmittelbar darueber **GND**; unten rechts **3V3**, rechts unmittelbar darueber **GND**. Es gibt damit physisch **2 GND-Pins, 1× 3V3 und 1× VIN**, die ueber gemeinsame Versorgungsschienen verteilt werden. **VIN nicht mit 3V3 verbinden und nicht ungeprueft als belastbaren 5-V-Ausgang betrachten.** Internes OLED braucht keinen zusaetzlichen externen Versorgungsanschluss.

Die Zahlen sind **Abzweige/Kontaktstellen auf der Sensor-Lochrasterplatine**, keine Anzahl erforderlicher ESP32-GND-Pins. Beide ESP32-GND-Kontakte gehoeren zur gleichen Signalmasse.

| Sensor/Komponente | GND-Abzweige | 3V3-Abzweige | Versorgung getrennt von 3V3 |
|---|---:|---:|---|
| ADS1115: GND und ADDR (0x48), VDD | 2 | 1 | – |
| pH-Spannungsteiler 20 kOhm A0 nach GND | 1 | 0 | – |
| PH4502C: GND und bestehende VCC | 1 | 0 | 1 (vorhandene, funktionierende Versorgung) |
| CQRobot EC/TDS: GND und Versorgung | 1 | 0 | **Spannung und Zielschiene offen; Modulvariante pruefen** |
| DS18B20: GND, VDD und 4,7-kOhm-Pull-up DATA nach 3V3 | 1 | 2 | – |
| DHT22 Nummer 1, 2 und 3: je GND und VCC | 3 | 3 | – |
| BH1750: GND und ADDR nach GND, VCC nach 3V3 | 2 | 1 | – |
| RGB-LED: gemeinsame Kathode GND, Gruen/Rot/Blau je ueber eigene Serienvorwiderstaende an GPIO | 1 | 0 | – |
| Taster 1 D19 und Taster 2 D18: jeweils nach GND | 2 | 0 | – |
| **Gesamt, mit geplantem BH1750 und D18-Taster** | **14** | **7** | **1 fest; EC offen** |

**Zusaetzliche optionale 3V3-Abzweige nur wenn Pull-ups auf den Modulen fehlen:** drei DHT22-DATA-Pull-ups (3), ADS/OLED-I2C D21 und D22 (2), BH1750-I2C D13 und D14 (2). Im Extremfall weitere **7× 3V3**, also **bis zu 14 Anschlusspunkte insgesamt**. Pull-ups niemals blind doppelt bestuecken.

**Praktische Verteilervorbereitung auf dem 26×30-Lochraster:** GND-Schiene mit **mindestens 14 Abzweigstellen**, besser **16–18** inklusive Reserve; 3V3-Schiene mit **7 festen**, besser **mindestens 14** Kontaktstellen, falls externe Pull-ups noetig werden. Fuer die vorhandene PH4502C-Versorgung **1 Anschluss** beibehalten und **1–2 Reserveplaetze** vorsehen, aber **noch kein zweites Modul auf Verdacht an VIN/5 V anschliessen**. Wenn die CQRobot-Versorgung nach Variantenpruefung 3V3 statt 5 V erfordert, muss dieser Anschluss auf die 3V3-Schiene umgeplant werden.

**Nicht in obiger Summe:** optionaler MLX90640, Buzzer und dessen Treiber, spaetere Module sowie der **separate ESP32-C3 im Leistungsgehaeuse**. Dessen Masse-/3V3-/Relaisversorgung ist gesondert zu dimensionieren. PH4502C PO/ADS A0: bestehende Verdrahtung beibehalten und vorhandene Spannung nicht allein aus dem dokumentierten Teiler ableiten.

## 0c. Tatsächlich gemessene Modulmaße für das Sensor-PCB (17.10.2026)

**Die PCB V0.4 ist noch NICHT bestellfertig.** Diese Werte stammen aus Messungen des Nutzers und ersetzen bisherige Platzhalter. Maße gelten nicht für beliebige andere Modulvarianten.

| Modul/Bauteil | Maß in mm | Bedeutung / Einbaulage |
|---|---|---|
| ESP32-OLED-Modul | **53 × 23** | Mitte Befestigungsbohrung bis Mitte Befestigungsbohrung (L × B) |
| ESP32-Stift-/Buchsenleisten | **27** | gemessener Querabstand der zwei Leisten (genauen Bezug auf die Pin-Mitten vor finalem Footprint bestätigen) |
| ADS1115-Modul | **29 × 17** | gemessene Länge × Breite, **nicht** als Lochmittenabstand bestätigt |
| PH4502C | **33 × 23** | Mitte Befestigungsbohrung bis Mitte Befestigungsbohrung; **abgewinkelte Anschlussstifte, Modul steht hochkant** |
| CQRobot EC-Modul | **23 × 37** | Mitte Befestigungsbohrung bis Mitte Befestigungsbohrung; Achsrichtung vor endgültiger Montage prüfen |
| Taster 1 und 2 | **je 10 × 10** | Nutzermaß am 17.10.2026 bestätigt; Pinraster und Kontaktanordnung müssen vor Auswahl des Einlöt-Footprints geprüft werden |
| Buzzer | **Ø ca. 10** | Schätzwert, **zweipoliger Buzzer, Treiber/Spannung weiter ungeklärt** |

**WICHTIG für den PCB-Aufbau:** Der ESP32-Querabstand ist jetzt mit 27 mm statt des früheren Generator-Platzhalters 25,4 mm angegeben. Die **Montagebohrungen der aufgesteckten Module dürfen nicht einfach an den Abmessungsrechtecken platziert werden**: Ihre Lage relativ zu den tatsächlichen Pinreihen muss erst aufgenommen werden. Bei **hochkant stehendem PH4502C** ist statt des flachen Modulumrisses vor allem der rechtwinklige Stecker, dessen Rastermaß, die genaue Pinfolge und der nötige Gehäusefreiraum entscheidend. Die 33 × 23 mm Lochmitten sind für eine ggf. separate Befestigung der pH-Platine hilfreich, aber nicht automatisch Leiterplatten-Bohrungen im Träger.

**RTC-Entscheidung:** aktuell **kein RTC erforderlich/zu bestücken**. Der Raspberry Pi kann Datums-/Uhrzeitstempel setzen; die derzeitige Sensor-Firmware hat weder eine RTC-Anbindung noch autonome zeitgebundene Last-Zeitpläne. Nur für späteren eigenständigen Uhrzeitbetrieb (z. B. Lichtplan bei WLAN-/Serverausfall) RTC/DS3231 erneut bewerten. Ein eventuell vorhandenes RTC-Modul besitzt laut Nutzer ebenfalls abgewinkelte Pins und würde hochkant stehen; **kein Footprint, solange RTC nicht benötigt wird**.

**Noch nicht abschließend vermessen:** Lage/Ø der Modul-Befestigungsbohrungen relativ zu den Anschlussleisten, ADS1115-Pinreihen und genaue Ausrichtung, PH4502C-Pinraster und Gehäusefreiraum, EC-Pinfolge und Versorgung, Buzzer-Treiber/Spannung sowie die noch nicht bestätigten Taster-Pinabstände (Außenmaß jetzt 10 × 10 mm). Für PCB-Fertigung Footprints und KiCad-DRC separat prüfen.

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

## 5. RGB-LED – drei Vorwiderstandsketten
| Von | Widerstandskette | Nach |
|---|---|---|
| ESP32 **D4 / GPIO4** | **220 Ω → 220 Ω in Reihe = 440 Ω** | **RGB Grün (Anode)** |
| ESP32 **D33 / GPIO33** | **220 Ω → 220 Ω in Reihe = 440 Ω** | **RGB Rot (Anode)** |
| **Gemeinsame Kathode** | keiner | **GND** |
| ESP32 **TX2 / GPIO17** | **220 Ω → 220 Ω in Reihe = 440 Ω** | **RGB Blau (Anode), nur bestaetigte Pumpenabschaltung fuer Filterwechsel** |

Bei bestaetigter Pumpenwartung hat **Blau Vorrang**: Rot und Gruen sind AUS, damit die LED wirklich rein blau statt cyan/weiss leuchtet. Der Sensor-ESP32 bekommt derzeit noch **keine bestaetigte Pumpen-Rueckmeldung vom C3**, daher bleibt Blau absichtlich AUS. **Ein Druck auf D18 allein ist kein Nachweis fuer Pumpe AUS.** Beim Filterwechsel muss die Pumpe unabhaengig von der Software sicher elektrisch getrennt sein.

D32 wird **nicht mehr** für RGB benutzt, weil dort laut Nutzer kein Signal ankam. **Grün = WUCHS; Rot + Grün = Gelb = BLÜTE.** D4 ist in beiden aktuellen Sketches eingetragen; Hardwarefunktion erst nach dem Umstecken und Flashen bestätigen.

## 6. BEIDE Taster am ESP32 mit OLED und Buzzer
| Bauteil | Kontakt 1 | Kontakt 2 | Externer Widerstand / Status |
|---|---|---|---|
| **Taster 1 – WUCHS/BLÜTE** | **D19 / GPIO19**, rechts 6. Kontakt von oben | **GND** auf demselben Sensor-ESP32 | **Keiner**, interner INPUT_PULLUP ist in beiden Sketches aktiv |
| **Taster 2 – Pumpe AUS/EIN** | **D18 / GPIO18**, rechts 7. Kontakt von oben | **GND** auf demselben Sensor-ESP32 | **Keiner** bei zukuenftigem INPUT_PULLUP; **aktueller Firmware-Rollback ohne D18-Abfrage** |
| **Buzzer-Steuersignal** | **RX2 / GPIO16**, rechts 6. Kontakt von oben | Elektrische Beschaltung noch zu prüfen | Typ/Treiber noch offen; Buzzer-Firmware deaktiviert |

Die beiden Taster sind **normale, nicht rastende Schließer**. Bei einem 4-Pin-Taster müssen die ausgewählten Kontakte im Ruhezustand getrennt und nur beim Drücken leitend sein. **Keine Tasterverbindung an 3V3 oder VIN.** Bei abgezogenem USB-Kabel verdrahten.

**Taster 1: Diagnose bei weiterhin ausbleibender Funktion:** Nach Flashen des aktuellen Sketches meldet Serial (115200 Baud) beim Druck `TASTER D19 ...`; im Kalibriersketch zeigt `t` HIGH (offen) bzw. LOW (gedrückt). Die Eingabefunktion auf D19 ist bereits programmiert.

**Taster 2: Funktion derzeit NICHT aktiv (Firmware nach LED-/pH-Regression auf Stand vor D18 zurueckgesetzt). Bisherige, voruebergehend zurueckgenommene D18-Implementierung:** Erster Druck = Wartungssperre/Pumpe AUS **angefordert**, nächster Druck = Betrieb **freigegeben**. D18 wird per FALLING-Interrupt mit 50-ms-Entprellung erfasst. Der Anforderungszustand wird im NVS `pumpbtn_v1` gespeichert. **Serial 115200** meldet `PUMPEN-TASTER D18 ...`; im Kalibriersketch zeigt `t` Eingang und Wartungszustand. Bei aktiver Pause zeigt das OLED `P!`. Im Produktionssketch melden `/api/local/summary` und `/api/local/actuators` den Booleschen Wert `pump_maintenance_pause_requested`, mit `pump_output_confirmed=false` – **dies bestätigt ausdrücklich KEINE tatsächliche Pumpenabschaltung**. Der Sensor-ESP32 übermittelt die Anforderung derzeit **nicht** an die Leistungsbox. Die C3-Firmware, der Transport, sichere Abschalt- und Wiederanlaufverriegelung sowie Rückmeldungen fehlen. Das Anlöten bewirkt noch kein Abschalten. Für Filterreinigung ist eine sichere elektrische Trennung gegen Wiederanlauf erforderlich.

## 7. MLX90640 – vorbereitet, nicht montiert
| Modulanschluss | Verbindung | Widerstand |
|---|---|---|
| **SDA** | ESP32 **D21**, gemeinsam mit ADS1115 und OLED | Kein zusätzlicher Pull-up, solange Busbestückung ungeprüft |
| **SCL** | ESP32 **D22**, gemeinsam mit ADS1115 und OLED | Kein zusätzlicher Pull-up, solange Busbestückung ungeprüft |
| **GND** | Gemeinsame Masse | keiner |
| **VCC/VIN** | **Noch nicht festlegen** – Versorgung der tatsächlichen Breakout-Variante prüfen | keiner festgelegt |

MLX90640 ist in der Firmware deaktiviert. Geplante I²C-Adresse 0x33. I²C-Bus nicht ohne Prüfung mit 5-V-Pull-ups betreiben.

## 8. Separate Leistungsbox – 1 × SSR25 + 4 × SSR10, KEINE 230-V-Verdrahtung

**Nutzerbestaetigter SSR-Bestand (18.10.2026):** Es sind **exakt fuenf SSR** vorgesehen: **1 × SSR25 fuer LED-Beleuchtung** und **4 × SSR10 fuer Abluft, Umluft, Heizung und Pumpe**. Ein zusaetzliches SSR fuer die Lampenkuehlung ist **nicht** vorhanden und darf nicht ohne Rueckfrage in den Plan eingetragen werden.

| SSR | Funktion | ESP32-C3-GPIO |
|---|---|---|
| **SSR25 (1 ×)** | **LED-Beleuchtung (gemeinsame Leistungsschaltung)** | offen |
| **SSR10 (1 ×)** | **Abluft** | offen |
| **SSR10 (1 ×)** | **Umluft** | offen |
| **SSR10 (1 ×)** | **Heizung / Heizstrahler** | offen |
| **SSR10 (1 ×)** | **Pumpe** | offen |

**Zusaetzliche Komponenten, noch kein verbindlicher Steuer-Pinplan:**
- **2 × LED-Dimmer** fuer die LED-Treiber: Steuereingang / Dimmverfahren (z. B. 0–10 V, PWM oder Widerstandsschnittstelle) anhand der tatsaechlichen Treiberpruefen. ESP32-GPIO **nicht direkt** mit unbekanntem Dimmeingang verbinden.
- **4 × 120-mm-PC-Luefter, je 12 V**, fuer die Waermeabfuhr der LED-Panels. Nutzer hat ein separates Netzteil mit zwei Anschluessen erwaehnt; genaue DC-Versorgung und die Schaltart der vier Luefter sind **noch offen**. Diese Luefter sind **nicht** einer der vier SSR10-Funktionen und **kein sechster SSR-Kanal** wurde bestaetigt.
- **LED-Panel-/Treiberkonfiguration (Nutzer bestaetigt):** **4 LED-Panels gesamt**, aufgeteilt auf **2 LED-Treiber mit je 2 in Reihe geschalteten Panels**. **Beide Treiber werden gemeinsam ueber das vorhandene SSR25 versorgt.** Jeder Treiber bekommt einen eigenen der beiden Dimmer. Die konkrete elektrische Dimmerschnittstelle sowie SSR-/Treiber-Typ und Belastbarkeit vor der Leistungsverdrahtung anhand der Datenblaetter pruefen.
- Die Leistungsschaltung liegt im **separaten Leistungsgehaeuse**, **nicht auf der Sensor-Kleinspannungs-PCB**. Netzspannung, SSR-Typ (AC/DC-Eignung), Kuehlung und Absicherung von einer Elektrofachkraft pruefen lassen. Der C3-Pinplan und seine Firmware sind weiterhin nicht freigegeben.

**Pumpentaster-Zuordnung:** Ausschließlich **Sensor-ESP32 mit OLED, D18 → Taster 2 → GND**; Details samt Widerstand direkt in Abschnitt 6. **Keine Pumpentaster-Verbindung am separaten Leistungscontroller.** Die D18-Tastenerfassung ist im aktuellen Firmwarestand voruebergehend deaktiviert; sie steuert keine Pumpe.

Die **4×4-Matrix-Tastatur** bleibt lediglich für später vorgemerkt (keine Befestigung vorhanden). PCF8574 und Tastenpins sind **noch nicht** zu verdrahten. Die geplante C3-WLAN-Anbindung, Relaislogik und Pumpensperre sind noch nicht implementiert oder getestet.

## 9. Widerstands-Einkauf-/Bestückungsübersicht
| Bauteil | Widerstände unmittelbar am Bauteil | Status |
|---|---|---|
| **ADS A0 / PH4502C PO** | **1 × 10 kΩ PO→A0 und 1 × 20 kΩ A0→GND** | **Vorhandene Verdrahtung beibehalten** |
| **ADS A1 / CQRobot EC** | **kein zusätzlicher Teiler** | Analogpegel prüfen |
| **DS18B20** | **1 × 4,7 kΩ DATA(D23)→3V3** | vorgesehen/bereits bestätigt |
| **RGB Grün** | **2 × 220 Ω in Reihe, D4→LED** | Firmware auf D4, physisch prüfen |
| **RGB Rot** | **2 × 220 Ω in Reihe, D33→LED** | vorgesehen |
| **RGB Blau** | **2 × 220 Ω in Reihe, TX2/GPIO17→LED** | PCB V0.6 vorbereitet; Firmware bleibt ohne C3-Rueckmeldung AUS |
| **DHT22 Nr. 1/2/3** | ggf. **je 1 × 4,7 kΩ DATA→3V3** | nur bei fehlendem Modul-Pull-up |
| **ADS/OLED-I²C** | ggf. **1 × 4,7 kΩ D21→3V3 + 1 × 4,7 kΩ D22→3V3** | nur wenn Pull-ups fehlen |
| **BH1750-I²C** | ggf. **1 × 4,7 kΩ D13→3V3 + 1 × 4,7 kΩ D14→3V3** | nur wenn Pull-ups fehlen |
| **Taster 1: Phasenwechsel am Sensor-ESP32** | **kein externer Widerstand** (interner Pull-up) | **D19 → Taster → GND** |
| **Taster 2: Pumpenpause am OLED-Sensor-ESP32** | **kein externer Widerstand bei zukuenftigem INPUT_PULLUP** | **D18 → Taster → GND**, rechte Seite 7. Pin von oben (Tabellenposition 9); aktuelle Firmware nach Rollback noch ohne D18-Funktion |
| **Buzzer / geplante C3-Relais** | **Widerstände/Treiber erst nach Prüfung der konkreten Module festlegen** | nicht erraten |

**Wichtig:** Der frühere Widerspruch „ADS A0 ohne Spannungsteiler“ ist hiermit korrigiert. Die bestehende **10-kΩ-/20-kΩ-Teilerverdrahtung** und die auf ihr beruhende pH-Kalibrierung sind maßgeblich. Nicht ohne Messungen verändern.


## BH1750 Lichtmessung auf der Kulturflaeche (1,00 × 1,00 m)

Die aktuelle Sensor-Firmware berechnet aus dem gemessenen **Lux (lx)** des BH1750 den **geschaetzten einfallenden Lichtstrom (~lm)** auf einer festen Kulturflaeche von **1,00 m²**: `geschaetzte_lm = Lux × 1,00 m²`. Die Pflanzengroesse aendert die konfigurierte Beetflaeche nicht. **Achtung:** Ein einzelner BH1750 misst lokal, nicht die raeumliche Mittelbeleuchtungsstaerke. Die Lumenangabe ist deshalb eine grobe Naeherung fuer das Beet, **kein vom BH1750 direkt gemessener Wert und keine Lampen-Gesamtleistung**. Keine Ableitung von PPFD oder PAR.

Der OLED zeigt Lux und `~lm` im Wechsel mit einer bestehenden Informationszeile (alle 5 Sekunden), ohne die pH-/EC-Werte oder den Alarm zu ueberdecken. Der Kalibriersketch zeigt beide auch im seriellen Hardwarestatus. Der Produktionssketch bewahrt `light_level` (Lux) und ergaenzt in API und Telemetrie `illuminance_lux`, `estimated_incident_lumens`, `light_area_m2` und `lumens_estimated`. Bei nicht erkanntem BH1750 sind die neuen Messwerte `null`. **Neue Firmware ist noch nicht auf dem ESP32 kompiliert, geflasht oder getestet. Den laufenden LED-/pH-Fehler zuerst abklaeren.**
