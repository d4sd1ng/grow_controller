# Project Overview — Growroom Control System

## 1. Projekt-Zweck
Autonome Steuerung eines Indoor-Growrooms mittels ESP32-Mikrocontroller, Sensoren und Relais. Visualisierung und Fernsteuerung über ein Python-basiertes Web-Dashboard. Mobile Überwachung via Cheap Yellow Display (CYD).

## 2. Kern-Komponenten

| Komponente | Technologie | Funktion |
|-----------|-------------|----------|
| **Growroom-ESP** | ESP32-WROOM-32D + SSD1306 OLED | Lokale Steuerzentrale: Sensor-Lesung, Regelung, Relais-Schaltung, OLED-Anzeige, Telemetry-Sendung |
| **CYD** | ESP32-2432S028R (2.8" TFT + Touch) | Mobile Fernbedienung und Systemmonitor mit LVGL v8 |
| **Server** | Python (FastAPI + Streamlit + SQLite) | Dashboard, API, Datenbank, Alert-Management |

## 3. Architektur & Datenfluss

```
+---------------+      HTTP/JSON       +------------------+
|  Sensoren     |  ----------------->  |  Python-Server   |
|  (DHT22, EC,  |                      |  (FastAPI API,   |
|   pH, BH1750) |                      |   Streamlit UI,  |
+---------------+                      |   SQLite DB)     |
       |                               +------------------+
       v                                         ^
+---------------+                               |
|  Growroom-ESP |  <----- Control-State ------- |
|  (Regelung +  |                               |
|   Relais)     |  ------> Telemetry ---------- |
+---------------+                               |
       |                                        |
       v                                        v
+---------------+                      +------------------+
|   Relais      |                      |      CYD         |
|  (Heizung,    |                      |  (LVGL Display,  |
|   Pumpe,      |                      |   Touch-Buttons) |
|   Lüfter)     |                      +------------------+
+---------------+
```

### Ablauf
1. **Sensor-Lesung**: ESP32 liest DHT22 (Temp/RH), EC-Sonde, pH-Sonde, BH1750 (Lux)
2. **Regelung (lokal auf ESP)**: Hysterese-basierte Regelung für Heizung, Lüftung, Pumpe
3. **Relais-Schaltung**: GPIO-Pins schalten SSR-Relais für 230V-Verbraucher
4. **OLED-Anzeige**: SSD1306 zeigt aktuelle Werte und Relais-Status lokal an
5. **Telemetry**: ESP32 sendet Messwerte + Relais-Status an Server (POST /api/v1/telemetry)
6. **Control-State / MANUAL-Override**: Dashboard/CYD schalten Modus und Relais via API. ESP32 holt Zielwerte alle 5s (GET /api/v1/control/state) und führt sie im MANUAL-Modus aus.
7. **Dashboard**: Streamlit zeigt Historie, Charts, Alerts, Steuerung
8. **CYD**: Mobile Zusammenfassung + Relais-Buttons + Charts über HTTP API

## 4. Verzeichnisstruktur

```
grow_controller/
├── .env                          # Secrets (WLAN, API-Key, Telegram)
├── .env.example                  # Vorlage für .env
├── main.py                       # Streamlit Dashboard (UI + Logik)
├── telemetry_api.py              # FastAPI REST API (Endpoints)
├── telemetry_store.py            # SQLite-Datenbankzugriff
├── data_manager.py               # High-Level Daten-Manager
├── controller.py                 # (reserviert für zukünftige Steuerlogik)
├── pid_controller.py             # PID-Regler (Python-Referenz)
├── ml_controller.py              # ML-Anomalie-Erkennung
├── esp32_controller.py           # ESP32-Simulator (Python-Tests)
├── schedule_manager.py           # Zeitpläne für Licht & Co.
├── telegram_bot.py               # Telegram-Bot (optional)
├── telegram_notify.py            # Alert-Versand (aktuell auskommentiert)
├── system_tests.py               # Unit-Tests
├── requirements.txt              # Python-Abhängigkeiten
│
├── firmware/
│   ├── growroom_telemetry_client/
│   │   ├── growroom_telemetry_client.ino   # ESP32 Steuerzentrale + OLED
│   │   └── grow_http_client.h              # HTTP-Helper (GET/POST)
│   │
│   ├── cyd_grow_display/
│   │   ├── cyd_grow_display.ino            # CYD mit LVGL v8
│   │   └── lv_conf.h                       # LVGL-Konfiguration
│   │
│   └── cyd_remote_control/
│       ├── cyd_remote_control.ino          # CYD als Fernbedienung (TFT_eSPI)
│       └── grow_http_client.h              # HTTP-Helper
│
└── docs/                         # (leer — reserved)
```

## 5. API-Endpunkte (FastAPI)

| Methode | Endpoint | Zweck |
|---------|----------|-------|
| GET | `/api/v1/health` | Server-Status (ohne API-Key) |
| POST | `/api/v1/telemetry` | Messwerte empfangen (ESP → Server) |
| GET | `/api/v1/telemetry/series` | Historische Daten abfragen |
| GET | `/api/v1/control/state` | Aktuelle Sollwerte/Relais-Zustände |
| POST | `/api/v1/control/targets` | Relais manuell setzen |
| POST | `/api/v1/control/mode` | AUTO / MANUAL umschalten |
| GET | `/api/v1/mobile/summary` | CYD: Messwerte + Relais + Alerts |
| GET | `/api/v1/alerts/recent` | Letzte Alerts abrufen |

## 6. Hardware-Setup

### Growroom-ESP (ESP32-WROOM-32D mit OLED)
- **WLAN**: eingebautes WiFi
- **I2C**: GPIO21 (SDA), GPIO22 (SCL)
  - SSD1306 OLED @ 0x3C oder 0x3D
  - BH1750 Lichtsensor @ 0x23 oder 0x5C
- **GPIO-Sensoren**:
  - GPIO36 → DHT22 #1 (Luft-Temp/RH)
  - GPIO39 → DHT22 #2 (optional)
  - GPIO34 → EC-Sonde (ADC)
  - GPIO35 → pH-Sonde (ADC)
- **Relais-Ausgänge**:
  - GPIO4 → Heizung (SSR)
  - GPIO5 → Wasserpumpe (SSR)
  - GPIO16 → Zirkulationslüfter (SSR)
  - GPIO17 → Abluftlüfter (SSR)

### CYD (ESP32-2432S028R)
- 2.8" TFT (ILI9341 oder ST7789) über SPI
- XPT2046 Touch über SPI
- SD-Karten-Slot (optional)

## 7. Steuerungs-Modi

| Modus | Beschreibung |
|-------|-------------|
| **AUTO** | ESP32 regelt autonom basierend auf hardcoded Sollwerten (Hysterese). Server dient Logging + Monitoring. |
| **MANUAL** | Relais werden über Dashboard oder CYD via API gesetzt. ESP32 führt Befehle aus. Server-Steuerlogik in `main.py` bleibt aktiv für Simulation/Override. |

## 8. Regel-Logik (Hysterese auf ESP32)

- **Heizung**: AN wenn `T < target - 0.5°C`, AUS wenn `T > target + 0.5°C`
- **Zirkulation**: AN wenn `RH > target + 2%`, AUS wenn `RH < target - 2%`
- **Abluft**: AN wenn `T > target + 2°C` (Notfall-Kühlung)
- **Pumpe**: AN wenn `EC < target - 0.1`, AUS wenn `EC > target + 0.1`

## 9. Sicherheit
- **API-Key**: optionaler `X-Telemetry-Key` Header für alle Endpunkte (außer /health)
- **Secrets**: WLAN-Passwörter, API-Keys, Telegram-Tokens nur in `.env` / `device_secrets.h`
- **Netzwerk**: Für vertrauenswürdiges Heim-LAN konzipiert
- **HTTPS**: optional für Firmware (`IOT_USE_HTTPS=1`)

## 10. Deployment-Optionen
- **Raspberry Pi als lokaler Server**: Empfohlen. API + Dashboard + SQLite + NAS-Backup laufen auf dem Pi. ESPs sprechen direkt den Pi an (feste DHCP-IP).
- **VPS (Hetzner)**: Optional als WireGuard-VPN-Gateway für Fernzugriff. Kein Grow-Traffic über VPS.
- **Tailscale**: Alternative für Fernzugriff ohne Portforwarding.

## 11. Zukünftige Erweiterungen (optional)
- Mehrere ESP32-Geräte (Multi-Device-Support ist vorbereitet)
- ML-basierte Vorhersage für Relais-Zustände
- Setpoints dynamisch vom Server an den ESP32 übertragen
