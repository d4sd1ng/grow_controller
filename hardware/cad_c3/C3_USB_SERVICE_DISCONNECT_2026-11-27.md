# C3-Trägerplatine – Service-Trennung der 5-V-Versorgung
Stand 27.11.2026 · **Prototyp / keine Fertigungsfreigabe**

## Ursache
Beim verwendeten steckbaren ESP32-C3-SuperMini ist nicht nachgewiesen, dass USB-VBUS vollständig von seinem 5-V-Pin entkoppelt ist. Eine Diode zwischen Buck-Wandler und C3 allein verhindert *nicht* zuverlässig, dass extern erzeugte 5 V bei gestecktem USB-Kabel in die USB-Stromversorgung zurückfließen.

## Ergänzung im PCB-CAD
Datei: `c3_6motor_usb_service_disconnect_DRAFT_NOT_FOR_ORDER.kicad_pcb`

- `JP1`: zweipolige 2,54-mm-Stiftleiste mit **abziehbarer Kurzschlussbrücke**, vorgesehen zwischen der Kathode von `D1` und dem C3-5-V-Eingang.
- `+5V_BUCK` → `D1` → `+5V_DIODE` → **JP1 (Brücke)** → `+5V_PROTECTED` → ESP32-C3-SuperMini 5V.
- Die Brücke stellt eine **manuelle**, keine automatische Quellenumschaltung dar.
- Ein auf der Platine eingezeichneter Hinweis lautet: „JP1: REMOVE BEFORE USB SERVICE“.
- Bei **nicht** gesetzter Brücke ist die externe 5-V-Zuleitung zum C3-Pin 5V im PCB-Strompfad getrennt. USB-VBUS kann das Modul dann separat versorgen, sofern das konkrete Modul dies wie vorgesehen unterstützt.
- Weder das Steckverbinder-Footprint noch seine Position gelten als Herstellerfreigabe oder geprüfte Isolation; die tatsächlichen Modul-Rückspeisewege sind weiterhin vor Fertigung zu prüfen.
- Vor jedem USB-Service müssen externe Lasten spannungslos sein. Eine versehentlich gesteckte Brücke bei gleichzeitigem Anschluss von USB und 12 V kann weiterhin zu einer Rückspeisung führen.

## Status des Gesamtcontrollers
- Nur die Trennung des generischen 5-V-Niederspannungspfads ist mit `JP1` bearbeitet.
- Vollständiger Schaltplan und ERC, Verdrahtung sämtlicher noch freien Anschlussklemmen, Absicherung, endgültige Teileauswahl, Belastungstests und mechanische Freigabe fehlen.
- Ein fehlerfreier DRC ist **keine** Nachweisführung für sichere USB-Stromquellenwahl oder die Eignung für gefährliche Fördermedien.
- Kein netzspannungsführender Anschluss oder Netzlast-Plan ist Bestandteil dieses CAD-Bereichs.

**Freigabe: DRAFT, NICHT BESTELLFERTIG.**
