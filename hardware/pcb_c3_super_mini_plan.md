# Grow-Controller — C3-Steuerplatine, Planungsstand 15.11.2026

**Status:** Planungsgrundlage, **kein** freigegebener Schaltplan, Layout oder Fertigungsdatensatz.

## Festgelegte Ausstattung

- ESP32-C3 Super Mini, ESP32-C3FH4, 4 MB Flash, Board-Nennabmessung 23 × 18 mm.
- Ein PCA9685 als Erweiterungsbaustein.
- Sechs reservierte Dosierpumpenkanäle für Pumpen mit 12 V DC und 3 W angegebener Leistungsaufnahme je Pumpe.
- Zwei separate Lüftergruppen am PCA9685.
- Fünf SSR-Steuersignale: Licht, Abluft, Umluft, Heizung, Wasserpumpe.
- Dosierkanal 1 = Plagron pH Min; Kanäle 2–6 noch nicht fest zugeordnet.
- Kein lokales Display am Sensor-ESP32; LVGL-Remote ist separates Gerät.
- Ausgänge für chemische Förderung in der Firmware weiterhin gesperrt.

## Aktuelle Firmware-Pinbelegung (ESP32-C3)

| Signal | C3-GPIO | Bemerkung |
| --- | ---: | --- |
| SSR Licht | 0 | SSR-Steuerseite, keine Netzlast auf dieser Platine |
| SSR Abluft | 1 | dito |
| SSR Umluft | 3 | dito |
| SSR Heizung | 4 | dito |
| SSR Wasserpumpe | 5 | dito |
| PCA9685 SDA | 7 | I²C in Firmware zugeordnet |
| PCA9685 SCL | 10 | I²C in Firmware zugeordnet |
| PCA9685 /OE | 20 | globales Ausgangssperrsignal |

Die Zuordnung entspricht dem Firmwarestand und dem vom Nutzer gezeigten
**Pinout-Bild als Orientierung**. Sie ist noch keine elektrische oder mechanische
Verifikation des gelieferten Boards. GPIO20 ist auch als UART0-RX nutzbar;
Konflikte mit Programmierung/Debugging sind vor Fertigung zu pruefen.

## Aktuelle PCA9685-Zuordnung

| PCA9685-Kanal | Funktion | Stand |
| --- | --- | --- |
| OUT0 | Pumpe 1 — Plagron pH Min | Ausgang gesperrt |
| OUT1 | Pumpe 2 | Ausgang gesperrt; Produkt offen |
| OUT2 | Pumpe 3 | Ausgang gesperrt; Produkt offen |
| OUT3 | Pumpe 4 | Ausgang gesperrt; Produkt offen |
| OUT4 | Pumpe 5 | Ausgang gesperrt; Produkt offen |
| OUT5 | Pumpe 6 | Ausgang gesperrt; Produkt offen |
| OUT6 | Lüftergruppe 1 | Treiber-/Signalanforderungen prüfen |
| OUT7 | Lüftergruppe 2 | Treiber-/Signalanforderungen prüfen |
| OUT8–OUT15 | Reserve | Nicht belegt |

**Wichtig:** PCA9685-Ausgänge sind nur Steuersignale, keine Motorversorgung.
Eine sicherheitsgeprüfte Leistungstreiberstufe ist separat erforderlich.

## Versorgung und Mechanik — noch nicht freigegeben

Rechnerische nominale Pumpenlast: 3 W / 12 V = 0,25 A pro Pumpe;
sechs Pumpen zusammen nominal 18 W und 1,5 A, falls alle gleichzeitig
laufen. Das sind **keine** gemessenen Anlauf-/Blockierströme.
Netzteil, Sicherung, Schutzbeschaltung, thermische Auslegung und
Leiterbahnbelastbarkeit werden daraus noch nicht endgültig abgeleitet.

Offene Fertigungsangaben:

1. Verifizierter Footprint des gelieferten ESP32-C3 Super Mini
   (Padmitten, Raster, Lochdurchmesser, Toleranzen und USB-Zugänglichkeit).
2. Verifizierte Versorgungspfade für 12 V, 5 V und 3,3 V,
   einschließlich regulatorischer Verluste, Rückspeisung und Masseführung.
3. Mess- bzw. Datenblattangaben zu Anlauf-/Blockierströmen, Motorpolung
   und Schutzanforderungen der gekauften Pumpen.
