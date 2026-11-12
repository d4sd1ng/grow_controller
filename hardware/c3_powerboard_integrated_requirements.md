> **AKTUELLE FESTLEGUNG 12.11.2026:** C3 Mini ohne Display; GENAU **6** 12-V-Dosierpumpen à 3 W (18 W / 1,50 A rechnerisch zusammen), **6** MOSFET-Stufen und **6** zweipolige Anschluesse. Kein siebter Reservekanal. Ausgaenge der C3-Firmware bleiben bis zur Hardwarepruefung deaktiviert. Keine automatische Dosierung ohne Zuordnung/Kalibrierung.

# Grow Controller: Eine integrierte C3-Leistungscontroller-PCB – verbindliche Anforderungen (24.10.2026)

## Nutzerentscheidung / funktionales Abnahmekriterium

**Alle sechs zweipoligen DC-Peristaltikpumpen werden unmittelbar mit je genau zwei Motorleitungen an dieselbe Platine angeschlossen.** Zwischen PCB und Motor gibt es **keine separate Pumpentreiberplatine, kein PCA9685-Breakout, kein zusaetzliches Relaismodul und keinen externen MOSFET-Baustein**. Das gilt unter der ausdruecklichen Voraussetzung, dass **zweipolige DC-Buerstenmotor-Pumpen** gewaehlt werden; Schrittmotor- oder mehradrige Motorvarianten erfordern einen anderen Treiber- und Anschlussentwurf.

Diese Platine integriert zugleich die Logik fuer **1 x ESP32-C3 SuperMini**, **5 SSR-Steuerausgaenge** (1 x SSR25 LED, 4 x SSR10 Abluft/Umluft/Heizung/Wasserpumpe), **1 gemeinsamen DC-Lampenkuehlungs-Ausgang fuer vier 12-V-Luefter** und **6 getrennte DC-Dosierpumpen-Motorausgaenge**.

**Achtung:** Auf dieser PCB ausschliesslich Steuer- und Schutzkleinspannung. Die netzseitigen SSR-Lastpfade samt Sicherungen, Kuehlung und Netzverkabelung sind nicht Teil dieser PCB und benoetigen eine fachgerechte gesonderte Installation. Die SSR-Module selbst sind nicht mit den Pumpen-MOSFETs gleichzusetzen.

## Elektrische Funktionsarchitektur (ein PCB)

| Komponente auf PCB | Anzahl | Zweck |
|---|---:|---|
| ESP32-C3 SuperMini mit bestellbarem/modulspezifischem Footprint | 1 | C3-Hauptsteuerung |
| PCA9685PW, direkt auf PCB, **kein Breakout** | 1 | I²C-Steuerung 6 Pumpenausgaenge |
| Logic-Level-MOSFET mit passendem thermischen Design, direkt auf PCB | 6 | Ein DC-Leistungsausgang je Pumpenmotor |
| Gate-Widerstaende + Gate-Pulldowns | 7 + 7 | Treiberansteuerung und definierter Ruhepegel |
| Motor-Freilauf-/Stoerschutz | 6 | Induktive Motorabschaltung, Auslegung nach Pumpendaten |
| Pumpenanschluss 2-polig, steck-/schraubbar | **6** | PUMP1–PUMP6: nur Motor+ und Motor− |
| Ueberstromschutz je Motorzweig (Footprint) | **6** | Bei blockiertem oder kurzgeschlossenem Motor |
| Zentrale DC-Motorstrom-Einspeisung mit Verpolschutz, Hauptsicherung, TVS und Stuetzelkos | 1 | Alle sechs DC-Motorkanäle versorgen |
| Stromversorgungsregelung fuer 5 V C3-Logik, direkt auf PCB | 1 | Kein separates DC/DC-Breakout; Auslegung nach tatsaechlichem Eingang |
| PCA9685 3V3-Abblockkondensator, SDA/SCL-Pullups, OE-10-kOhm-Pullup | je nach Schaltplan | Kein separates PCA-Modul |
| SSR-Steuerstufen mit Ausgangsklemmen | 5 | Nur Signalseite der fuenf vorhandenen SSR |
| 12-V-DC-Luefter-MOSFET-Stufe samt Schutzbeschaltung | 1 | Gemeinsame Abschaltung von vier Lueftern |
| Luefter-Anschlussklemmen | 4 | FAN1–FAN4 |
| Befestigungsbohrungen, eindeutige Leiterplattenaufdrucke PUMP1–6, SSR1–5, FAN1–4 | nach Platzierung | Montage und wartungsarme Verdrahtung |

