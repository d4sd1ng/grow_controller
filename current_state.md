# Current State — Growroom Control System

**Stand**: 2026-04-25 (nach Session-Rücksetzung)

---

## Python-Server (unverändert)

| Datei | Status | Anmerkung |
|-------|--------|-----------|
| `main.py` | ✅ Läuft | Streamlit Dashboard, Steuerlogik (PID + Hysterese), GPIO-Simulation |
| `telemetry_api.py` | ✅ Läuft | FastAPI REST API, alle Endpunkte aktiv |
| `telemetry_store.py` | ✅ Läuft | SQLite DB, app_control-Tabelle, Alerts, I2C-Device-Info |
| `data_manager.py` | ✅ Läuft | High-Level Wrapper für Store |
| `pid_controller.py` | ✅ Läuft | PID-Referenzimplementierung (Python) |
| `ml_controller.py` | ✅ Läuft | ML-Anomalie-Erkennung (scikit-learn) |
| `schedule_manager.py` | ✅ Läuft | Zeitpläne für Licht & Co. |
| `esp32_controller.py` | ✅ Läuft | Simulator für Tests, enthält `device_info` mit I2C-Geräten |
| `system_tests.py` | ✅ 14/14 OK | Alle Tests passen |

### Steuerlogik aktuell
- **Primäre Regelung**: `growroom_telemetry_client.ino` (ESP32) — Hysterese-basiert, autonom im AUTO-Modus
- **MANUAL-Override**: `main.py` (Python-Server) sendet Relais-Zielzustände via API. ESP32 führt sie aus.
- **Server-Steuerlogik**: `main.py` enthält PID + Hysterese für Simulation/Monitoring. Im echten Betrieb schaltet der ESP32 direkt die Relais.

---

## Firmware / ESP32

### 1. `growroom_telemetry_client.ino` — ESP32-WROOM-32D (Growroom)

| Aspekt | Status |
|--------|--------|
| **I2C-Scanner** | ✅ Implementiert — erkennt OLED (0x3C/0x3D), BH1750 (0x23/0x5C) |
| **Sensor-Lesung** | ✅ Echte Libraries: DHT22 (GPIO36), ADC für EC/pH (GPIO34/35) |
| **Relais-Schaltung** | ✅ GPIO-Ausgänge definiert und geschaltet (GPIO4,5,16,17) |
| **Steuerlogik** | ✅ Hysterese-Regelung (AUTO) + MANUAL-Modus mit Server-Befehlen |
| **OLED-Anzeige** | ✅ SSD1306 128x64, 4 Zeilen: Temp/RH, Case-Temp, EC/pH, Modus/Relais |
| **Telemetry senden** | ✅ POST /api/v1/telemetry alle 10s |
| **Control-State holen** | ✅ GET /api/v1/control/state alle 5s |
| **WiFi-Verbindung** | ✅ Standard WiFi-Client |
| **Gehäuse-Temperatur** | ✅ ESP32 interner Temperatursensor |

**Aktueller Modus**: ESP32 ist **autonome Steuerzentrale** — regelt im AUTO-Modus selbst, nimmt im MANUAL-Modus Befehle vom Server/Dashboard/CYD entgegen.

### 2. `cyd_grow_display.ino` — CYD (ESP32-2432S028R)

| Aspekt | Status |
|--------|--------|
| **Display-Treiber** | ✅ TFT_eSPI → LVGL v8 (umbgebaut in früherer Session) |
| **Touch** | ✅ LVGL Touch-Callback + 4 Relay-Buttons |
| **UI** | ✅ Flex-Layout mit Labels (Mode, Temp, RH, EC/pH, Relais, Alerts) + Button-Row |
| **HTTP-Kommunikation** | ✅ `grow_http_client.h`, Timeout 5s |
| **WiFi-Reconnect** | ✅ Automatisches Reconnect alle 5s bei Verlust |
| **Kommunikation mit Server** | ✅ Holt Daten via `/api/v1/mobile/summary`, sendet Relais-Befehle via `/api/v1/control/targets` |

**Dateien im Ordner**:
- `cyd_grow_display.ino` (LVGL-Version)
- `lv_conf.h` (LVGL-Konfiguration)

### 3. `cyd_remote_control.ino` — CYD als Fernbedienung

| Aspekt | Status |
|--------|--------|
| **Display-Treiber** | ✅ TFT_eSPI (manuell, ursprünglicher Code) |
| **Touch** | ✅ XPT2046 mit Kalibrierung |
| **UI** | ✅ 3 Panels: Relais-Buttons, Werte, Charts |
| **HTTP-Kommunikation** | ✅ `grow_http_client.h` |
| **Relais-Steuerung** | ✅ Touch-Buttons senden POST an Server |
| **Charts** | ✅ Sparklines aus lokalem Ringpuffer oder Server-Serie |

**Dateien im Ordner**:
- `cyd_remote_control.ino` (TFT_eSPI-Version, zurückgesetzt auf `745d1b8`)
- `grow_http_client.h`

**Anmerkung**: `cyd_remote_control` und `cyd_grow_display` sind **zwei separate CYD-Geräte** mit unterschiedlicher Firmware. `cyd_remote_control` = TFT_eSPI (Fernbedienung). `cyd_grow_display` = LVGL v8 (Monitor).

---

## Gelöschte / Rückgängig-gemachte Änderungen (diese Session)

| Datei | Aktion | Grund |
|-------|--------|-------|
| `firmware/cyd_remote_control/cyd_remote_control.ino` | Gelöscht → Wiederhergestellt | Falsche Annahme: kein zweites CYD |
| `firmware/cyd_remote_control/lv_conf.h` | Gelöscht | LVGL-Konfig für nicht-existierendes Gerät |
| `firmware/cyd_grow_display/cyd_grow_display.ino` | Unverändert in dieser Session | LVGL-Version aus früherer Session |
| `firmware/growroom_telemetry_client/growroom_telemetry_client.ino` | **Komplett umgeschrieben** | Autonome Steuerzentrale mit OLED, Hysterese, echten Sensoren |

---

## Neue Dokumentation (diese Session)

| Datei | Zweck |
|-------|-------|
| `project_overview.md` | Komplettes Projekt-Überblicksdokument |
| `task_contract.md` | Aufgaben-Liste mit Status und Priorität |
| `decision_log.md` | Chronologische Entscheidungsverfolgung |
| `current_state.md` | Diese Datei — technischer Stand |

---

## Bekannte Probleme / To-Do

1. **ADC-Kalibrierung feinjustieren**: Die EC/pH-Werte hängen vom konkreten LM358/Verstärker-Modul ab. Kalibrierwerte in `growroom_telemetry_client.ino` müssen ggf. angepasst werden.
2. **Setpoints dynamisch**: Aktuell hardcoded (`TARGET_AIR_TEMP`, etc.). Optional: Vom Server konfigurierbar machen.
