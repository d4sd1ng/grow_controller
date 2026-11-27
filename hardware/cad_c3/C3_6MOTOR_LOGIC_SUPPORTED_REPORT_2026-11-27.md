# C3-Unterbau: 6 generische 12-V-EIN/AUS-Ausgänge – CAD-Prüfbericht
27.11.2026 – nur allgemeiner Niederspannungs-Motorschaltentwurf; **nicht bestellfertig**.

## Implementierter CAD-Kandidat
- `hardware/cad_c3/c3_6motor_logic_supported_DRAFT_NOT_FOR_ORDER.kicad_pcb`.
- 6 voneinander getrennte Motor-Klemmenpaare J7–J12, 6 MOSFET-Kandidaten, je ein Gate-Serienwiderstand, Gate-Pulldown und induktiver Schutzdioden-Kandidat pro Kanal.
- PCA9685 VDD=3,3 V, SDA/SCL-Pull-ups (4,7 kOhm), /OE-Pull-up (10 kOhm), 100-nF-Kondensator am PCA9685.
- 12-V-Eingang J1, 5-V-Wandlerkandidat, 1N5819-Rückstromsperre in der 5-V-Zuleitung zur Modulversorgung.
- Alle sechs Ausgangssignale sind generische digitale Schaltsignale; keine Fördermengen-, Medien- oder automatischen Dosierfunktionen.

## Verifizierte CAD-Prüfungen
- **KiCad 9 Strict DRC: PASS, 0 DRC-Verstöße, 0 gemeldete unverbundene Pads, 0 Footprint-Fehler**.
- Zusätzlicher statischer `audit_c3_netlist.py`-Test: **PASS**; prüft benannte Versorgungspads, PCA-Pins, Gate-/Schalt-/Schutzknoten und Anschlussklemmen aller 6 Kanäle, Logik-Pull-ups und Stützkondensator.
- CI-Lauf: https://github.com/d4sd1ng/homepage/actions/runs/45937203717.

**Technische Grenze:** DRC und ein passender Pad-Netz-Audit beweisen keine elektrische Funktionsfähigkeit, korrekte Stromtragfähigkeit, sichere Stromquellenwahl oder vollständige Schnittstellen-Bestückung. Die übrigen Klemmen und Aktoren sind in diesem Projekt noch nicht vollständig spezifiziert und teilweise netzlos.

## Wichtiger offener Spannungsversorgungsfehler
Das aktuell für USB-Service vorgesehene ESP32-C3-SuperMini-Modul kann USB-VBUS direkt mit seinem 5-V-Pin verbinden. **Eine Schottky-Diode nur zwischen dem externen Buck und C3-5V verhindert lediglich Rückstrom zum Buck, aber NICHT zwingend Rückspeisung vom Buck in den angeschlossenen USB-Host.** Daher darf die bestehende Lösung NICHT als vollständig USB-entkoppelt oder gleichzeitig per USB und 12-V-Eingang betreibbar bezeichnet werden. Eine nachgewiesene Stromquellenwahl bzw. elektrische Trennung ist vor einer Bestellfreigabe erforderlich.

## Noch nicht geprüft / nicht freigegeben
1. Der reale Stromverbrauch des C3-Moduls und die Belastung der 3,3-V-Versorgung, einschließlich Start- und Funklast.
2. Eingangsschutz, Absicherung, Abblockung und Belastbarkeit des 12-V-Versorgungszweigs bei Motorstart und blockierten Motoren.
3. Gegenprüfung der konkreten gelieferten MOSFET-, Dioden- und Step-down-Package-Footprints mit Originalteilen sowie thermische Bewertung.
4. Vollständiger KiCad-Schaltplan samt ERC, Hardwaretests im ungefährlichen Kleinspannungsbereich, mechanische Gehäusefreigabe.
5. Fünf SSR-Signalschnittstellen und gemeinsamer Lüfterausgang: bisher nicht als vollständige elektrische Schaltstufen freigegeben; keine Netzspannung auf dieser Leiterplatte.
6. Betrieb mit gefährlichen Fördermedien, inklusive gesondert vorzusehender Sicherheitseinrichtungen, ist nicht Teil dieses generischen PCB-Prüfstands.
7. Keine freigegebenen Fertigungsdateien, Stückliste zum Einkauf oder Bestellung.

**Freigabestatus: BLOCKIERT / DRAFT – NICHT ZUR FERTIGUNG.**