4. Konkrete geprüfte Motortreiber, Schutz- und Freigabeschaltungen;
   konzentrierte Säureförderung nicht freigeben.
5. Gegenstück, Raster, Kontaktstrom und Zugentlastung aller Steckverbinder.
6. SSR-Eingangsstrom und dessen elektrische Verträglichkeit zur C3-Ausgangsstufe.
7. Elektrische Anforderungen der zwei Lüftergruppen.
8. Gehäuseinnenmaße, Montageabstände, Kabelführungen, Befestigungsbohrungen.
9. Ausreichende Trennung von Kleinspannung und jeglicher Netzlast;
   230-V-Lastseite gehört nicht zu diesem ungeprüften PCB-Konzept.
10. Erfolgreiche Kompilierung, Niederspannungs-Prüfaufbau und elektrische
    Verifikation vor jeder Fertigungsfreigabe.

## Vorgehen

1. Footprint und Gehäuse-Schnittstellen anhand Originalteil prüfen.
2. Komponenten- und Anschlussstückliste konsolidieren; vorläufige
   Platzierung auf einem mechanischen Übersichtsplan erstellen.
3. Getrennte elektrische Prüfung der nichtchemischen Steuersignale.
4. Erst nach bestätigter elektrischer Auslegung: vollständigen
   Schaltplan, PCB-Routing und Design-Rule-Check erstellen.

Diese Datei dokumentiert nur die **festgelegte Hardware-Architektur**,
keine bestellfähige Stückliste und keine Anleitung zur Säuredosierung.


## 15.11.2026 — Versorgung, Steckverbinder und PCB-Aufteilung konkretisiert

### A. Versorgungsarchitektur (Funktionsplan, keine freigegebene Schaltung)

Die sechs Pumpen sind als getrennte 12-V-Lastgruppe spezifiziert; die
ESP32-C3- und PCA9685-Logik gehört zu einer getrennten Niederspannungsgruppe.
Für das ESP32-C3 Super Mini ist die 5-V-Einspeisung am vorgesehenen
Versorgungseingang eine mögliche Architektur, **sofern die eingesetzte
Board-Revision dies erlaubt**. Die 3,3 V am PCA9685 müssen unter allen
Betriebszuständen stabil bleiben. Es wird **kein** direkter 12-V-Anschluss
an einen C3- oder PCA9685-Pin vorgesehen.

Die endgültige Stromversorgung erhält eigene Prüfpositionen für:
- gemessenen Lastbedarf von C3, PCA9685, SSR-Eingängen und Lüfterelektronik;
- Eingangs-/Ausgangsspannung, Stromreserve und Wärmeabfuhr des Wandlers;
- Kurzschluss-, Verpolungs- und Überspannungsbehandlung;
- störungsarme Logikversorgung unabhängig von Motor-Anlaufspitzen;
- Massereferenz und Trennung der störanfälligen Mess-/Steuersignale;
- Vermeidung unerwünschter Rückspeisung beim parallelen USB-C-Anschluss.

Vor Messung von Motor-Anlaufströmen wird **keine** verbindliche
Netzteilgröße, Sicherungsgröße oder Leiterbahnbreite behauptet.

### B. Elektrische Anschlussgruppen

| Gruppe | Anzahl | Zweck | Stand |
| --- | ---: | --- | --- |
| PWR-12V | 1 | Einspeisung Pumpenversorgung | Konzept |
| LOGIC-PWR | 1 | Versorgung C3/PCA9685 | Konzept |
| MOTOR-01 … MOTOR-06 | 6 | getrennte Pumpenanschlüsse | nur Anschlussplätze |
| SSR-01 … SSR-05 | 5 | vorhandene Kleinspannungs-Steuersignale | Eingangsanforderungen der SSR offen |
| FAN-01 … FAN-02 | 2 | zwei Lüftergruppen | elektrische Signalart offen |
| SERVICE | 1 | zugänglicher USB-C-/Servicebereich | mechanisch offen |

Diese Gruppen sind eine **Schnittstellenliste**, keine Festlegung auf
Schraubklemmen, JST-Serien oder Leiterquerschnitte. Die Positionen müssen
nach den realen Steckerabmessungen und Strömen ausgelegt werden.

### C. ESP32-C3 Super Mini als Steckmodul

