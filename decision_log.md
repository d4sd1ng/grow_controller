# Decision Log — Growroom Control System

## Format
Jeder Eintrag: Datum, Entscheidung, Kontext, Konsequenz.

---

## 2026-04-25 (diese Session)

### D01 — Keine Änderungen ohne Freigabe
- **Entscheidung**: Alle Code-Änderungen an lokalen Dateien nur noch mit expliziter Freigabe des Benutzers.
- **Kontext**: Mehrere falsche Annahmen führten zu chaotischen Änderungen (falsche Hardware-Identifikation, falsche Datei-Löschungen).
- **Konsequenz**: Strikte Trennung zwischen "Verstehen/Analysieren" und "Implementieren". Keine autonomen Datei-Änderungen mehr.

### D02 — cyd_remote_control auf Original-Stand zurücksetzen
- **Entscheidung**: `firmware/cyd_remote_control/` wird auf Commit `745d1b8` (vor LVGL-Commit) zurückgesetzt.
- **Kontext**: Commit `2055d39` (`feat(display): Neue mobile Anzeige mit LVGL v8 implementieren`) hat die ursprüngliche TFT_eSPI-Version überschrieben. Rückgängig machen.
- **Konsequenz**: `cyd_remote_control.ino` ist wieder die ursprüngliche 3-Panel-TFT_eSPI-Version. LVGL-Code für diesen Ordner ist entfernt.

### D03 — Steuerlogik auf ESP32 auslagern
- **Entscheidung**: Die Regelung (Heizung, Lüftung, Pumpe) läuft autonom auf dem ESP32. Der Server behält MANUAL-Override.
- **Kontext**: Der ESP32 im Growroom ist die "Steuerzentrale". Der Server dient Logging, Dashboard, Alert-History. Im MANUAL-Modus können Dashboard und CYD Relais über die API steuern.
- **Konsequenz**: `growroom_telemetry_client.ino` ist autonome Steuerzentrale mit Hysterese. `main.py` bleibt unverändert — es schickt im MANUAL-Modus Befehle an den ESP32, der sie ausführt. Kein Konflikt.

## 2026-04-24 (Vorsession — zusammengefasst aus Kontext)

### D04 — Hardware-Korrektur: Nicht 2 CYDs
- **Entscheidung**: Es gibt nur **ein** CYD. Das zweite Gerät ist ein ESP32-WROOM-32D mit integriertem OLED.
- **Kontext**: Ursprünglich wurden zwei CYDs angenommen. Der Benutzer korrigierte: nur ein CYD, der andere ist ein anderer ESP32 mit fest verdrahtetem OLED.
- **Konsequenz**: `cyd_grow_display` = CYD (LVGL). `cyd_remote_control` war eine Fehlannahme und wurde später korrigiert.

### D05 — LVGL nur auf CYD, nicht auf ESP32 WROOM
- **Entscheidung**: LVGL v8 wird nur für das CYD (`cyd_grow_display`) verwendet. Der ESP32 WROOM bekommt kein LVGL.
- **Kontext**: ESP32 WROOM hat weniger Ressourcen/Speicher und ein kleines OLED (128x64). LVGL ist overkill.
- **Konsequenz**: `cyd_grow_display.ino` = LVGL v8. `growroom_telemetry_client.ino` = einfache Text-Ausgabe auf SSD1306.

### D06 — OLED ist rein passiv (keine Touch-Steuerung)
- **Entscheidung**: Das SSD1306 OLED am ESP32 WROOM zeigt nur Werte an. Es steuert nichts.
- **Kontext**: ESP32-WROOM-32D mit OLED hat keinen Touch. Relais-Steuerung läuft autonom oder über Server/CYD.
- **Konsequenz**: Keine Touch-Buttons, keine Serial-Commands für Relais im `growroom_telemetry_client.ino`.

### D07 — I2C-Sensorerkennung implementieren
- **Entscheidung**: ESP32 scannt beim Boot den I2C-Bus und erkennt automatisch angeschlossene Geräte (OLED, BH1750, etc.).
- **Kontext**: Flexible Hardware-Konfiguration ohne Code-Änderung.
- **Konsequenz**: `scanI2CDevices()` in `growroom_telemetry_client.ino`. Erkannte Geräte werden in `device_info` an Server gemeldet.

## 2026-04-23 (Vorsession — frühere Entscheidungen)

### D08 — Architektur: Server + ESP32 + CYD
- **Entscheidung**: Drei-Ebenen-Architektur: Python-Server (Aggregation), ESP32 (Sensoren + Relais), CYD (Mobile Anzeige/Steuerung).
- **Kontext**: Ursprünglich war der Server die Steuerzentrale. ESP32 war nur "dumme" Ausführungseinheit.
- **Konsequenz**: API-Endpunkte `/api/v1/control/state` und `/api/v1/telemetry`. SQLite-Datenbank für Historie.

### D09 — CYD als Fernbedienung mit 3 Panels
- **Entscheidung**: CYD zeigt drei Panels: Links (Relais-Buttons), Mitte (Messwerte + Alerts), Rechts (Charts/Historie).
- **Kontext**: Mobile Überwachung und Steuerung aus der Ferne.
- **Konsequenz**: TFT_eSPI mit manuellem Zeichnen (später LVGL v8 in `cyd_grow_display`).

---

## Geklärte Entscheidungen (diese Session finalisiert)

| ID | Frage | Entscheidung |
|----|-------|-------------|
| O01 | Soll der ESP32 im MANUAL-Modus Befehle vom Server annehmen? | **Ja** — ESP32 holt alle 5s `/api/v1/control/state`. Im MANUAL-Modus führt er Server-Befehle aus. |
| O02 | Sollen Setpoints hardcoded oder vom Server konfigurierbar sein? | **Hardcoded defines** im ESP32-Code (TARGET_AIR_TEMP, etc.). Server dient Monitoring + Override. |
| O03 | `cyd_remote_control` vs `cyd_grow_display` — beides oder eins? | **Beide sind separate Geräte.** `cyd_grow_display` = CYD mit LVGL v8 (Monitor). `cyd_remote_control` = CYD mit TFT_eSPI (Fernbedienung). |
