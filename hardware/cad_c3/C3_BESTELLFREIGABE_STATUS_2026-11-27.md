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

## Getrennte Freigaben für PCB-Bestellung und Betrieb

Die bisherige Prüfung hat unzutreffend die komplette **Anlagensicherheit** als Voraussetzung für die Bestellung einer **unbestückten Leiterplatte** behandelt. Dies wurde in `release_gate_c3.py` korrigiert.

**PCB-Bestellung – nur technisch fertigungsrelevante Kriterien:** Alle Anschlussnetze vollständig festgelegt, Gesamtschaltplan/ERC, 12-V-Eingangs- und USB-Stromführung im Board ausgelegt, Footprints realteilgeprüft, Einbaumaße bestätigt, Gesamt-DRC bestanden, Gerber-/Bohrdaten geprüft. Aktuell **6 unvollständige Steckplätze und 8 fehlende Nachweise; keine Bestellfreigabe**.

**Betrieb der später bestückten Gesamtanlage – separat:** Realer Strom-/Thermiktest und Sicherheitsprüfung der vorgesehenen Anwendung. Diese zwei Prüfungen sind **nicht mehr Voraussetzung für eine unbestückte PCB-Bestellung**, bleiben aber vor dem bestimmungsgemäßen Betrieb relevant.

Die neuen Ergebnisfelder sind `pcb_order_ready` und `system_operation_ready`, `order_ready` bleibt ein Alias für die PCB-Bestellfreigabe zur Rückwärtskompatibilität. Es werden **keine** Freigaben automatisch auf `true` gesetzt.

**Bestellstatus weiterhin: NEIN**, weil relevante elektrische Teile im PCB-Design fehlen. Im aktuellen Entwurf gibt es noch keine vollständigen elektrisch überprüften SSR-Steuerschnittstellen und keinen vollständigen Lüfteranschluss. Es wäre falsch, diese per Softwarecheck als fertig zu behandeln.
