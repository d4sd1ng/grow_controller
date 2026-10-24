# Current State — Grow Controller

**Stand:** 2026-10-15  
**Arbeitsbranch:** `feature/ph-calibration-service-esp`  
**Architektur:** **zwei ESP32**: (1) Sensor-ESP32 mit OLED auf **26×30-Lochraster**, (2) **ESP32-C3 SuperMini** als separater Leistungscontroller.

## Sensorplatine 26 × 30 – aktuelle Pinbelegung

| Sensorpin | Zweck |
|---|---|
| GPIO21/22 | I²C Bus A: ADS1115 (0x48), integriertes OLED (0x3C) + MLX90640 (0x33; noch nicht angeschlossen) |
| GPIO13/14 | I²C Bus B: nur BH1750 (0x23, ADDR an GND), unabhängig vom ADS1115 |
| ADS1115 A0 | **PH4502C PO → 10 kΩ (in Reihe) → ADS A0; ADS A0 → 20 kΩ → GND**. Bestehenden Spannungsteiler der kalibrierten pH-Messkette beibehalten. |
| ADS1115 A1 | CQRobot EC/TDS-Analogsignal **ohne den pH-10k/20k-Teiler**; zulässige Analogspannung prüfen |
| **GPIO23** | **1× DS18B20-Wassertemperatur**, 1-Wire DATA, **4,7-kΩ-Pull-up nach 3V3** |
| GPIO25/26/27 | 3× DHT22 (Pflanzenzone, Zuluft, Abluft/oben) |
| **GPIO16 / RX2** | **WUCHS/BLUETE-Taster**; rechts 6 von unten laut Nutzer, gegen GND; Firmware noch auf GPIO19 und anzupassen |
| **GPIO4 (D4)** | RGB-Grün über **440 Ω (2×220 Ω in Reihe)**; ersetzt ausgefallenen GPIO32 |
| GPIO33 | RGB-Rot über **440 Ω (2×220 Ω in Reihe)** |
| **GPIO5 / D5** | **Buzzer**, rechts 8 von unten; genaue Beschaltung offen, in Firmware noch nicht korrigiert |
| **GPIO17 (TX2)** | RGB-Blau über **440 Ω (2×220 Ω in Reihe)**; rechts 7 von unten; nur bei bestaetigter Pumpenabschaltung, bislang AUS |
| **GPIO19 / D19** | **Wasserpumpen-Wartungstaster**, rechts 10 von unten laut Nutzer, gegen GND; Firmware noch nicht angepasst; derzeit keine tatsaechliche Abschaltung |
| **GPIO18 (D18)** | **frei**, aeltere Pumpentasterplanung hiermit aufgehoben |

**Änderung gegenüber V7:** DS18B20 wieder von GPIO32 auf **GPIO23** zurückverlegt, um den Pull-up mechanisch wie gewünscht anzuschließen. RGB-Grün war zuletzt auf **GPIO32** geplant, dort kommt laut Hardwaretest kein Signal an; deshalb nun auf **GPIO4 (D4)** ohne doppelte Pinbelegung. DS18B20-Funktion wurde zuvor vom Nutzer bei GPIO32 bestätigt; auf D23 nach dem Umverdrahten erneut prüfen.

**RGB-LED:** Gemeinsame Kathode laut Nutzer → GND, Blau ist neu auf TX2/GPIO17 vorgesehen und bleibt mangels bestaetigter C3-Pumpenabschaltung AUS. Grün = WUCHS; Rot + Grün = Gelb (BLÜTE). Bei bestaetigter Filterwartung nur BLAU (Rot/Gruen aus). Firmware verwendet `GROW_RGB_COMMON_ANODE=0` und aktiviert RGB mit `GROW_ENABLE_PHASE_RGB=1`. **LED-/pH-Abweichung nach Firmwarewechsel ist weiterhin zu klaeren; keine weitere Umverdrahtung auf Verdacht.** Ein Phasentaster D19 (bereits programmiert) und ein separater Pumpentaster D18 (noch nicht programmiert); Phasenzustand im NVS `growphase_v1`.

**Nicht verdrahten:** PH4502C TO/DO; keine SSR- oder Netzspannungsverkabelung auf der Sensorplatine. Interner OLED auf GPIO21/22. Kamera-Versorgung noch nicht freigegeben. Weitere Details: [wiring_plan_v2.md](wiring_plan_v2.md).

