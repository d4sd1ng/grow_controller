# Sensor-PCB — Designprüfung und Fertigungsstatus (15.11.2026)

**Aktuelle PCB:** `sensor_pcb_routed_candidate.kicad_pcb`
**Aktuelle ausführliche Prüfung:** `sensor_pcb_routed_review.md`

Die früheren Teile dieses Dokuments enthielten einen alten
Prototypenstand (81 Segmente), offene ADC-Netznamen und einen
**nicht bestätigten pH-Spannungsteiler**. Diese Zwischenstände sind
durch den jetzigen PCB-Entwurf **ersetzt**. Historische Änderungen
bleiben im Git-Verlauf nachvollziehbar.

### Elektroplanung

- Sensor-ESP32 DevKit 30 Pins steckbar, 5 V nur per USB-C.
- ADS1115 (1 × 10) steckbar, 3,3 V, SDA=GPIO21, SCL=GPIO22,
  ADDR=GND (I²C 0x48).
- pH PH4502C: 5-V-Versorgung, zwei GND-Anschlüsse,
  **PO direkt an ADS1115 A0**.
- EC/TDS: 5-V-Versorgung, GND, Signal direkt an ADS1115 A1.
- BH1750 SDA/SCL an GPIO13/14, separater I²C-Bus.
- MLX90640 mit eigenem vierpoligen Stecker an GPIO21/22.
- Drei DHT22, DS18B20, A02YYUW, RGB-LED mit gemeinsamer
  Kathode, zwei Taster; jeweils eigener Steckplatz.
- Widerstände ausschließlich bedrahtet: 4 × 4,7 kΩ,
  3 × 220 Ω bevorzugt, 4 × optionale I²C-Pull-ups.
  Keine zusätzliche SMD-LED und keine SMD-Widerstände.

### Elektrischer PCB-Status

25 Footprints, 100 Pads, 1213 Segmente, 64 Vias,
alle 22 Mehrfachnetze im eigenen geometrischen Test verbunden,
keine erkannten Fremdnetzkollisionen unter 0,2 mm.
Der verwendete Platzhalter-Pinabstand des ESP32 beträgt
25,40 mm Reihenmitte bei 2,54 mm Pinraster,
ADS1115 1 × 10 bei 2,54 mm.

### Fertigungsstatus

**Nicht freigegeben:** Modul- und Klemmen-CAD-Footprints
sind noch nicht durch reale Bauteile/3D geprüft;
Gehäuse und Montagelöcher sind nicht festgelegt.
KiCad-CLI ist in dieser Bearbeitungsumgebung nicht vorhanden,
daher **kein echter DRC, kein ERC, keine Gerber/Excellon-Dateien**.

Der Source-Code und die vollständige Bestell-/Prüfliste liegen
in `sensor_pcb_routed_candidate.kicad_pcb` und
`sensor_pcb_routed_review.md`.