Das generische Super-Mini-Pinout zeigt **2 × 8 Anschlüsse** mit 2,54-mm-
Raster *entlang* der jeweiligen Stiftleiste und den Außenabmessungen
23 × 18 mm. Die **Reihenmittenabstände**, Lötpad-/Bohrungsmaße,
USB-C-Überstände, Modul-Toleranzen und Antennenfreihaltung sind für die
konkrete gelieferte Revision noch zu prüfen. Deshalb wird **noch kein
verbindlicher Leiterplatten-Footprint erzeugt**.

Die dokumentierte GPIO20-Nutzung als PCA-/OE-Signal ist wegen der
UART0-RX-Doppelfunktion auf Boot-/Debug-Konflikte zu prüfen. GPIO2, 8
und 9 sind ESP32-C3-Strapping-Pins und dürfen nicht ohne Prüfung für
externe Signale belastet werden.

### D. Platzierungskonzept (nicht maßstäblich)

- **Servicekante:** ESP32-C3 Super Mini mit von außen erreichbarem USB-C,
  Antennenbereich ohne störende Leiterbahnen oder Abschirmung.
- **Logikbereich:** PCA9685, Logikversorgung und Diagnosepunkte.
- **Anschlusskante 1:** fünf SSR-Steuerschnittstellen und eine
  gemeinsame Lüftertreiber-Steuerschnittstelle.
- **Anschlusskante 2:** sechs mechanisch reservierte Pumpenanschlüsse;
  noch keine aktive Treiberschaltung zur Förderung konzentrierter Säure.
- **Versorgungskante:** separate Einspeisung und später festzulegende
  Schutz-/Regelungskomponenten.

Die endgültige Platinengröße ergibt sich erst aus realen Bauteil-
Footprints, Anschlussrastern und Gehäuseinnenmaßen. Eine gewählte
Platinenbreite ohne diese Daten wäre eine Vermutung.

### E. Entdeckte elektrische Architekturfrage

PCA9685 `/OE` ist ein **gemeinsames** Sperrsignal für sämtliche
PWM-Ausgänge. Da OUT0–OUT5 den Dosierkanälen und OUT6–OUT7 den
Lüftergruppen zugeordnet sind, ist `/OE` **keine unabhängige
Pumpenverriegelung**. Diese Anforderung muss vor Freigabe des
Hardwareentwurfs geprüft und von der übrigen Lüftersteuerung
unabhängig gelöst werden. Die Firmware bleibt derzeit gesperrt.

### F. Referenzdatenblätter

- Espressif ESP32-C3 (GPIO-/Strapping-Einschränkungen):
  https://documentation.espressif.com/esp32-c3_datasheet_en.html
- NXP PCA9685 (Versorgung, globale /OE-Funktion):
  https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf
- Generisches ESP32-C3-Super-Mini-Board:
  https://dl.artronshop.co.th/ESP32-C3%20SuperMini%20datasheet.pdf

**Status:** Funktionsaufteilung und Anschlussgruppen dokumentiert;
MOSFET-Schaltung, reale Bauteil-Footprints, Platinenmaße, DRC und
Fertigungsdateien weiterhin offen.


## 15.11.2026 — Steckverbinder: ausgewählte Baureihe und PCB-Stückzahlen

**Vorgesehene Baureihe:** Phoenix Contact COMBICON MSTB 2,5,
Raster **5,08 mm**, steckbare Schraubklemmen, Leiterplatten-Grundleisten.
Nur **Kleinspannung** auf dieser Platine, keine 230-V-Lastanschlüsse.

Datenblattbestätigte zusammenpassende Serien:
- 2-polige PCB-Grundleiste `MSTBA 2,5/ 2-G-5,08`,
  Hersteller-Nr. **1757242**, und 2-poliger Schraubstecker
  `MSTB 2,5/ 2-ST-5,08`, Hersteller-Nr. **1757019**.
- 3-polige PCB-Grundleiste `MSTBA 2,5/ 3-G-5,08`,
  Hersteller-Nr. **1757255**, und 3-poliger Schraubstecker
  `MSTB 2,5/ 3-ST-5,08`, Hersteller-Nr. **1757022**.

### Anschlussbudget (Steckplätze)

