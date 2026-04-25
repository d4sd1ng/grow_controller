# Installation — Growroom Control System v2.0.0

Diese Anleitung beschreibt **Schritt für Schritt** die Installation aller Komponenten. Keine Optionen, keine Verzweigungen — nur der direkte Weg.

Die Reihenfolge ist zwingend:
1. Server aufsetzen
2. `.env` konfigurieren
3. ESP32 flashen
4. System testen

> **Wichtig:** Alle sensiblen Werte (WLAN, API-Keys, Telegram) kommen ausschließlich in `.env`. `.env` ist in `.gitignore` und wird nie ins Repository gepusht.

---

## Schritt 1: Server vorbereiten

### 1.1 Systemvoraussetzungen prüfen

| Voraussetzung | Minimum | Prüfen |
|---------------|---------|--------|
| Python | 3.11 | `python --version` |
| Git | installiert | `git --version` |
| Arbeitsspeicher | 1 GB RAM | — |
| Speicherplatz | 2 GB frei | — |

### 1.2 Repository klonen

```bash
cd ~
git clone https://github.com/d4sd1ng/grow_controller.git
cd grow_controller
```

### 1.3 Virtuelle Umgebung erstellen

```bash
python -m venv .venv
```

### 1.4 Virtuelle Umgebung aktivieren

**Linux/macOS:**
```bash
source .venv/bin/activate
```

**Windows (PowerShell):**
```powershell
.\.venv\Scripts\Activate.ps1
```

> Ab jetzt arbeitest du **immer** in der venv. Der Prompt zeigt `(.venv)`.

### 1.5 Python-Abhängigkeiten installieren

```bash
pip install -r requirements.txt
```

Dauert ca. 2–3 Minuten. Installiert: Streamlit, FastAPI, Uvicorn, Pandas, Plotly, scikit-learn.

### 1.6 Test ausführen

```bash
python -m unittest system_tests test_extended
```

Erwartetes Ergebnis: `OK` (alle 28 Tests bestanden).

Wenn Tests fehlschlagen: Abhängigkeiten nochmals prüfen (`pip list`), dann Issue erstellen.

---

## Schritt 2: Konfiguration (`.env`)

### 2.1 `.env` aus Vorlage erstellen

```bash
cp .env.example .env
```

### 2.2 `.env` bearbeiten

```bash
nano .env    # Linux/macOS
notepad .env # Windows
```

**Folgende Variablen müssen gesetzt werden:**

| Variable | Beispielwert | Beschreibung |
|----------|--------------|--------------|
| `DASHBOARD_PASSWORD` | `MeinSicheresPW123` | Login-Passwort für das Web-Dashboard |
| `TELEMETRY_API_KEY` | `aB3cD4eF5gH6iJ7kL8mN9oP0qR` | Gemeinsamer Schlüssel zwischen Server und ESP32 |
| `IOT_WIFI_SSID` | `MeinHeimWLAN` | WLAN-Name für alle ESP32-Geräte |
| `IOT_WIFI_PASSWORD` | `wlanPasswort2024` | WLAN-Passwort für alle ESP32-Geräte |
| `IOT_TELEMETRY_SERVER_HOST` | `192.168.178.50` | **Feste IP** des Servers im Heimnetz |

**Folgende Variablen können auf Standardwerten bleiben:**

| Variable | Standardwert | Bedeutung |
|----------|--------------|-----------|
| `TELEMETRY_API_PORT` | `8765` | Port der Telemetry-API |
| `TELEMETRY_API_HOST` | `0.0.0.0` | API lauscht auf allen Interfaces |
| `GROW_DATA_DB` | `data/grow_telemetry.db` | SQLite-Datenbank-Pfad |
| `GROW_DATA_RETENTION_DAYS` | `90` | Daten älter als 90 Tage werden gelöscht |

**Telegram (optional):**

| Variable | Beispielwert |
|----------|--------------|
| `TELEGRAM_BOT_TOKEN` | `123456789:ABCdefGHIjklMNOpqrSTUvwxyz` |
| `TELEGRAM_CHAT_ID` | `987654321` |

> **WLAN-Router:** Stelle sicher, dass der Server eine **feste IP** hat (DHCP-Reservierung im Router). Die ESP32-Geräte können sich nicht an eine sich ändernde IP-Adresse binden.

