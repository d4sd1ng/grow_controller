# Deployment auf Raspberry Pi v2.0.0

Diese Anleitung beschreibt den **vollständigen** Aufbau auf einem Raspberry Pi. Der Pi wird als permanenter Server betrieben — 24/7, mit Auto-Start nach Stromausfall.

**Architektur:**

```
                    +------------------+
                    |  Dein PC/Tablet  |
                    |  (Browser)       |
                    +--------+---------+
                             |
                    WiFi/Ethernet
                             |
                    +--------v---------+
                    |  Raspberry Pi    |
                    |  192.168.178.50  |
                    |                  |
                    |  :8765 Telemetry |
                    |  :8501 Dashboard |
                    |  SQLite DB       |
                    +--------+---------+
                             |
                    WiFi (2.4 GHz)
                             |
         +-------------------+-------------------+
         |                   |                   |
+--------v--------+ +--------v--------+ +--------v--------+
|  ESP32 Sensor   | |  ESP32 Relays   | |  CYD Display    |
|  192.168.178.101| |  192.168.178.102| |  192.168.178.103|
+-----------------+ +-----------------+ +-----------------+
```

> **Voraussetzung:** Pi hat feste IP im Router (DHCP-Reservierung). Alle Geräte im selben 2.4-GHz-WLAN.

---

## Schritt 1: Raspberry Pi vorbereiten

### 1.1 Betriebssystem

- **Raspberry Pi OS Lite (64-bit)** oder **Ubuntu Server 22.04/24.04**
- SSH aktiviert
- Netzwerk konfiguriert (feste IP empfohlen)

### 1.2 System aktualisieren

```bash
sudo apt update && sudo apt upgrade -y
```

### 1.3 Notwendige Pakete installieren

```bash
sudo apt install -y python3 python3-pip python3-venv git curl
```

### 1.4 Python-Version prüfen

```bash
python3 --version
```

Mindestens Python 3.11. Falls älter:
```bash
sudo apt install -y python3.11 python3.11-venv
```

---

## Schritt 2: Repository einrichten

### 2.1 Klonen

```bash
cd ~
git clone https://github.com/d4sd1ng/grow_controller.git
cd grow_controller
```

### 2.2 Virtuelle Umgebung

```bash
python3 -m venv .venv
source .venv/bin/activate
```

### 2.3 Abhängigkeiten

```bash
pip install --upgrade pip
pip install -r requirements.txt
```

Dauert ca. 5 Minuten auf einem Pi 4, länger auf Pi 3.

### 2.4 Tests

```bash
python -m unittest system_tests test_extended
```

Erwartet: `OK`

---

## Schritt 3: Konfiguration

### 3.1 `.env` erstellen

```bash
cp .env.example .env
nano .env
```

**Pflicht-Werte für Pi-Betrieb:**

```env
# Server-Konfiguration
TELEMETRY_API_HOST=0.0.0.0
TELEMETRY_API_PORT=8765
TELEMETRY_API_KEY=ErsetzeDurch32ZeichenRandomString

# Dashboard
DASHBOARD_PASSWORD=MeinSicheresPW123

# WLAN für ESP32-Geräte
IOT_WIFI_SSID=MeinHeimWLAN
IOT_WIFI_PASSWORD=WLANpasswort2024
IOT_TELEMETRY_SERVER_HOST=192.168.178.50

# Datenbank
GROW_DATA_DB=/home/pi/grow_controller/data/grow_telemetry.db
```

> `IOT_TELEMETRY_SERVER_HOST` muss die **tatsächliche IP des Pi** im Heimnetz sein. Nach der Einrichtung nicht mehr ändern, sonst müssen alle ESP32 neu geflasht werden.

### 3.2 Datenbank-Verzeichnis

```bash
mkdir -p data
```

### 3.3 Secrets exportieren

```bash
python export_firmware_secrets_from_env.py
```

