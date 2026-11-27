# C3 12-V-Leiterplatte – Prototyp-Fertigungsdaten

**Stand:** 27.11.2026 · **Dateistand:** `c3_12v_5signals_1fan_WIP_NOT_FOR_ORDER.kicad_pcb`.

## Fertigungsdatei – verfügbar

**ZIP:** https://raw.githubusercontent.com/d4sd1ng/homepage/ci/c3-kicad-floorplan-20261125/fabrication/grow_controller_c3_12v_r1/Grow_Controller_C3_12V_R1_Gerber_Drill.zip

**Erzeugung und Prüfung:** https://github.com/d4sd1ng/homepage/actions/runs/46036614871

**SHA256 des zugehörigen KiCad-Boards:** `32034c2bfdbc816ac076a8a4b133e30e868831a5b383d6151697a589ae3079e2`.

Der Fertigungsstand enthält eine Kontur von **160 × 116 mm**, zwei Kupferlagen, Masken- und Bestückungsdrucklagen sowie Excellon-Bohrdaten. Das ZIP enthält ferner KiCad-Board, Stückliste/Anschlussbelegung und den KiCad-DRC-Bericht.

Die KiCad-9-Prüfung des zugehörigen Layoutstands meldete:
- **0 DRC-Verstöße**
- **0 als unverbunden gemeldete Pads**
- **0 Footprint-Fehler**
- Erfolgreicher Export aller sieben üblichen Gerber-Lagen sowie der Bohrdatei.

## Klarstellung des technischen Status

Die ZIP-Datei ist ein **Fertigungspaket für einen unbestückten Prototypen**. Die reale Passform aller Bauteile und die elektrische Funktion der anschließend bestückten Platine wurden damit nicht nachgewiesen. Insbesondere wurden Netzteilbuchse/Stecker nicht an Originalteilen abgenommen und die tatsächliche Gesamtstrombelastbarkeit nicht durch einen Musteraufbau bestätigt.

Das Prüfergebnis `pcb_order_ready: false` in der übergeordneten Freigabedatei bezeichnet die **noch ausstehende umfassende technische Freigabe** und darf nicht mit dem Vorhandensein technisch gültig exportierter Gerber/Bohrdaten verwechselt werden.

**Verifizierte Feststellung:** Ein vollständiger Satz exportierter Produktionsdaten für den CAD-Entwurf liegt vor. Eine betriebsfertige Geräteabnahme ist damit nicht erfolgt.