## pH-Alarm und 12-Wochen-Nährstoffprofil

**pH-Alarm:** LOW <5,5; HIGH >6,2; 5,5–6,2 inklusive im Regelbereich (3 Messungen zur Bestätigung, 0,1 pH Hysterese bei Zustandswechseln). Die bisherigen 6,5-Grenzen sind im Sensor- und CYD-Code durch 6,2 ersetzt.

**EC bedeutet elektrische Leitfähigkeit (mS/cm), nicht ppm.** Die neuen Nutzer-Sollwerte sind:

| Woche | pH-Soll | EC-Soll (mS/cm) |
|---|---:|---:|
| 1 | 6,0 | 1,6 |
| 2 | 5,8 | 1,8 |
| 3–7 | 5,5 | 2,0 |
| 8 | 5,5 | 2,5 |
| 9–10 | 5,5 | 1,8 |
| 11–12 | 5,5 | 0,2 |

Die Wochenzahl ist im separaten ESP32-NVS `growweek_v1` manuell wählbar (`k1` bis `k12` über seriellen Monitor oder API `POST /api/local/control/week` mit JSON `{"week":3}`); kein Datum zur automatischen Wochenfortschaltung festgelegt. `0` heißt unbekannt. Die Wochenziele sind **nicht** als Dosierung oder EC-Alarm implementiert: EC-Toleranzen und automatische Regelung sind offen. Phasentaster WUCHS/BLÜTE unabhängig davon.

## Zweites Gerät — ESP32-C3 SuperMini

Separate Installation im Leistungsgehäuse fuer **1× SSR25 (LED-Stromversorgung) und 4× SSR10 (Abluft/Umluft/Heizung/Wasserpumpe)**. Die vier 12-V-Lampenkuehler erhalten eine eigene MOSFET-Ansteuerung. Die Dimmung ist auf Nutzerwunsch zurueckgestellt. **Mechanische Klarstellung:** Die sieben Peristaltikpumpen werden separat nahe den bodennahen Vorratsbehältern montiert; auf der einen C3-Leistungscontroller-Platine sitzen nur die sieben DC-Motortreiber mit Schutzbauteilen, deren Versorgung und sieben elektrische Pumpenklemmen sowie die restliche Steuerung. Die vom Nutzer genannten ca. 50 cm Ansaughöhe sind modellabhängig und betreffen die Höhendifferenz Flüssigkeitsspiegel → Pumpeneinlass. Keine Pumpen und keine Flüssigkeitsschläuche im Steuergehäuse einplanen. Separate Pumpenhalterung/Gehäuse erforderlich; Elektronikplatinengröße separat bemessen. **NEU (22.10.2026): Genau dieselbe EINE C3-Leistungscontroller-PCB soll **sieben** spaetere Mikro-/Mini-Dosierpumpen (**5 × Dünger, 1 × pH−, 1 × pH+**) vollstaendig als Bestueckungsplaetze fuer **sieben** Treiberstufen und **sieben** 2-polige Pumpenklemmen enthalten; KEINE zusaetzliche Dosierpumpen-PCB.** Steuerung ueber **PCA9685 (Adresse 0x40) an C3 GPIO7/SDA (links 3 von oben), GPIO10/SCL (links 6)**; **PCA OUT0–OUT4** steuern **fuenf Dünger-Pumpen**, **OUT5 pH−**, **OUT6 pH+**; insgesamt **sieben** getrennte MOSFET-Ausgaenge. pH+ und pH− gegenseitig verriegeln, bei Mess- oder Kommunikationsfehler sperren. **GPIO20 (links 7)** steuert PCA-OE mit externem Pull-up nach 3V3 (Reset = Ausgang gesperrt). MOSFET-Gate-Pulldowns, passende Freilaufdioden und ein separater abgesicherter DC-Pumpeneingang sind im endgueltigen PCB vorzusehen. Versorgungsspannung/Motoranlaufstroeme nicht erraten, bis Pumpenmodelle feststehen. **Andere C3-GPIOs unveraendert:** SSR GPIO0/1/3/4/5, Lampenkuehlung GPIO6; GPIO2/8/9 Boot-unbelegt; GPIO21 Reserve/UART. **C3-Pinreihe korrigiert auf Nutzerfoto (USB oben): links GPIO5/6/7/8/9/10/20/21, rechts 5V/GND/3V3/GPIO4/3/2/1/0.** Sensor-Pumpen-Wartungstaster ist gemaess juengster Nutzerkorrektur **GPIO19/D19 (rechts 10 von unten)** statt frueher GPIO18. Die bisherige Sensor-Firmware ist an diese Pinveraenderung noch nicht angepasst; keine C3-SSR-/Dosierpumpen-Firmware implementiert, keine elektrische Leistungsfreigabe und kein fertigungstaugliches geroutetes C3-KiCad-PCB vorhanden. Pumpenwartung muss elektrisch sicher gegen Wiederanlauf freigeschaltet werden, Dosierdauer benoetigt Limit/Watchdog und Kalibrierung.