### 2.3 `device_secrets.h` generieren

Nachdem `.env` gespeichert ist:

```bash
python export_firmware_secrets_from_env.py
```

Erwartete Ausgabe:
```
OK: E:\Projects\grow_controller\firmware\growroom_telemetry_client\device_secrets.h
OK: E:\Projects\grow_controller\firmware\cyd_remote_control\device_secrets.h
OK: E:\Projects\grow_controller\firmware\cyd_grow_display\device_secrets.h
```

Diese Datei enthält:
- WLAN-SSID und Passwort
- Server-Host und Port
- Telemetry-API-Key
- HTTPS-Flag (für VPS-Betrieb)

> **Niemals `device_secrets.h` committen!** Sie ist in `.gitignore`.

---

## Schritt 3: Telemetry-API starten

### 3.1 Server starten

```bash
python run_telemetry_api.py
```

Erwartete Ausgabe:
```
INFO:     Uvicorn running on http://0.0.0.0:8765 (Press CTRL+C to quit)
```

### 3.2 Health-Check

In einem zweiten Terminal (venv aktiv):

```bash
curl http://localhost:8765/api/v1/health
```

Erwartetes Ergebnis:
```json
{"ok":true,"db":"connected","version":"2.0.0"}
```

Wenn das nicht kommt: `.env`-Werte prüfen, Firewall prüfen.

### 3.3 Firewall-Regel (einmalig)

**Windows:**
```powershell
# PowerShell als Administrator
New-NetFirewallRule -DisplayName "Grow Telemetry API" -Direction Inbound -Protocol TCP -LocalPort 8765 -Action Allow
```

**Linux (ufw):**
```bash
sudo ufw allow 8765/tcp
```

---

## Schritt 4: ESP32 flashen

### 4.1 Entwicklungsumgebung

- **Arduino IDE 2.x** installieren
- Board-Paket **ESP32 by Espressif Systems** installieren (Version 2.x)
- Board: **ESP32 Dev Module**

### 4.2 Libraries installieren

Im Arduino IDE Library Manager:

| Library | Version | Suchbegriff |
|---------|---------|-------------|
| ArduinoJson | 6.x | `ArduinoJson` |

### 4.3 Sketch öffnen

```
File → Open → firmware/growroom_telemetry_client/growroom_telemetry_client.ino
```

> Wichtig: Die `.ino`-Datei muss **denselben Namen** wie der Ordner haben.

### 4.4 Prüfen: `device_secrets.h` vorhanden

Im Sketch-Ordner muss `device_secrets.h` liegen (generiert in Schritt 2.3).

Wenn nicht vorhanden, nochmals ausführen:
```bash
python export_firmware_secrets_from_env.py
```

### 4.5 Seriellen Monitor vorbereiten

```
Tools → Serial Monitor → Baudrate: 115200
```

### 4.6 Hochladen

```
Sketch → Upload (Ctrl+U)
```

Nach dem Upload sollte der Serielle Monitor anzeigen:
```
WiFi......
Connected: 192.168.178.101
POST telemetry -> 200
```

Wenn `POST telemetry -> 200` erscheint, ist die Verbindung zum Server erfolgreich.

### 4.7 Fehlerbehebung

| Problem | Ursache | Lösung |
|---------|---------|--------|
| `WiFi.....` endlos | Falsche WLAN-Daten | `.env` korrigieren, `device_secrets.h` neu generieren |
| `POST telemetry -> 0` | Server nicht erreichbar | Server-IP prüfen, Firewall prüfen, API-Port prüfen |
| `POST telemetry -> 401` | API-Key falsch | `.env` prüfen, `device_secrets.h` neu generieren |
| `POST telemetry -> 404` | URL falsch | `IOT_TELEMETRY_SERVER_HOST` prüfen |

---

## Schritt 5: Dashboard starten (optional, empfohlen)

### 5.1 Neues Terminal öffnen

```bash
# venv aktivieren
source .venv/bin/activate   # Linux/macOS
.\.venv\Scripts\activate   # Windows

# Dashboard starten
streamlit run main.py
```

### 5.2 Öffnen im Browser