**Dosiervariante (12.11.2026): GENAU sechs Pumpen PUMP1–PUMP6, PCA9685 OUT0–OUT5, keine Reserve.** Die konkrete Zuordnung zu Duenger/pH+/- ist noch offen; automatische Dosierung bleibt gesperrt. PCA-OE ist beim Reset HIGH und sperrt die Pumpen.

**GPIO-Zuordnung unveraendert:** PCA9685 SDA GPIO7, SCL GPIO10, OE GPIO20 (OE HIGH=OFF via 10 kOhm Pull-up), SSR-Ausgaenge GPIO0/1/3/4/5, Lampenkuehlung GPIO6, GPIO2/8/9 frei wegen Boot-Strapping. PCA OUT0–OUT5 schalten die sechs DC-Motortreiber.

## Mechanik und Gehaeuse

- **Veraltete Platzierungsstudien:** Die alte 230×135-mm-KiCad-Vorlage und die spaetere 180×110-mm-Skizze beruecksichtigten 7 Pumpen. Beide sind nicht zur Fertigung geeignet; die Sechskanal-PCB muss neu platziert, geroutet und geprueft werden.
- Bestehende vier M3-Lochpositionen sind als Platzhalter (8,8), (222,8), (8,127), (222,127) mm (Koordinaten relativ zur linken oberen PCB-Ecke). **Erst nach Routing/Mechanik freigeben.**
- In der V01 fehlen insbesondere Footprints der 7 Einzelzweig-Schutzschaltungen, Hauptsicherung/Verpolschutz/TVS und der onboard geregelten Logikversorgung. Die **endgueltige PCB-Abmessung muss nach deren Einbau und echten Anschlussabmessungen** neu bestimmt werden; nicht allein aus 230 x 135 mm ein finales Gehaeuse bestellen.
- Das neue Gehaeuse muss ausser der PCB ggf. alle **5 vorhandenen SSR-Baugruppen**, Kuehlung, Versorgungskomponenten, Kabeldurchfuehrungen und zugaengliche Kleinspannungs-Stecker aufnehmen. **Daher ist die Gesamt-Gehäusegröße nicht aus der PCB-Größe allein abzuleiten.**
- Pumpenkoerper sind **separat montiert**, nur ihre 2-poligen Motorkabel gehen zum PCB.

## Konkrete externe Beschaffung

- **Sechs** bauartgleiche oder kompatible **zweipolige DC-Peristaltikpumpen** mit dokumentierter Nennspannung, Nenn-/Anlauf-/Blockierstrom, Motorpolung, Steckertyp und max. Betriebsdauer.
- Passende Motorkabel samt Gegensteckern fuer **6 PCB-Motorausgaenge**.
- Geeignete getrennte/abgesicherte **Schutzkleinspannungsversorgung** passend zu den Motoren und dem Gesamtstrom; bei gleicher 12-V-Betriebsspannung kann ein gemeinsames auf PCB verwaltetes 12-V-DC-Versorgungskonzept fuer Pumpen und vorhandene 12-V-Luefter nach Belastungspruefung ausgelegt werden.
- Mechanische Pumpenhalterungen und Gehaeuse sowie geeignetes Kabelmanagement. Die Motoren brauchen keine eigenen Treiber- oder PCA-Platinen.

## Bestellung/Fertigungsfreigabe

Fuer die **endgueltige BOM, Leiterbahnquerschnitte, Leistungsschutz, DC/DC-Wandler, Stecker, Gehaeuse- und PCB-Maße** muss mindestens **ein konkreter Pumpentyp mit Datenblatt bzw. Betriebsspannung und maximalem blockiertem Motorstrom** feststehen. Fuer SSR-Steuerstufen sind deren Eingangs-Nennspannungen/-stroeme erforderlich. Kein Betrieb mit netzseitigen Lasten ohne fachgerechte Pruefung.

**Stand:** Die hier festgelegte Integration ist eine **verbindliche Konstruktionsanforderung**, nicht der Nachweis, dass eine fertig geroutete, DRC-freie und herstellbare Platine bereits vorliegt. Vor Freigabe sind Schaltplan, konkreter BOM, PCB-Routing, DRC/ERC, Testmuster und Gehaeusepassung notwendig.
