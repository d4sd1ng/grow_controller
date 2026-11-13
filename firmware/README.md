## Firmwarestand auf `main` (13.11.2026)

| Ordner | Stand | Funktion |
|---|---|---|
| `growroom_telemetry_client/` | Alter Repository-Stand, nicht als finale Sensorcontroller-Firmware verifiziert | Sensor- und Steuercontroller; keinesfalls automatisch mit alten Varianten ueberschreiben |
| `cyd_grow_display/` | LVGL Remote, C3-Schnittstelle Stand 1.2 | Server-Messwerte, HEAT/CIRC/EXH, C3-Luefter 1/2, gesperrte Dosierimpulse, C3 STOP; keine LVGL-Bedienung fuer Wasserpumpe oder Wachstum/Bluete |
| `esp32_c3_power_controller/` | C3-Firmware v1.2.0 mit PCA9685 | 5 SSR (inkl. Wasserpumpe vom Sensor-ESP), 6 Dosierpumpen, 2 Lueftergruppen, Watchdog und Status-API |
| `cyd_remote_control/` | Historische Alternative, nicht die LVGL-Firmware | Nicht mit dem LVGL-Sketch verwechseln |

**Integration nicht abschliessend nachgewiesen:** Der C3 liest `GET /api/local/actuators`
auf dem Sensor-ESP (`grow-sensor.local:8081`). Diese Schnittstelle und das Feld
`commands.water_pump` sind in der endgueltig verwendeten Sensor-Firmware noch
gegenzupruefen. Sie wurde bewusst nicht durch eine aeltere Version ersetzt.
Die LVGL-Remote liest Server-Telemetrie unter `/api/v1/mobile/summary`
und den C3-Status direkt unter `/api/v1/actuator/state`.

Aus Sicherheitsgruenden sind `HARDWARE_OUTPUTS_ARMED=false` und die
Dosiervorgabe `DOSER_UI_DURATION_MS=0`. Es wurden **kein Arduino-Compile,
kein ESP32-Hardwaretest und kein Systemintegrationstest** durchgefuehrt.

# ESP32-Firmware (Growroom + portables CYD)

## Rollen

| Gerät | Aufgabe |
|-------|---------|
| **Growroom-ESP** (fest, mit optionalem lokalem Display) | `GET /api/v1/control/state` → Relais am Hardware-GPIO. Sensoren messen; `POST /api/v1/telemetry` → Server/SQLite (Dashboard-Charts). Lokale Anzeige: z. B. dieselben Messwerte auf OLED/TFT oder wie im Sketch über **Serial**. |
| **Cheap Yellow Display** (portable) | `cyd_remote_control`: **3 Tafeln** — links Relais, Mitte Werte + Alert-Liste, rechts Mini-Verläufe T/rH/VPD (Ringpuffer aus Polls). Alternativ: `cyd_grow_display` (kompakt). |

**Server** (`run_telemetry_api.py` + optional Streamlit): eine SQLite-Datei — `app_control` (Modus + Relais-Soll), `readings`, `alerts_log`.

- **Automatik**: Streamlit regelt Relais (wenn Tab läuft + Modus „auto“) und schreibt Soll in die DB.
- **Manuell**: Soll kommt vom **CYD** oder Tastern im Dashboard; Growroom-ESP liest nur noch `control/state`.

## Endpoints (Header `X-Telemetry-Key` wenn `TELEMETRY_API_KEY` gesetzt)

- `POST /api/v1/telemetry` — Messwerte (+ optional `relays`)
- `GET /api/v1/control/state` — `{ control_mode, targets }`
- `POST /api/v1/control/targets` — Teilupdate Relais → Modus **manual**
- `POST /api/v1/control/mode` — `{"mode":"auto"|"manual"}`
- `GET /api/v1/mobile/summary` — Steuerung + letzte Messung + Alert-Liste
- `GET /api/v1/telemetry/series` — Zeitreihe aus SQLite (`minutes`, `hours`, `max_points`) für CYD-/Web-Charts
- `GET /api/v1/alerts/recent` — nur Meldungen
- `POST /api/v1/alerts` — Gerät kann Fehler an den Server melden (`message`, `severity`, `source`)