### Zentrales Bedienfeld (neu geplant)

Die vorhandene **4×4-Matrix-Tastatur (0–9, A–D, *, #)** ist **nur als spätere Bedienoption vorgemerkt**, weil derzeit keine Befestigungsmöglichkeit besteht. Nach geeigneter Montage soll sie **sämtliche Funktionen** bedienen: fünf Relais/Aktoren, Klima/VPD, pH/EC, DS18B20, Woche/Phase, Sollwerte, Zeitpläne, Alarme und geführte Kalibrierung. Tastenvorschlag: **1–5 Aktoren, 6 Klima, 7 pH, 8 EC, 9 Wasser/Temperatur, 0 Phase/Woche, A Pumpen-Wartung AUS/EIN je Tastendruck, B Automatik/Manuell, C Status, D Einstellungen, * Zurück, # Bestätigen**. Kein direkter Heizungs-/Licht-Schaltbefehl allein durch eine Ziffer.

Ein **PCF8574-I²C-Portexpander am ESP32-C3** ist vorgesehen, damit eine passive 4×4-Matrix nicht acht direkte GPIOs verbraucht. Der C3-I²C-Pinplan und die acht Matrixleitungen sind **noch nicht geprüft**, ebenso fehlen Protokoll, Firmware und Anzeigeabgleich mit **CYD** und Sensor-OLED. Der separat geplante Pumpen-Drucktaster **bleibt unverändert vorgesehen**; Taste A ist lediglich eine mögliche spätere Zweitbedienung. **Bis eine Halterung gefunden ist: Tastatur nicht verdrahten, keine Pins reservieren und keine neue Tastatur-Firmware implementieren.** Für die tatsächliche Filterreinigung ist eine sichere elektrische Trennung unabhängig von der Tastatur nötig.

## Firmware

- `firmware/ph_calibration_tool`: Kalibrierung/Diagnose/pH-Alarmtests, **keine Relais-GPIO-Ausgänge**.
- `firmware/growroom_telemetry_client`: Messwerte, pH-Alarm, OLED, HTTP-API und Sollwertverwaltung; **alte direkte Relais-GPIO-Schaltung entfernt**. Aktorstatus ist `esp32_c3_not_connected`. WLAN-Verbindung zum C3 und tatsächliche Umsetzung von Steuerbefehlen stehen aus.
- `firmware/ds18b20_diagnostic`: 1-Wire-Test GPIO23 nach Umverdrahtung.
- `firmware/powerbox_actuator`: alte Firmware, **nicht** als fertige C3-Version verwenden.
- CYD und Raspberry Pi kommunizieren bislang weiterhin mit dem Sensor-ESP32.

## Verifizierter Stand

- PH4502C PO pH-4-/pH-7-Kalibrierung laut Nutzer erfolgt.
- pH-Alarmtests LOW / HIGH / NORMAL laut Nutzer funktionsfähig.
- PH4502C TO nicht verwendet. Genau ein DS18B20 laut Nutzer erfolgreich erkannt (bislang GPIO32), Rückverlegung auf GPIO23 nun vereinbart.
- RGB-LED gemeinsame Kathode geklärt, ein Taster und je zwei 220-Ω-Widerstände pro R/G/B-Kanal vorgesehen. Buzzer-Typ und Versorgung noch nicht ermittelt.
- Die aktuell geänderten Sketche wurden hier **nicht** mit einer realen Arduino-ESP32-Toolchain kompiliert und nicht erneut auf Hardware getestet.

## Nächste Aufgaben

1. Lochrasterplatine 26×30 anhand echter Modulmaße trocken bestücken, USB/OLED und Sensoranschlüsse ausrichten.
2. Buchsenleisten, 3V3/GND-Verteilung und Sensorsteckverbinder auf der **Sensorplatine** löten; Durchgang und Versorgung prüfen.
3. DS18B20 auf GPIO23 mit Pull-up 4,7 kΩ nach 3V3 prüfen; bei einem Sensor ist keine ROM-Zuordnung nötig.
4. DHT22/BH1750 prüfen und die MLX90640 über den auf D21/D22 reservierten Anschluss mit der tatsächlichen Breakout-Versorgung verbinden, sobald ihr Datenblatt vorliegt.
5. Getrennten C3-Controller softwareseitig entwickeln, kommunikative und elektrische Sicherheitsprüfung vor jeder Leistungsfreigabe.
6. PostgreSQL-Migration von bisherigen Serverdaten separat durchführen.


## BH1750 Lichtmessung auf der Kulturflaeche (1,00 × 1,00 m)

Die aktuelle Sensor-Firmware berechnet aus dem gemessenen **Lux (lx)** des BH1750 den **geschaetzten einfallenden Lichtstrom (~lm)** auf einer festen Kulturflaeche von **1,00 m²**: `geschaetzte_lm = Lux × 1,00 m²`. Die Pflanzengroesse aendert die konfigurierte Beetflaeche nicht. **Achtung:** Ein einzelner BH1750 misst lokal, nicht die raeumliche Mittelbeleuchtungsstaerke. Die Lumenangabe ist deshalb eine grobe Naeherung fuer das Beet, **kein vom BH1750 direkt gemessener Wert und keine Lampen-Gesamtleistung**. Keine Ableitung von PPFD oder PAR.

Der OLED zeigt Lux und `~lm` im Wechsel mit einer bestehenden Informationszeile (alle 5 Sekunden), ohne die pH-/EC-Werte oder den Alarm zu ueberdecken. Der Kalibriersketch zeigt beide auch im seriellen Hardwarestatus. Der Produktionssketch bewahrt `light_level` (Lux) und ergaenzt in API und Telemetrie `illuminance_lux`, `estimated_incident_lumens`, `light_area_m2` und `lumens_estimated`. Bei nicht erkanntem BH1750 sind die neuen Messwerte `null`. **Neue Firmware ist noch nicht auf dem ESP32 kompiliert, geflasht oder getestet. Den laufenden LED-/pH-Fehler zuerst abklaeren.**


### Leistungscontroller-PCB – mechanisches Konstruktionsmaß (24.10.2026)

**Sieben elektrische Dosierpumpen-Anschlüsse vollständig vorbereiten; zunächst nur sechs externe Pumpen tatsächlich installieren.** Welcher Kanal unbestückt bleibt, wurde nicht festgelegt; die Controller-Platine erhält dennoch alle sieben gleichwertigen Anschlüsse. Die Pumpenkörper werden separat montiert, nicht auf der Platine.

Als **mechanisches Entwurfsmaß** ist nach einer ersten rechteckigen Platzierungsstudie **180 × 110 mm** vorgesehen, mit **vier geplanten Ø3,2-mm-Montagebohrungen** auf **168 × 98 mm** Mittenabstand. Die Zeichnung berücksichtigt sieben 2-polige Pumpenklemmen an der unteren Platinenkante, fünf SSR-Steueranschlüsse oben, vier Lüfteranschlüsse rechts und getrennte Kleinspannungs-Einspeisungen. Anschlussgehäuse wurden in der Studie mit 13 × 12 mm reserviert. Rechteckige Baugruppen-Umrisse überschneiden sich nicht, aber **echte Footprints, Leiterbahnführung, thermische Abstände, Strombelastbarkeit, Kabelzugang und KiCad-DRC sind noch nicht überprüft**. Das Maß ist deshalb **keine Fertigungs-/Gehäuse-Bestellfreigabe**. Das Gesamtgehäuse muss die externen SSR-Leistungselemente und deren Installation gesondert aufnehmen.