```
http://localhost:8501
```

Login:
- Username: `admin`
- Password: `DASHBOARD_PASSWORD` aus `.env`

### 5.3 Dashboard-Funktionen

| Tab | Funktion |
|-----|----------|
| **Übersicht** | Live-Sensorwerte, VPD-Anzeige, Relais-Status |
| **Charts** | Historische Verläufe (24h, 7d, 30d) |
| **Steuerung** | Relais manuell schalten, Sollwerte setzen |
| **Devices** | ESP32-Geräte verwalten |
| **ML** | Modell trainieren, VPD/EC/pH-Optimierung |
| **Export** | CSV/JSON-Daten exportieren |

---

## Schritt 6: CYD (Cheap Yellow Display) — optional

### 6.1 Hardware

- ESP32-CYD-Board mit ILI9341 + XPT2046 Touch

### 6.2 Libraries

| Library | Version |
|---------|---------|
| ArduinoJson | 6.x |
| TFT_eSPI | 2.x |
| XPT2046_Touchscreen | 1.x |

### 6.3 TFT_eSPI konfigurieren

Vor dem ersten Build: `User_Setup.h` für CYD anpassen oder die Setup-Datei aus dem CYD-Repository kopieren.

### 6.4 Sketch auswählen

| Anwendungsfall | Sketch | Ordner |
|----------------|--------|--------|
| Touch + Charts | Remote Control | `firmware/cyd_remote_control/` |
| Nur Anzeige | Grow Display | `firmware/cyd_grow_display/` |

### 6.5 Flashen

```
File → Open → firmware/cyd_remote_control/cyd_remote_control.ino
Sketch → Upload
```

`device_secrets.h` wird automatisch aus demselben `.env` generiert (Schritt 2.3).

---

## Zusammenfassung — Schnellreferenz

| Aufgabe | Befehl |
|---------|--------|
| Repo klonen | `git clone https://github.com/d4sd1ng/grow_controller.git` |
| venv erstellen | `python -m venv .venv` |
| venv aktivieren (Linux) | `source .venv/bin/activate` |
| venv aktivieren (Windows) | `.\.venv\Scripts\Activate.ps1` |
| Abhängigkeiten | `pip install -r requirements.txt` |
| Tests | `python -m unittest system_tests test_extended` |
| `.env` erstellen | `cp .env.example .env` |
| Secrets exportieren | `python export_firmware_secrets_from_env.py` |
| API starten | `python run_telemetry_api.py` |
| Health-Check | `curl http://localhost:8765/api/v1/health` |
| Dashboard starten | `streamlit run main.py` |
| Firewall (Windows) | `New-NetFirewallRule -DisplayName "Grow Telemetry API" -Direction Inbound -Protocol TCP -LocalPort 8765 -Action Allow` |
| Firewall (Linux) | `sudo ufw allow 8765/tcp` |

---

## Schnell-Checkliste

### Server:
- [ ] Python 3.11+ installiert
- [ ] Repository geklont
- [ ] venv erstellt und aktiviert
- [ ] `pip install -r requirements.txt` erfolgreich
- [ ] Alle 28 Tests bestanden
- [ ] `.env` erstellt und alle Werte gesetzt
- [ ] `device_secrets.h` generiert
- [ ] Telemetry-API gestartet, Health-Check `{"ok":true}`
- [ ] Firewall-Port 8765 geöffnet
- [ ] Dashboard läuft, Login funktioniert

### ESP32:
- [ ] Arduino IDE + ESP32-Board-Paket installiert
- [ ] ArduinoJson v6 installiert
- [ ] Sketch `growroom_telemetry_client.ino` geöffnet
- [ ] `device_secrets.h` im Sketch-Ordner vorhanden
- [ ] Board: ESP32 Dev Module, Port korrekt
- [ ] Upload erfolgreich
- [ ] Serieller Monitor zeigt `Connected: <IP>`
- [ ] Serieller Monitor zeigt `POST telemetry -> 200`

### Netzwerk:
- [ ] Server hat feste IP (Router: DHCP-Reservierung)
- [ ] ESP32 und Server im selben WLAN
- [ ] `IOT_TELEMETRY_SERVER_HOST` = Server-IP
