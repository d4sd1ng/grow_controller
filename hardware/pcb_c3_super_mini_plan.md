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
- **Anschlusskante 1:** fünf SSR-Steuerschnittstellen und zwei
  Lüftergruppen-Schnittstellen.
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
