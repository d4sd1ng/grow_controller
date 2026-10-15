# Current State — Grow Controller

**Stand:** 2026-10-15  
**Arbeitsbranch:** `feature/ph-calibration-service-esp`  
**Architektur:** **zwei ESP32**: (1) Sensor-ESP32 mit OLED auf **26×30-Lochraster**, (2) **ESP32-C3 SuperMini** als separater Leistungscontroller.

## Sensorplatine 26 × 30 – aktuelle Pinbelegung

| Sensorpin | Zweck |
|---|---|
| GPIO21/22 | I²C Bus A: ADS1115 (0x48), integriertes OLED (0x3C) + MLX90640 (0x33; noch nicht angeschlossen) |
| GPIO13/14 | I²C Bus B: nur BH1750 (0x23, ADDR an GND), unabhängig vom ADS1115 |
| ADS1115 A0 | PH4502C PO – bestehende kalibrierte pH-Messkette |
| ADS1115 A1 | CQRobot EC/TDS-Analogsignal |
| **GPIO23** | **1× DS18B20-Wassertemperatur**, 1-Wire DATA, **4,7-kΩ-Pull-up nach 3V3** |
| GPIO25/26/27 | 3× DHT22 (Pflanzenzone, Zuluft, Abluft/oben) |
| GPIO16 | Zweipoliger pH-Buzzer: Steuersignal reserviert; Treiber/Spannung unbekannt, Firmware deaktiviert |
| **GPIO4 (D4)** | RGB-Grün über **440 Ω (2×220 Ω in Reihe)**; ersetzt ausgefallenen GPIO32 |
| GPIO33 | RGB-Rot über **440 Ω (2×220 Ω in Reihe)** |
| GPIO19 | **Ein** 4-poliger Schließtaster gegen GND, `INPUT_PULLUP`, Wechsel WUCHS/BLÜTE |

**Änderung gegenüber V7:** DS18B20 wieder von GPIO32 auf **GPIO23** zurückverlegt, um den Pull-up mechanisch wie gewünscht anzuschließen. RGB-Grün war zuletzt auf **GPIO32** geplant, dort kommt laut Hardwaretest kein Signal an; deshalb nun auf **GPIO4 (D4)** ohne doppelte Pinbelegung. DS18B20-Funktion wurde zuvor vom Nutzer bei GPIO32 bestätigt; auf D23 nach dem Umverdrahten erneut prüfen.

**RGB-LED:** Gemeinsame Kathode laut Nutzer → GND, Blau bleibt frei. Grün = WUCHS; Rot + Grün = Gelb (BLÜTE). Firmware verwendet `GROW_RGB_COMMON_ANODE=0` und aktiviert RGB mit `GROW_ENABLE_PHASE_RGB=1`. **DS18B20 D23 und RGB-Grün D4 statt D32 verdrahten, dann den neuen Sketch flashen.** Ein physischer Taster, Phasenzustand im NVS `growphase_v1`.

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

Separate Installation im Leistungsgehäuse für **fünf Relais**: **(1) beide Pflanzenlampen gemeinsam über einen Lichtkanal, (2) Abluft, (3) Umluft, (4) Heizung/Heizstrahler und (5) die neu geplante Filter-/Umwälzpumpe**. Zusätzlich ist ein **normaler, nicht rastender Pumpen-Drucktaster** vorgesehen, unabhängig vom Phasentaster des Sensor-ESP32: **1× drücken = Pumpe AUS (Wartungssperre), erneut drücken = Pumpe EIN bzw. normaler Pumpenbetrieb freigegeben**. Der Zustand muss gegen Übersteuerung durch Automatik/WLAN geschützt sein; Neustart oder Kommunikationsverlust dürfen keinen unbeabsichtigten Pumpenstart auslösen. Dessen C3-Eingang und Montageort sind noch offen. Ein Software-Taster ersetzt keine fachgerecht ausgeführte elektrische Freischaltung für Wartungsarbeiten. **Die C3-Pinbelegung und Steuerungsfirmware sind noch nicht freigegeben; keine Relaisansteuerung funktionsfähig.** Kommunikationsausfall und Rückmeldungen müssen sicher behandelt werden. Keine 230-V-Anschlussanleitung in diesem Repository.

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
- RGB-LED gemeinsame Kathode geklärt, ein Taster und je zwei 220-Ω-Widerstände pro R/G-Kanal vorgesehen. Buzzer-Typ und Versorgung noch nicht ermittelt.
- Die aktuell geänderten Sketche wurden hier **nicht** mit einer realen Arduino-ESP32-Toolchain kompiliert und nicht erneut auf Hardware getestet.

## Nächste Aufgaben

1. Lochrasterplatine 26×30 anhand echter Modulmaße trocken bestücken, USB/OLED und Sensoranschlüsse ausrichten.
2. Buchsenleisten, 3V3/GND-Verteilung und Sensorsteckverbinder auf der **Sensorplatine** löten; Durchgang und Versorgung prüfen.
3. DS18B20 auf GPIO23 mit Pull-up 4,7 kΩ nach 3V3 prüfen; bei einem Sensor ist keine ROM-Zuordnung nötig.
4. DHT22/BH1750 prüfen und die MLX90640 über den auf D21/D22 reservierten Anschluss mit der tatsächlichen Breakout-Versorgung verbinden, sobald ihr Datenblatt vorliegt.
5. Getrennten C3-Controller softwareseitig entwickeln, kommunikative und elektrische Sicherheitsprüfung vor jeder Leistungsfreigabe.
6. PostgreSQL-Migration von bisherigen Serverdaten separat durchführen.