| Beschriftung | Anzahl | Pole je Platz | Verbinderklasse | Status |
| --- | ---: | ---: | --- | --- |
| PWR-12V | 1 | 2 | MSTB 5,08 | 12-V-Niederspannung, Auslegung offen |
| SSR-01 … SSR-05 | 5 | 2 | MSTB 5,08 | Nur Steuersignale; individuelle SSR-Prüfung nötig |
| FAN-CONTROL | 1 | 3 | MSTB 5,08 | Ein gemeinsames Steuersignal für den 12-V-Lüftertreiber; Kontakte noch nicht zugewiesen |
| MOTOR-01 … MOTOR-06 | 6 | 2 | MSTB 5,08 | Mechanische Reservierung; aktive Ansteuerung nicht freigegeben |

**Summe:** 12 zweipolige und 1 dreipoliger Steckplatz
(13 Grundleisten + 13 Gegenstecker). Davon sind sechs
zweipolige Plätze für die Motor-Anschlussgruppe reserviert.
Der Anschluss von Pumpe 1 für konzentriertes pH-Minus erhält
aus dieser reinen Steckverbinderplanung **keine** elektrische
Freigabe oder Ansteuerungsanleitung.

Der **eine** 3-polige Lüfter-Steckplatz ist eine mechanische
Reserve. Laut Nutzer laufen beide Lüfter an **einem 12-V-Treiber
mit zwei Lüfterausgängen** und benötigen nur **ein gemeinsames
Steuersignal**. Die Lüfter selbst werden am Treiber angeschlossen,
nicht als zwei getrennte C3-Steueranschlüsse.
**Weder die Art des Steuersignals (Enable, PWM, Dimmung etc.)
noch die Kontakte des Treibers sind verifiziert.**

### PCB-Stückliste: Steckverbinder als Planungsbasis

| Hersteller-Nr. | Typ | Anzahl |
| --- | --- | ---: |
| 1757242 | MSTBA 2,5/ 2-G-5,08, Grundleiste 2-polig | 12 |
| 1757019 | MSTB 2,5/ 2-ST-5,08, Stecker 2-polig | 12 |
| 1757255 | MSTBA 2,5/ 3-G-5,08, Grundleiste 3-polig | 1 |
| 1757022 | MSTB 2,5/ 3-ST-5,08, Stecker 3-polig | 1 |

Die Steckerserie wurde für Raster, dokumentierte Paarung und
Lösbarkeit ausgewählt. Die Hersteller-Nennstromwerte bedeuten
**keine Freigabe** des Gesamtsystems: Leiter, Leiterbahnen,
Eingangskreise, Absicherung, Belastung und Temperatur müssen
separat geprüft werden.

### Noch zu verifizieren (vor Footprint-/Gehäusefreigabe)

1. Exakte Körper- und Einbaumaße inklusive Einsteckraum sowie
   Leitungsabgang aus **aktuellem** Hersteller-CAD-/Datenblatt.
2. Schaltsignal- und Strombedarf der SSR-Eingänge.
3. Typ und Schnittstelle der Lüfter.
4. Physisches Netzteil-/Logikversorgungs-Konzept, einschließlich
   separater LOGIC-PWR-Quelle. USB-C am C3 ist weiterhin
   als Servicezugang vorgesehen.
5. Passende Kabeleinführungen, Zugentlastung und mechanische
   Verwechslungsgefahr gleichartiger Stecker; eindeutige
   Beschriftungen, ggfs. codierte Alternativen.
6. Eine separate mechanisch/elektrisch geprüfte Lösung
   für die chemische Dosierpumpe, bevor diese betrieben wird.

Quellen:
- https://www.phoenixcontact.com/en-de/products/pcb-connector-mstb-25-3-st-508-1757022
- https://assets.rs-online.com/image/upload/v1698744215/Datasheets/9a177187be001fcb7f378a85a8dbc040.pdf

### Aktualisierung: gemeinsamer 12-V-Lüftertreiber

**Neue verbindliche Anforderung:** Zwei Lüfter werden über
**einen gemeinsamen 12-V-Treiber mit zwei Abgängen** betrieben.
Auf der C3-Steuerplatine ist dafür **ein einziger**
Treiber-Steckplatz `FAN-CONTROL` reserviert, nicht zwei
unabhängige Lüftersteueranschlüsse.

