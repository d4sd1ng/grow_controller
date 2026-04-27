# Growroom Control System

Streamlit dashboard for simulated sensors, VPD helpers, optional Telegram alerts, and GPIO simulation (runs anywhere with Python 3.11+).

**Geräte-Setup (Server, Growroom-ESP, CYD):** [installation_instructions.md](installation_instructions.md) · **Verdrahtung:** [wiring_instructions.md](wiring_instructions.md) · **Deployment (RPi + optional VPS/VPN):** [deployment_vps.md](deployment_vps.md)

## Setup

```bash
python -m venv .venv
.venv\Scripts\activate   # Windows
pip install -r requirements.txt
```

Copy `.env.example` to `.env`. **Alle sensiblen und serverseitigen Settings** (Telegram, **`TELEMETRY_API_KEY`**, Ports, DB-Pfad, **`IOT_*`** für WLAN/Server-Host) kommen **nur aus `.env`** — der Python-Code nutzt `env_config`-Getter. Firmware: **`python export_firmware_secrets_from_env.py`** erzeugt git-ignorierte **`device_secrets.h`** pro Sketch aus derselben `.env`. **Telegram-Versand ist im Code aktuell auskommentiert** — Alerts landen in `alerts_log` und auf dem **CYD** (`/api/v1/mobile/summary`). Zum Reaktivieren in `main.py` die `telegram_notify`-Zeilen wieder einsetzen.

## Run

```bash
streamlit run main.py
```

**Telemetry-API** (ESP32 im Growroom → Server; **Cheap Yellow Display** holt die letzten Werte):

```bash
python run_telemetry_api.py
```

Standard: `http://<PC-IP>:8765` — u. a. `GET /api/v1/health` (SQLite-Ping, **ohne** API-Key), `POST /api/v1/telemetry`, `GET /api/v1/telemetry/series` (**echte Historie** aus SQLite, Parameter `hours` / `minutes` / `max_points`), `GET /api/v1/mobile/summary` (CYD: Messwerte + Relais + **Alerts**), `GET/POST /api/v1/control/*`. Optional **`X-Telemetry-Key`**. **`firmware/`**: `cyd_remote_control` nutzt `/telemetry/series` für die drei rechten Kurven.

Optional Telegram command bot (separate process; uses the same `.env` token):

```bash
python telegram_bot.py
```

## Telemetry (Speicher)

- Das Streamlit-Dashboard schreibt in **SQLite** nur, wenn sich Messwerte/Relais **merklich** ändern **oder** ein **Heartbeat** von ca. 60 s vergangen ist (weniger Reroll-Spam). ESP-/API-Ingest bleibt unverändert: jeder `POST /api/v1/telemetry` wie bisher eine Zeile.
- Speicherort: **`data/grow_telemetry.db`** standardmäßig (Ordner wird angelegt).
- Feste Spalten: Temperaturen, Luftfeuchte, Licht, EC, pH, vier Relais-Flags — passend zur Anzeige/Charts.
- Zusatzfelder aus dem ESP (andere Keys, verschachtelte Objekte) liegen als JSON in **`extras_json`** und gehen beim späteren Hardware-Import nicht verloren.
- **Retention:** alte Zeilen werden nach **`GROW_DATA_RETENTION_DAYS`** (Standard 90) gelöscht. **`GROW_DATA_DB=:memory:`** nur für Tests/ephemer.

Ursprünglich gedacht als zwei ESP32 (Sensoren + Relais) — **ein** Board reicht: Sensoren lesen, Relais schalten, ein Kanal (HTTP/MQTT/Serial) liefert JSON; der PC übernimmt Aggregation, Alerts und SQLite. Beim Anbinden echtes Payload-Felder auf die Tabellenspalten mappen (Relais ggf. aus `relays.heater` → Spalte `heater`).

## Security

**Eingrenzung:** Das Setup ist für ein **vertrauenswürdiges Heim-LAN** gedacht — nicht als hart abgesicherte Internet-API.

| Bereich | Stand im Projekt |
|--------|------------------|
| **Geheimnisse** | Tokens, API-Key, WLAN-Passwort nur in **`.env`** bzw. generiertem **`device_secrets.h`** (nicht committen). |
| **API-Zugriff** | Optional **`TELEMETRY_API_KEY`**: alle geschützten Routen verlangen Header **`X-Telemetry-Key`**. **Ohne** gesetzten Key ist die API im LAN für jeden erreichbar, der den Port nutzen kann. **`/api/v1/health`** bleibt **ohne** Key (nur DB-Erreichbarkeit, keine Messwerte). |
| **Transport** | **Python-Server:** typisch HTTP intern (Uvicorn); nach außen am VPS **HTTPS** (Proxy). **Firmware:** optional **`IOT_USE_HTTPS=1`** → TLS mit `WiFiClientSecure`; **`IOT_TLS_INSECURE=1`** überspringt Zertifikatscheck (MITM-Risiko). Heim-LAN ohne TLS: weiter **HTTP** möglich. |
| **Auswirkung** | **`POST /api/v1/control/*`** kann **Relais** und damit Heizung/Lüftung/Pumpe steuern — mit API-Key und Netztrennung absichern. |
| **Daten** | SQLite unter **`data/`** — Zugriff nur für den Server-Benutzer; Backups nicht öffentlich ablegen. |
| **CORS** | Standard **`TELEMETRY_CORS_ORIGINS=*`**; für Web-UI aus einer festen Origin ggf. einschränken. |
| **Telegram** | Bot-Token jemals in Git gelandet? In [BotFather](https://t.me/BotFather) **widerrufen** und neuen Token in `.env` setzen. |

**Pragmatische Maßnahmen:** `TELEMETRY_API_KEY` **setzen** (langer Zufallsstring), Telemetry-Port **nicht** ins Internet öffnen, Grow-Geräte in **Gast-WLAN / IoT-VLAN** mit Regeln nur zum Server-Port, Server und `.env` nur für vertrauenswürdige Konten.

## Tests

```bash
python -m unittest system_tests
```
