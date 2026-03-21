# Installation — Growroom Control System

Anleitung **pro Gerät**: was auf dem **Server**, dem **Growroom-ESP32** und dem **Cheap Yellow Display (CYD)** installiert und konfiguriert werden muss. **Verdrahtung der Relais und Sensoren:** siehe [wiring_instructions.md](wiring_instructions.md). **API-Übersicht & Sketch-Hinweise:** [firmware/README.md](firmware/README.md) · **Deployment (Raspberry Pi als Zentrale, VPS nur VPN o. Ä.):** [deployment_vps.md](deployment_vps.md).

**Sicherheit:** Alle **sensiblen** Einstellungen (Telegram-Token, Chat-ID, **Telemetry-API-Key**, **WLAN-Zugangsdaten** für die Geräte, LAN-Host des Servers für die ESPs) liegen nur in der Datei **`.env`** im Projektroot (nicht ins Repository). Python liest sie über `env_config`; die Firmware kann dieselben Werte mit **`python export_firmware_secrets_from_env.py`** in **`device_secrets.h`** pro Sketch-Ordner schreiben (Datei ist per `.gitignore` ausgeschlossen).

---

## Inhalt

1. [Rollen im Setup](#1-rollen-im-setup)
2. [Server (PC / Mini-PC / Pi)](#2-server-pc--mini-pc--pi)
3. [Growroom-ESP32 (fest im Raum)](#3-growroom-esp32-fest-im-raum)
4. [Cheap Yellow Display (portable Fernbedienung)](#4-cheap-yellow-display-portable-fernbedienung)
5. [Netzwerk, Firewall, API-Key](#5-netzwerk-firewall-api-key)
6. [Optional: LM358 für EC/pH-Verstärkung](#6-optional-lm358-für-ecph-verstärkung)
7. [Optional: Absicherung mit Sicherungen](#7-optional-absicherung-mit-sicherungen)

---

## 1. Rollen im Setup

| Komponente | Aufgabe |
|------------|---------|
| **Server** | Python: **Telemetry-API** (SQLite, REST für ESP + CYD), optional **Streamlit-Dashboard** (Simulation/Automatik, Charts). Eine gemeinsame DB-Datei (`data/grow_telemetry.db` o. Ä.). |
| **Growroom-ESP32** | Sensoren lesen, **Relais** schalten; zyklisch **`POST /api/v1/telemetry`** und **`GET /api/v1/control/state`**. Sketch: `firmware/growroom_telemetry_client/`. |
| **CYD** | Portables Display: Messwerte, Alerts, Relais-Steuerung, optional Kurven. Sketches: `firmware/cyd_remote_control/` (Touch + Charts) oder **`cyd_grow_display`** (kompakter). |

---

## 2. Server (PC / Mini-PC / Pi)

### 2.1 Voraussetzungen

- **Python 3.11+**
- Projektordner `grow_controller` (dieses Repository)

### 2.2 Virtuelle Umgebung & Dependencies

```bash
python -m venv .venv
```

Windows (PowerShell):

```powershell
.\.venv\Scripts\Activate.ps1
pip install -r requirements.txt
```

Linux/macOS:

```bash
source .venv/bin/activate
pip install -r requirements.txt
```

Installiert u. a.: **Streamlit**, **pandas**, **plotly**, **FastAPI**, **uvicorn**, optional **python-telegram-bot**.

### 2.3 Konfiguration — alles Server-seitige aus **`.env`**

Die Python-Prozesse laden die Datei **`.env`** im Projektroot (`env_config.load_dotenv_if_present()`). **Sensible und betriebsrelevante Server-Werte** werden ausschließlich über **`env_config`**-Getter aus der Umgebung gelesen (nach dem Laden von `.env`) — nicht aus hart codierten Secrets im Quelltext.

- `.env.example` nach **`.env`** kopieren und anpassen (`.env` **nie** committen).
- **Typische Variablen in `.env`:**
  - **`TELEGRAM_BOT_TOKEN`** / **`TELEGRAM_CHAT_ID`** — nur für den Bot (sensible Tokens)
  - **`TELEMETRY_API_KEY`** — optional; wenn gesetzt, Header **`X-Telemetry-Key`** an allen geschützten Routen
  - **`TELEMETRY_API_PORT`**, **`TELEMETRY_API_HOST`**, optional **`TELEMETRY_CORS_ORIGINS`**, **`TELEMETRY_API_RELOAD`**
  - **`GROW_DATA_DB`**, **`GROW_DATA_RETENTION_DAYS`**
  - **`IOT_WIFI_SSID`**, **`IOT_WIFI_PASSWORD`**, **`IOT_TELEMETRY_SERVER_HOST`** — für **`device_secrets.h`**
  - optional **`IOT_USE_HTTPS`**, **`IOT_TELEMETRY_SERVER_PORT`**, **`IOT_TLS_INSECURE`** — TLS zur API (z. B. Hetzner + Caddy); siehe [deployment_vps.md](deployment_vps.md)

**ESP32/CYD und `.env`:** Die MCU kann keine `.env` lesen. **Empfohlen:** Nach dem Befüllen der `.env` im Projektroot:

```bash
python export_firmware_secrets_from_env.py
```

Das erzeugt **`device_secrets.h`** (WLAN, Host, Client-Port, API-Key, **`SERVER_USE_HTTPS`**) und die Sketches nutzen **`grow_http_client.h`** für HTTP oder HTTPS. **Ohne** `device_secrets.h`: Fallback **HTTP** + Platzhalter im `#else`-Zweig.

### 2.4 Prozesse starten

**Telemetry-API** (für ESP + CYD Pflicht im Regelbetrieb):

```bash
python run_telemetry_api.py
```

Erreichbar unter `http://<SERVER-LAN-IP>:<Port>` — **Port = `TELEMETRY_API_PORT` aus `.env`** (Standard **8765**). Prüfen: **`GET /api/v1/health`** (SQLite-Ping, **ohne** API-Key).

**Streamlit-Dashboard** (optional, für lokale Bedienung/Automatik):

```bash
streamlit run main.py
```

**Telegram-Bot** (optional, separater Prozess):

```bash
python telegram_bot.py
```

> **Hinweis:** Streamlit schreibt Messwerte gedrosselt in SQLite; der **Growroom-ESP** liefert weiterhin über **`POST /api/v1/telemetry`** pro Zyklus eine Zeile — so bleibt die Historie für Charts zuverlässig.

### 2.5 Firewall / Router

- Den in **`.env`** gesetzten **`TELEMETRY_API_PORT`** (oder Standard **8765**) im **LAN** für ESPs/CYD freigeben (Windows-Firewall eingehend, Ziel: Python/`uvicorn`).

---

## 3. Growroom-ESP32 (fest im Raum)

### 3.1 Entwicklungsumgebung

- **Arduino IDE** 2.x (oder **PlatformIO** mit gleichen Libraries), Board-Paket **ESP32** (Espressif).

### 3.2 Sketch

- Ordner **`firmware/growroom_telemetry_client/`** öffnen; die `.ino`-Datei muss **gleichnamig** zum Ordner sein (`growroom_telemetry_client.ino`).

### 3.3 Arduino-Libraries (Library Manager)

| Library | Version / Hinweis |
|---------|-------------------|
| **ArduinoJson** | v6 |
| (sonst nur Board-Core) | **WiFi**, **HTTPClient** — im ESP32-Core enthalten |

### 3.4 Sensible Werte & Sketch

- **Production:** `IOT_*` und Telemetry-Keys in **`.env`**, dann **`python export_firmware_secrets_from_env.py`** vor dem Flashen.
- **Relais-Pins:** **`PIN_RELAY_*`** und ggf. **HIGH/LOW** weiterhin im `.ino` (Hardware, keine Secrets) — siehe [wiring_instructions.md](wiring_instructions.md).

### 3.5 Ablauf nach dem Flashen

- Gerät bootet, verbindet WLAN, pollt regelmäßig **`/api/v1/control/state`**, setzt Relais, sendet **`/api/v1/telemetry`** mit Messwerten (im aktuellen Sketch sind Messwerte noch Platzhalter — durch echte Sensor-Lesung ersetzen).

---

## 4. Cheap Yellow Display (portable Fernbedienung)

### 4.1 Hardware

- **[ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display)** (ILI9341 + XPT2046-Touch auf vielen Boards).

### 4.2 Display & Touch — **`TFT_eSPI`**

- Library **TFT_eSPI** installieren.
- **User\_Setup** für dein CYD-Board wählen oder aus dem CYD-Repo übernehmen (Pins/Rotation müssen zum Panel passen).

### 4.3 Sketch-Auswahl

| Sketch | Ordner | Beschreibung |
|--------|--------|--------------|
| **Remote mit Touch & Kurven** | `firmware/cyd_remote_control/` | Drei Bereiche: Relais, Werte + Alerts, Mini-Verläufe (`/api/v1/telemetry/series`). |
| **Kompakt** | `firmware/cyd_grow_display/` | Anzeige + Steuerung über API, ohne Touch-Framework im Kommentar beschrieben — weniger UI-Komplexität. |

### 4.4 Zusätzliche Libraries (je nach Sketch)

| Library | Sketches |
|---------|----------|
| **ArduinoJson** v6 | beide |
| **TFT_eSPI** | beide |
| **XPT2046_Touchscreen** | **`cyd_remote_control`** |

### 4.5 Touch kalibrieren (`cyd_remote_control`)

- **`CAL_X_MIN`**, **`CAL_X_MAX`**, **`CAL_Y_MIN`**, **`CAL_Y_MAX`** sowie ggf. **XPT2046-Pins** am Board prüfen; bei Bedarf Kalibrier-Beispiel aus dem CYD-Repository nutzen.

### 4.6 Sensible Werte

Wie Growroom-ESP: **`.env`** + **`python export_firmware_secrets_from_env.py`** → **`device_secrets.h`**. Touch-Kalibrierung (`CAL_*`) bleibt unkritisch im Sketch.

---

## 5. Netzwerk, Firewall, API-Key

- Alle Geräte **im selben LAN** wie der Server (oder Layer-3-Routing bewusst einrichten).
- Am Server: **Telemetry-API** dauerhaft laufen. **Bind, Port, API-Key, CORS:** nur **`.env`**.
- **`TELEMETRY_API_KEY`:** gesetzt = Clients müssen Header mitsenden; leer lassen = ohne Key-Schutz (nur im vertrauenswürdigen LAN sinnvoll).
- Monitoring: **`GET /api/v1/health`**.

---

## 6. Optional: LM358 für EC/pH-Verstärkung

Der **LM358** kann EC- und pH-Sensoren **puffern und verstärken**. Beispiel-Verdrahtung (Komponentenwerte ggf. an deine Sensoren anpassen):

1. **Versorgung**
   - Pin 8 (V+) → **5 V**
   - Pin 4 (V−) → **GND**

2. **EC (erster Verstärker)**
   - EC-Sensorausgang → Pin 2 (inverting input)
   - **10 kΩ** zwischen Pin 2 und Pin 1 (Output)
   - Pin 3 (non-inverting) → Spannungsteiler (z. B. zwei **10 kΩ** zwischen 5 V und GND)
   - Ausgang Pin 1 → **ESP32 ADC** (im Referenzprojekt u. a. **GPIO 34** — nur Eingang, gültige Pins laut deinem Board/Wiring prüfen)

3. **pH (zweiter Verstärker)**
   - pH-Sensorausgang → Pin 6 (inverting input)
   - **10 kΩ** zwischen Pin 6 und Pin 7
   - Pin 5 (non-inverting) → Spannungsteiler wie oben
   - Ausgang Pin 7 → **ESP32 ADC** (z. B. **GPIO 35**)

**Hinweis:** Konkrete GPIO- und Schaltungsdetails mit [wiring_instructions.md](wiring_instructions.md) und deinem Datenblatt abstimmen.

---

## 7. Optional: Absicherung mit Sicherungen

Zum Schutz der Lastkreise **passende Sicherungen** vorschalten (Typ/Nennstrom an deine realen Ströme anpassen):

1. **Hauptversorgung** — z. B. **10 A** träge, vor der Verteilung auf Verbraucher
2. **Heizung** — z. B. **5 A** schnell, auf der Zuleitung zum Heizungsrelais
3. **Wasserpumpe** — z. B. **2 A** schnell, auf der Pumpenzuleitung
4. **Lüfter** — z. B. **1 A** schnell, pro Lüfterkanal nach Leistung
5. **Beleuchtung** — Nennstrom passend zur Lampe (häufig **5–10 A**, je nach Verbrauch)

Sicherungen **leicht erreichbar** montieren (Sicherungshalter), elektrische Arbeit nur fachgerecht/Norm-konform.

---

## Kurz-Checkliste

| Wo | Was |
|----|-----|
| **Server** | Python venv, `pip install -r requirements.txt`, **`.env`** (Port/Key/DB/…), `python run_telemetry_api.py`, optional `streamlit run main.py` |
| **Growroom-ESP** | **ArduinoJson v6**, Sketch `growroom_telemetry_client`; Secrets über **`export_firmware_secrets_from_env.py`** → `device_secrets.h` |
| **CYD** | **TFT_eSPI**, **ArduinoJson v6**, ggf. **XPT2046_Touchscreen**; derselbe Export nach `device_secrets.h` |
| **Netzwerk** | Firewall-Port = **`TELEMETRY_API_PORT`** aus `.env`; API-Key nur in `.env` + generierter Header |