**Softwareabweichung:** Die bestehende `firmware/C3/C3.ino`
kennt noch `fan_group_1` auf PCA9685 OUT6 und
`fan_group_2` auf OUT7 und behandelt sie unabhängig.
Die Firmware ist also **noch nicht** an die neue
Ein-Kanal-Anforderung angeglichen. Die Umstellung darf
erst erfolgen, wenn die tatsächliche Steuerart und
Schnittstelle des 12-V-Treibers feststehen. OUT7 wird
nicht automatisch anders belegt.

Offene Treiberdaten: Modell/Typenschild, Versorgung,
zwei Lastabgänge, Steuereingang samt Pegel-/Strombedarf
und Eingangsbezugspotential. **12-V-Versorgung allein
belegt keinen geeigneten PWM- oder Enable-Eingang.**


### Klarstellung: zweistufige Abluft

Der vorhandene Abluftlüfter hat einen **manuellen Kippschalter**
für Stufe 1 und Stufe 2. Die Stufenauswahl verbleibt am Gerät.
Der Grow-Controller schaltet die Abluft **ausschließlich EIN/AUS**
über den bereits vorgesehenen SSR-Steuerkanal `exhaust_vent`
(C3 GPIO1). Für die Stufenwahl wird weder ein zusätzlicher
GPIO noch ein zusätzlicher PCA9685-Ausgang oder Steckanschluss vorgesehen.

Die tatsächlichen elektrischen Anschluss-/SSR-Anforderungen des
Abluftlüfters sind weiterhin vor Inbetriebnahme zu prüfen.
Dies ist von der getrennten Funktion der zwei anderen Lüfter am
gemeinsamen 12-V-Treiber zu unterscheiden.


### Modellnachtrag: Prima Klima PK125-2

Der verbaute Abluftlüfter ist der **Prima Klima PK125-2**,
ein 230-V-AC-Rohrventilator mit integriertem Zweistufenschalter.
Offizielle Produktangaben (Stand der Prüfung 15.11.2026):

- 230 V, 50/60 Hz, 0,29 A, 63 W;
- 125-mm-Anschluss; Stufe I 220 m³/h, Stufe II 400 m³/h;
- integriertes Anschluss- und Steuerungssystem;
- Herstellervermerk: **„Nicht elektrisch regelbar!“**.

Quelle: https://primaklima.com/de/shop/ventilatoren-de/ac-ventilatoren/pk125-2/

**Gültige Hardwareanforderung:** Abluft wird durch das bereits
vorgesehene SSR nur EIN/AUS geschaltet; die Wahl zwischen Stufe I
und Stufe II erfolgt am vorhandenen manuellen Geräteschalter.
Eine automatische Stufenumschaltung wird **nicht** als
Platinenfunktion vorgesehen, da dafür keine dokumentierte,
herstellerseitig freigegebene Steuerschnittstelle vorliegt.
Keine Änderung an Netzspannungsverdrahtung, Schalter, Motor
oder interner Gerätesteuerung aus dieser Planungsdatei ableiten.
SSR-Typ und Eignung für Motorlast sind noch zu prüfen.


### Betriebszuordnung der manuellen Abluftstufen (15.11.2026)

Nutzervorgabe für Prima Klima PK125-2:

| Controllerphase | Am Lüfter manuell eingestellte Stufe | Hersteller-Luftleistung |
| --- | --- | --- |
| Wachstum (`wuchs`) | Stufe 1 | 220 m³/h |
| Blüte (`bluete`) | Stufe 2 | 400 m³/h |

**Keine automatische elektrische Stufenumschaltung** durch den Controller:
Die bestehende SSR-Schnittstelle schaltet den Ventilator ausschließlich
EIN/AUS. Der Phasenwechsel des Controllers verändert **nicht** die
Schalterstellung am PK125-2. Für eine spätere Benachrichtigung kann
die manuell einzustellende Stufe aus der Phase abgeleitet werden.


### Steckbare ESP32-C3-Super-Mini-Aufnahme

**Verbindliche Konstruktionsanforderung:** Der ESP32-C3 Super Mini wird **steckbar in Buchsenleisten auf der C3-Trägerplatine** montiert, nicht fest verlötet. Damit bleibt das Modul ohne Entlöten austauschbar.