## Sensible Konfiguration

- **Nicht** WLAN-Passwort oder API-Key dauerhaft in den `.ino` im Repo pflegen.
- Im Projektroot **`.env`** mit u. a. **`IOT_WIFI_SSID`**, **`IOT_WIFI_PASSWORD`**, **`IOT_TELEMETRY_SERVER_HOST`**, **`TELEMETRY_API_PORT`**, **`TELEMETRY_API_KEY`** (siehe `.env.example`).
- **HTTPS** (VPS mit Reverse-Proxy): **`IOT_USE_HTTPS=1`**, optional **`IOT_TELEMETRY_SERVER_PORT=443`**, **`IOT_TLS_INSECURE=1`** für ESP ohne Zertifikatscheck (oder `0` + eigenes Root-CA im Code).
- Dann: **`python export_firmware_secrets_from_env.py`** — schreibt **`device_secrets.h`** in jeden Sketch-Ordner (gitignored). HTTP(S) läuft über **`grow_http_client.h`**.

## WLAN und API-Zugangsdaten ohne Git-Commit

Die Git-Historie und Pull Requests duerfen keine Zugangsdaten enthalten.
**Nie** echte SSIDs, WLAN-Passwoerter, API-Keys oder Tokens in .ino, .h,
Markdown, Issues oder Pull Requests schreiben.

1. Im Projektroot eine lokale Datei `.env` verwenden.
   Wenn diese noch nicht existiert: `cp .env.example .env`
   (eine vorhandene `.env` **nicht** ueberschreiben).
2. Lokal `IOT_WIFI_SSID`, `IOT_WIFI_PASSWORD`,
   `IOT_TELEMETRY_SERVER_HOST` und `TELEMETRY_API_KEY` setzen.
   Den gleichen API-Key am Telemetrie-Server konfigurieren.
3. `python export_firmware_secrets_from_env.py` im Projektroot ausfuehren.
   Das erstellt `firmware/<sketch>/device_secrets.h` fuer bestehende Sketchordner.
4. **Erst danach** den entsprechenden Arduino-Sketch kompilieren und flashen.
   Ohne die generierte Headerdatei bricht die Kompilierung absichtlich ab.
5. `.env` und `device_secrets.h` sind gitignored, bleiben lokal und duerfen
   nicht in Pull Requests, Support-Tickets oder Chats kopiert werden.

Achtung: Die Entfernung aus aktuellen Dateien entfernt fruehere Commits und
GitHub-PR-Diffs nicht. Bereits veroeffentlichte Zugangsdaten sofort austauschen.
Git-Verlauf und GitHub-Caches sind getrennt zu bereinigen.

## Arduino IDE

Sketch-Ordner = Dateiname des `.ino`.

- **TFT_eSPI** + CYD-`User_Setup` (siehe [ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)).
- **cyd_remote_control**: zusätzlich **XPT2046_Touchscreen** (SPI-Touch). Die Konstanten `CAL_*` und ggf. XPT-Pins musst du einmal an dein Board anpassen (Kalibrier-Sketch aus dem CYD-Repo).
- **ArduinoJson** v6.

### Noch mehr „Grafik“?

Auf dem CYD geht mit **TFT_eSPI** schon einiges (Rundungen, Farben, „LED“-Punkte, Layout wie oben). Wer **mehr UI** will (Screens, Slider, Animationen): **LVGL** mit Display-Treiber — deutlich mehr Flash/RAM, dafür echte GUI-Bausteine. Für eine Fernbedienung reicht oft TFT_eSPI + Touch wie im Remote-Sketch.

```bash
python run_telemetry_api.py
```

Firewall-Port **8765** (oder `TELEMETRY_API_PORT`) im LAN freigeben.