Ausgabe muss drei `OK:`-Zeilen zeigen.

---

## Schritt 4: systemd-Services einrichten

systemd startet die Services automatisch beim Booten und startet sie neu wenn sie abstürzen.

### 4.1 Telemetry-API Service

Erstelle die Datei:

```bash
sudo nano /etc/systemd/system/grow-telemetry.service
```

Inhalt:
```ini
[Unit]
Description=Grow Controller Telemetry API
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/grow_controller
Environment=PATH=/home/pi/grow_controller/.venv/bin
ExecStart=/home/pi/grow_controller/.venv/bin/python /home/pi/grow_controller/run_telemetry_api.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

> **Anpassen:** `User=pi` und Pfade auf deinen tatsächlichen Benutzernamen und Home-Verzeichnis ändern.

### 4.2 Dashboard Service

```bash
sudo nano /etc/systemd/system/grow-dashboard.service
```

Inhalt:
```ini
[Unit]
Description=Grow Controller Dashboard
After=network-online.target grow-telemetry.service
Wants=network-online.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/grow_controller
Environment=PATH=/home/pi/grow_controller/.venv/bin
ExecStart=/home/pi/grow_controller/.venv/bin/streamlit run /home/pi/grow_controller/main.py --server.headless=true --server.port=8501 --server.address=0.0.0.0
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### 4.3 Services aktivieren

```bash
sudo systemctl daemon-reload
sudo systemctl enable grow-telemetry.service
sudo systemctl enable grow-dashboard.service
sudo systemctl start grow-telemetry.service
sudo systemctl start grow-dashboard.service
```

### 4.4 Status prüfen

```bash
sudo systemctl status grow-telemetry.service
sudo systemctl status grow-dashboard.service
```

Beide müssen `active (running)` anzeigen.

### 4.5 Logs ansehen

```bash
# Telemetry-API Logs
sudo journalctl -u grow-telemetry.service -f

# Dashboard Logs
sudo journalctl -u grow-dashboard.service -f
```

---

## Schritt 5: Firewall

### 5.1 UFW (Ubuntu/Raspberry Pi OS)

```bash
sudo ufw allow 8765/tcp comment 'Grow Telemetry API'
sudo ufw allow 8501/tcp comment 'Grow Dashboard'
sudo ufw allow 22/tcp comment 'SSH'
sudo ufw enable
```

Prüfen:
```bash
sudo ufw status
```

Erwartete Ausgabe:
```
To                         Action      From
--                         ------      ----
8765/tcp                   ALLOW       Anywhere
8501/tcp                   ALLOW       Anywhere
22/tcp                     ALLOW       Anywhere
```

> **Wichtig:** Port 8765 und 8501 nur im LAN freigeben. Nicht ins Internet weiterleiten (keine Port-Forwarding-Regel im Router), solange nur Heimgeräte zugreifen.

---

## Schritt 6: ESP32 flashen

### 6.1 Auf dem PC (nicht auf dem Pi)

`.env` kopieren oder die Werte notieren:
- `IOT_WIFI_SSID`
- `IOT_WIFI_PASSWORD`
- `IOT_TELEMETRY_SERVER_HOST` (Pi-IP)
- `TELEMETRY_API_KEY`

### 6.2 Arduino IDE

1. `firmware/growroom_telemetry_client/growroom_telemetry_client.ino` öffnen
2. `device_secrets.h` muss vorhanden sein (wurde in Schritt 3.3 generiert)
3. Board: ESP32 Dev Module
4. Upload

### 6.3 Serieller Monitor

Nach dem Upload:
```
WiFi.....
Connected: 192.168.178.101
POST telemetry -> 200
```

`POST telemetry -> 200` bedeutet: ESP32 erreicht den Pi-Server.

---

## Schritt 7: Zugriff testen

### 7.1 Vom PC im Browser

```
http://192.168.178.50:8501
```

Login mit:
- Username: `admin`
- Password: `DASHBOARD_PASSWORD` aus `.env`