- Modul: ESP32-C3 Super Mini, vom Nutzer angegebene Boardabmessung **23 × 18 mm**.
- Das vorgelegte Pinout zeigt **2 × 8 Anschlusskontakte**. Die passenden Buchsenleisten und der endgültige PCB-Footprint sind anhand des tatsächlich gelieferten Moduls zu prüfen.
- USB-C muss in eingebautem Zustand zum Programmieren und zur Wartung zugänglich bleiben.
- Antennenfreiraum, Bauhöhe, Stecksicherheit und mechanische Toleranzen berücksichtigen.
- Eine steckbare Modulaufnahme ist **kein** weiterer externer Schraubklemmenanschluss; die separat dokumentierten C3-Anschlusszahlen bleiben unverändert.
- Fehlende mechanische Daten (insbesondere Reihenmittenabstand, Pad-/Pin-Geometrie und Steckerfreiraum) werden vor Fertigungsfreigabe vermessen oder per verifiziertem Hersteller-CAD geklärt.

Die Firmware-Pinbelegung bleibt unverändert.


### Versorgung verbindlich präzisiert: 5 V für die Steuerplatine

**Nutzerfestlegung:** Auch die ESP32-C3-Steuerplatine erhält eine
**5-V-Versorgung**. Das steckbare ESP32-C3-Super-Mini-Modul bleibt
mechanisch entnehmbar. Die sechs Dosierpumpen sind davon getrennte
**12-V-Lasten**. Der PCA9685 verwendet **3,3-V-Logik**.

Die Angaben zu 12 V für Pumpen und 5 V für die Steuerung sind
unterschiedliche Versorgungsbereiche, kein Widerspruch.

**Noch nicht festgelegt:** Ob die 5 V für die C3-Trägerplatine
ausschließlich über USB-C am Modul oder von einer separaten
5-V-Einspeisung bereitgestellt werden; ebenso die zulässige
Strombelastung des Modulreglers und Rückspeisepfade.
Diese Fragen müssen vor einem elektrischen PCB-Schaltplan
geklärt werden. Keine parallelen Einspeisungen ohne Prüfung.

### C3-Versorgung: 5 V (Nutzervorgabe 15.11.2026)

**Festgelegt:** Auch das steckbare ESP32-C3-Super-Mini-Modul wird mit **5 V** versorgt, analog zum Sensor-ESP32. Die 5-V-Zuführung ist an den vorgesehenen 5-V-Eingang des konkreten Super-Mini-Boards zu führen; **keine 5 V an 3V3 oder GPIOs**. Die 12-V-Pumpenlastversorgung bleibt davon getrennt. Der PCA9685 ist mit kompatibler 3,3-V-Logikversorgung zu planen.

**Verbindlich festgelegt:** Die 5-V-Versorgung des ESP32-C3 Super Mini erfolgt **ausschließlich über dessen USB-C-Anschluss**. Keine parallele 5-V-Einspeisung über die Trägerplatine und keine 12 V am C3. **Noch zu prüfen:** Ob die verfügbare 3,3-V-Versorgung des Moduls die geplante Logiklast zuverlässig deckt und welche Stromversorgung die übrige C3-Trägerplatine benötigt.

### Maßangaben ESP32-C3 Super Mini aus Produktabbildung (15.11.2026)

Aus der vom Nutzer gelieferten bemaßten Produktabbildung:

- Leiterplattenbreite: **18,00 mm**.
- Leiterplattenlänge: **22,52 mm** (Körpermaß laut Abbildung, USB-C-Steckerüberstand beachten).
- Reihenmittenabstand der seitlichen Kontakte: **15,24 mm** (6 × 2,54 mm).
- **8 Kontaktpositionen je Seite**, insgesamt 16.
- Längsraster: **2,54 mm nominal**, aus dem erkennbaren Lochbild und der 15,24-mm-Bemaßung abgeleitet, nicht als separate Bildbemaßung ausgewiesen.
- Das Bild zeigt an den Kanten castellated Kontakte mit Löchern; vor Auswahl steckbarer Buchsenleisten prüfen, ob das gelieferte Modul angelötete Stiftleisten trägt bzw. nachgerüstet werden kann. **Bloße Kantenkontakte sind nicht direkt in normale Buchsenleisten steckbar.**
- Diese Maße beziehen sich **nur auf den C3**, nicht auf den 30-Pin-Sensor-ESP32.

Stand: vom Bild abgelesene Geometriedaten, tatsächliches Modul und USB-C-/Antennenfreiraum vor Bestellfreigabe prüfen.
