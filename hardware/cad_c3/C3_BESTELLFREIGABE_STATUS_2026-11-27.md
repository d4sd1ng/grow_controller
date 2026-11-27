# Grow Controller C3 – Fertigungsfreigabe (Statusbericht)

**27. November 2026 · Status: NICHT BESTELLFERTIG**

## Der derzeit tatsächlich vorliegende CAD-Umfang

- Leiterplatten-Entwurfsdatei: `c3_6motor_usb_service_disconnect_DRAFT_NOT_FOR_ORDER.kicad_pcb`.
- Platinenumriss im CAD: **160 × 116 mm**; Gehäuse und Befestigungsmaße am Original nicht abgenommen.
- 50 Footprints insgesamt, 6 generische DC-Motorkanal-Kandidaten auf der Niederspannungsplatine.
- Ein 12-V-Eingang, 12-V→5-V-Konverterkandidat, steckbare C3-Aufnahme mit manuell trennbarem 5-V-Servicepfad.
- Ein PCA9685 mit modellierten 3,3-V-Steuersignalen.
- 5 Klemmenpositionen J2–J6 und eine dreipolige J13: **ohne elektrische Netze**.
- Der letzte KiCad-DRC prüfte ausschließlich die im CAD modellierten Verbindungen. Er kann elektrisch unzugeordnete Klemmen nicht als fehlende Verbindungen erkennen.

## Bestellhindernisse (Freigabe erforderlich)

| Prüffeld | Nachweis | Status |
|---|---|---|
| Alle Steckanschlüsse elektrisch belegt und verifiziert | J2–J6, J13 ohne Netze | BLOCKIERT |
| Vollständiger Schaltplan / ERC | kein freigegebener Gesamtschaltplan | BLOCKIERT |
| Eingangsschutz der 12-V-Versorgung | Sicherungs-/Schutzauslegung fehlt | BLOCKIERT |
| USB-/5-V-Quellenumschaltung | JP1 ist nur ein manuell zu trennender Versorgungspfad; realer USB-VBUS-Weg ungeprüft | BLOCKIERT |
| Bauteil-/Footprintfreigabe | Originalteile einschließlich C3-Modul, Klemmen und Konverter nicht geprüft | BLOCKIERT |
| Elektrische Last- und Thermikprüfung | tatsächliche Einschalt-/Blockierströme noch nicht belegt | BLOCKIERT |
| Mechanische Freigabe | Einbaumaße und Steckerzugänglichkeit offen | BLOCKIERT |
| Unabhängige Prüfung des vollständigen Systems | keine Freigabe für gefährdungsrelevante Anwendungen | BLOCKIERT |
| KiCad DRC für fertige Gesamtschaltung | bisher nur Teilentwurf geprüft | BLOCKIERT |

## Verifizierbare Prüfungen und Sicherung gegen Fehlbestellung

- Bestehender Teilentwurf: KiCad-9-DRC **0 Verstöße** bei zuletzt geprüftem Layout, und ein statischer Pad-/Netztest für die bereits angelegten Pins. Das ist **keine** Freigabe aller Funktionen.
- Neuer **Fail-closed-Fertigungscheck**: `release_gate_c3.py` prüft bewusst *auch* freie J2–J6/J13-Pads sowie Nachweise in `C3_RELEASE_EVIDENCE.json`. Bei offenen Anforderungen muss dieser Check mit `order_ready: false` abbrechen.
- CI: <https://github.com/d4sd1ng/homepage/actions/runs/45969053802>. (Workflow prüft absichtlich, dass die Freigabe des unfertigen Boards blockiert wird.)

**Keine Gerber-/Drill-/BOM-Daten aus dem Entwurf als freigegebene Bestellung ausgeben.** Ein grün durchlaufender DRC und eine fertige Platinenkontur allein reichen dafür nicht aus.