### 7.2 API Health-Check

```bash
curl http://192.168.178.50:8765/api/v1/health
```

Erwartet:
```json
{"ok":true,"db":"connected","version":"2.0.0"}
```

### 7.3 API-Dokumentation

```
http://192.168.178.50:8765/docs
```

Interaktive Swagger-UI mit allen Endpunkten.

---

## Schritt 8: Backup einrichten

### 8.1 NAS mounten (optional)

```bash
# Einmalig: Mount-Punkt erstellen
sudo mkdir -p /mnt/nas_backup

# Einmalig: fstab-Eintrag
sudo nano /etc/fstab
# Zeile hinzufügen:
//192.168.178.10/backup /mnt/nas_backup cifs credentials=/home/pi/.smbcredentials,iocharset=utf8,sec=ntlmssp 0 0
```

### 8.2 Backup-Script anpassen

`grow_backup.sh` im Repo-Root anpassen:

```bash
nano grow_backup.sh
```

`DB_SOURCE` auf den tatsächlichen Pfad setzen:
```bash
DB_SOURCE="/home/pi/grow_controller/data/grow_telemetry.db"
BACKUP_DIR="/mnt/nas_backup/grow_controller"
```

### 8.3 Cron-Job

```bash
crontab -e
```

Zeile hinzufügen (täglich um 03:00 Uhr):
```
0 3 * * * /home/pi/grow_controller/grow_backup.sh >> /home/pi/grow_controller/logs/backup.log 2>&1
```

Logs-Verzeichnis erstellen:
```bash
mkdir -p logs
```

---

## Wartung

### Services neu starten

```bash
sudo systemctl restart grow-telemetry.service
sudo systemctl restart grow-dashboard.service
```

### Update nach Git-Änderung

```bash
cd ~/grow_controller
git pull origin main
source .venv/bin/activate
pip install -r requirements.txt
sudo systemctl restart grow-telemetry.service
sudo systemctl restart grow-dashboard.service
```

### Speicherplatz prüfen

```bash
df -h
du -sh data/
```

Die Datenbank wächst mit ca. 5–10 MB pro Woche (bei 10-Sekunden-Intervall).

---

## Fehlersuche

| Problem | Diagnose | Lösung |
|---------|----------|--------|
| Service startet nicht | `sudo journalctl -u grow-telemetry.service -n 50` | `.env` prüfen, Pfade prüfen |
| ESP32: `POST -> 0` | `curl http://<pi-ip>:8765/api/v1/health` | Firewall, IP, Port prüfen |
| Dashboard nicht erreichbar | `sudo systemctl status grow-dashboard` | Port 8501 prüfen, Firewall |
| Datenbank sehr groß | `du -sh data/grow_telemetry.db` | Retention prüfen (`GROW_DATA_RETENTION_DAYS`) |
| Pi hängt sich auf | `htop` | RAM prüfen, ggf. Swap erhöhen |

---

## Schnell-Checkliste

- [ ] Raspberry Pi OS installiert, SSH aktiv
- [ ] Feste IP im Router reserviert
- [ ] Python 3.11+ installiert
- [ ] Repository geklont nach `~/grow_controller`
- [ ] venv erstellt, Dependencies installiert
- [ ] Alle 28 Tests bestanden
- [ ] `.env` erstellt mit korrekten Werten
- [ ] `device_secrets.h` generiert
- [ ] systemd-Services erstellt und aktiviert
- [ ] Firewall-Ports 8765 und 8501 geöffnet
- [ ] Services laufen (`systemctl status` zeigt `active`)
- [ ] Health-Check vom PC aus erfolgreich
- [ ] Dashboard im Browser erreichbar
- [ ] ESP32 geflasht, zeigt `POST telemetry -> 200`
- [ ] Backup-Script konfiguriert (optional)
- [ ] Cron-Job für Backup eingerichtet (optional)
