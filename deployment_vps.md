> **Firmwarestand 13.11.2026:** Der Sensor-Sketch ist jetzt `firmware/Sensor/Sensor.ino` (siehe [Firmware-README](firmware/README.md)).


# Deployment auf Raspberry Pi v2.0.0 — Anleitung für Anfänger

Diese Anleitung erklärt, wie du das Growroom-System auf einem **Raspberry Pi** betreibst. Der Pi läuft 24/7 und startet die Programme automatisch nach einem Stromausfall neu.

**Was du am Ende hast:** Ein kleiner Computer (Raspberry Pi), der ständig läuft, Sensordaten von ESP32-Geräten sammelt und ein Web-Dashboard bereitstellt. Du kannst von jedem Gerät im Heimnetz darauf zugreifen.

---

## Was ist ein Raspberry Pi?

Ein Raspberry Pi ist ein kleiner Computer, so groß wie eine Kreditkarte. Er verbraucht wenig Strom, macht kaum Geräusch und kann tagelang durchlaufen. Perfekt für ein Growroom-System, das 24 Stunden am Tag laufen soll.

**Modelle:**
- **Raspberry Pi 4** (empfohlen) — schnell, 2–8 GB RAM
- **Raspberry Pi 3** — langsamer, aber ausreichend
- **Raspberry Pi 5** — der Neueste, etwas teurer

**Was du brauchst:**
- Raspberry Pi (mit Gehäuse empfohlen)
- Micro-SD-Karte (mindestens 16 GB, besser 32 GB)
- USB-C-Netzteil (offizielles Raspberry-Pi-Netzteil empfohlen)
- Ethernet-Kabel oder WLAN-Verbindung

---

## Schritt 1: Raspberry Pi vorbereiten

### 1.1 Betriebssystem auf die SD-Karte schreiben

1. Lade **Raspberry Pi Imager** herunter: https://www.raspberrypi.com/software/
2. Installiere und starte das Programm
3. Wähle dein Raspberry-Pi-Modell
4. Wähle **Raspberry Pi OS Lite (64-bit)** — das ist ohne grafische Oberfläche, spart Ressourcen
5. Wähle deine Micro-SD-Karte
6. Klicke "Weiter" und dann "Bearbeiten" (Zahnradsymbol), um Einstellungen vorzunehmen:
   - Setze einen Haken bei "SSH aktivieren"
   - Setze einen Benutzernamen und ein Passwort (z.B. Benutzer: `pi`, Passwort: dein Passwort)
   - Gib deine WLAN-Daten ein (damit der Pi sich automatisch verbindet)
7. Klicke "Speichern" und dann "Ja"
8. Warte, bis der Vorgang abgeschlossen ist

### 1.2 Erster Start

1. Stecke die Micro-SD-Karte in den Pi
2. Verbinde das Ethernet-Kabel (empfohlen) oder stelle sicher, dass WLAN konfiguriert ist
3. Schließe das Netzteil an
4. Der Pi startet automatisch (LED leuchtet)

### 1.3 Auf den Pi zugreifen (von deinem PC)

**Über SSH (Terminal-Verbindung):**

Öffne auf deinem PC die Eingabeaufforderung (cmd) oder PowerShell:

```bash
ssh pi@raspberrypi.local
```

> **Was ist SSH?** "Secure Shell" = eine verschlüsselte Verbindung zu einem anderen Computer. Damit kannst du den Pi von deinem PC aus steuern.

Wenn das nicht funktioniert, finde die IP-Adresse des Pi:

1. Logge dich in deinen Router ein (meist http://192.168.178.1 oder http://fritz.box)
2. Suche in der Geräteliste nach "raspberrypi"
3. Notiere die IP-Adresse (z.B. `192.168.178.50`)
4. Verbinde dich:
   ```bash
   ssh pi@192.168.178.50
   ```

Gib das Passwort ein, das du beim Imager festgelegt hast.

> **Wichtig:** Wenn du zum ersten Mal verbindest, fragt SSH, ob du dem Computer vertraust. Tippe `yes` und Enter.

### 1.4 System aktualisieren

Sobald du auf dem Pi eingeloggt bist (du siehst `pi@raspberrypi:~$`):

```bash
sudo apt update && sudo apt upgrade -y
```

Das aktualisiert alle Programme. Dauert 5–15 Minuten.

> **Was ist `sudo`?** "Superuser Do" = führe diesen Befehl mit Administrator-Rechten aus. Du wirst nach deinem Passwort gefragt.

> **Was ist `apt`?** Der Paketmanager von Debian/Raspberry Pi OS. Installiert und aktualisiert Programme.

### 1.5 Feste IP-Adresse einrichten

Der Pi braucht immer dieselbe IP-Adresse, sonst finden die ESP32-Geräte ihn nicht mehr.

**Im Router (empfohlen):**

1. Öffne deinen Router (http://fritz.box oder http://192.168.178.1)
2. Suche nach "DHCP-Reservierung" oder "Feste IP"
3. Wähle den Raspberry Pi aus der Geräteliste
4. Weise ihm eine feste IP zu (z.B. `192.168.178.50`)

**Alternativ direkt am Pi:**

```bash
sudo nano /etc/dhcpcd.conf
```

Füge am Ende hinzu:
```
interface eth0
static ip_address=192.168.178.50/24
static routers=192.168.178.1
static domain_name_servers=192.168.178.1
```

> **Was bedeuten diese Zahlen?**
> - `192.168.178.50` = Die feste IP deines Pi
> - `/24` = Die Subnetzmaske (für Heimnetze fast immer /24)
> - `192.168.178.1` = Dein Router (die "1" am Ende ist fast immer der Router)

Speichern mit `Strg+O`, Enter, dann `Strg+X` zum Beenden.

Neustart:
```bash
sudo reboot
```

Warte 1 Minute, dann wieder verbinden:
```bash
ssh pi@192.168.178.50
```

---

## Schritt 2: Notwendige Programme installieren

Auf dem Pi (über SSH) eingeloggt:

```bash
sudo apt install -y python3 python3-pip python3-venv git curl
```

Das installiert:
- **Python 3** = die Programmiersprache
- **pip** = Python-Paketmanager
- **venv** = Virtuelle Umgebungen erstellen
- **Git** = Code herunterladen
- **curl** = Webseiten im Terminal testen

Prüfe die Python-Version:
```bash
python3 --version
```

Mindestens Python 3.11. Wenn älter:
```bash
sudo apt install -y python3.11 python3.11-venv
```

---

## Schritt 3: Das Projekt herunterladen

```bash
cd ~
git clone https://github.com/d4sd1ng/grow_controller.git
cd grow_controller
```

---

## Schritt 4: Virtuelle Umgebung einrichten

```bash
python3 -m venv .venv
source .venv/bin/activate
```

> **Was ist `source`?** Ein Befehl, der ein Skript ausführt und dabei die aktuelle Umgebung ändert. Hier: Die virtuelle Umgebung wird aktiviert.

Du siehst jetzt `(.venv)` vor deinem Prompt.

### Python-Bibliotheken installieren

```bash
pip install --upgrade pip
pip install -r requirements.txt
```

Dauert auf einem Pi 4 ca. 5 Minuten, auf einem Pi 3 länger.

### Test

```bash
python -m unittest system_tests test_extended
```

Erwartet: `OK`

---

## Schritt 5: Konfiguration

### 5.1 .env erstellen

```bash
cp .env.example .env
nano .env
```

**Folgende Werte musst du ändern:**

```env
# Dashboard-Passwort (ändern!)
DASHBOARD_PASSWORD=MeinSicheresPasswort123

# Geheimer Schlüssel (32 zufällige Zeichen)
TELEMETRY_API_KEY=abcdefghijklmnopqrstuvwxyz123456

# Dein WLAN (gleiches wie im Router)
IOT_WIFI_SSID=MeinHeimWLAN
IOT_WIFI_PASSWORD=MeinWLANPasswort2024

# Die IP-Adresse dieses Raspberry Pi!
IOT_TELEMETRY_SERVER_HOST=192.168.178.50

# Datenbank (bleibt auf dem Pi)
GROW_DATA_DB=/home/pi/grow_controller/data/grow_telemetry.db
```

> **Wichtig:** `IOT_TELEMETRY_SERVER_HOST` muss die **IP-Adresse des Pi** sein (die du in Schritt 1.4 festgelegt hast).

Speichern: `Strg+O`, Enter, `Strg+X`

### 5.2 Datenbank-Verzeichnis erstellen

```bash
mkdir -p data
```

### 5.3 Secrets exportieren

```bash
python export_firmware_secrets_from_env.py
```

Sollte drei "OK:"-Zeilen anzeigen.

---

## Schritt 6: Automatischer Start einrichten

### Was ist systemd?

**systemd** ist ein Programm, das auf Linux-Systemen andere Programme verwaltet. Wir nutzen es, damit unsere Growroom-Software automatisch startet, wenn der Pi hochfährt.

### 6.1 Telemetry API als Service einrichten

Erstelle eine neue Service-Datei:

```bash
sudo nano /etc/systemd/system/grow-telemetry.service
```

Füge folgenden Text ein:

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

> **Was bedeutet das?**
> - `Description` = Name des Dienstes
> - `After=network-online.target` = Starte erst, wenn das Netzwerk bereit ist
> - `User=pi` = Läuft unter dem Benutzer "pi"
> - `Restart=always` = Wenn das Programm abstürzt, startet es neu
> - `RestartSec=10` = Warte 10 Sekunden vor dem Neustart

Speichern: `Strg+O`, Enter, `Strg+X`

### 6.2 Dashboard als Service einrichten

```bash
sudo nano /etc/systemd/system/grow-dashboard.service
```

Füge ein:

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

Speichern: `Strg+O`, Enter, `Strg+X`

### 6.3 Services aktivieren

```bash
sudo systemctl daemon-reload
sudo systemctl enable grow-telemetry.service
sudo systemctl enable grow-dashboard.service
sudo systemctl start grow-telemetry.service
sudo systemctl start grow-dashboard.service
```

> **Was machen diese Befehle?**
> - `daemon-reload` = systemd neu laden, damit es die neuen Dateien erkennt
> - `enable` = Starte diesen Service automatisch beim Booten
> - `start` = Starte den Service jetzt sofort

### 6.4 Prüfen, ob es läuft

```bash
sudo systemctl status grow-telemetry.service
```

Du solltest sehen:
```
● grow-telemetry.service - Grow Controller Telemetry API
     Loaded: loaded (/etc/systemd/system/grow-telemetry.service; enabled)
     Active: active (running) since ...
```

Wenn dort `active (running)` steht: Perfekt!

Gleich für das Dashboard:
```bash
sudo systemctl status grow-dashboard.service
```

### 6.5 Logs ansehen (wenn etwas nicht klappt)

```bash
# Letzte 50 Zeilen der Telemetry-API
sudo journalctl -u grow-telemetry.service -n 50

# Live-Logs (laufen weiter, beenden mit Strg+C)
sudo journalctl -u grow-telemetry.service -f

# Gleich für Dashboard
sudo journalctl -u grow-dashboard.service -n 50
```

---

## Schritt 7: Firewall öffnen

Die Firewall des Pi blockiert eingehende Verbindungen standardmäßig. Du musst die Ports öffnen.

```bash
sudo apt install -y ufw
sudo ufw allow 8765/tcp comment 'Growroom Telemetry API'
sudo ufw allow 8501/tcp comment 'Growroom Dashboard'
sudo ufw allow 22/tcp comment 'SSH'
sudo ufw enable
```

> **Was ist ufw?** "Uncomplicated Firewall" = eine einfache Firewall für Linux.

Prüfe:
```bash
sudo ufw status
```

Sollte anzeigen:
```
To                         Action      From
--                         ------      ----
8765/tcp                   ALLOW       Anywhere
8501/tcp                   ALLOW       Anywhere
22/tcp                     ALLOW       Anywhere
```

> **Wichtig:** Öffne diese Ports **nicht** in deinem Router (kein Port-Forwarding). Sie sollen nur im lokalen Netzwerk erreichbar sein.

---

## Schritt 8: ESP32 flashen

Jetzt musst du den ESP32 mit den Daten des Pi konfigurieren.

### 8.1 Auf deinem PC (nicht auf dem Pi)

Gehe zurück zu deinem PC. Die `.env`-Datei auf dem Pi enthält die richtigen Werte.

Notiere dir diese Werte vom Pi:
- `IOT_WIFI_SSID`
- `IOT_WIFI_PASSWORD`
- `IOT_TELEMETRY_SERVER_HOST` (die Pi-IP)
- `TELEMETRY_API_KEY`

Oder kopiere die `.env`-Datei vom Pi auf deinen PC:

```bash
# Auf deinem PC (PowerShell / cmd)
scp pi@192.168.178.50:~/grow_controller/.env C:\Users\DeinName\Documents\grow_controller\.env
```

> **Was ist `scp`?** "Secure Copy" = kopiert Dateien über eine verschlüsselte Verbindung.

### 8.2 Arduino IDE

1. Öffne Arduino IDE auf deinem PC
2. Öffne den Sketch: `firmware/Sensor/Sensor.ino`
3. Prüfe, ob `device_secrets.h` im gleichen Ordner liegt
4. Stelle sicher, dass die Werte in `device_secrets.h` stimmen:
   - `SERVER_HOST` = IP des Pi (z.B. `192.168.178.50`)
   - `SERVER_PORT` = `8765`
   - `TELEMETRY_API_KEY` = dein API-Key
5. Board: ESP32 Dev Module
6. Upload

### 8.3 Test

Serieller Monitor (115200 Baud):
```
WiFi......
Connected: 192.168.178.101
POST telemetry -> 200
```

`200` bedeutet: Der ESP32 hat erfolgreich Daten an den Pi geschickt!

---

## Schritt 9: Zugriff testen

### Vom PC im Browser

```
http://192.168.178.50:8501
```

(ersetze die IP durch die IP deines Pi)

Login:
- Username: `admin`
- Password: dein `DASHBOARD_PASSWORD` aus `.env`

### API-Test

```bash
curl http://192.168.178.50:8765/api/v1/health
```

Sollte anzeigen:
```json
{"ok":true,"db":"connected","version":"2.0.0"}
```

### API-Dokumentation

```
http://192.168.178.50:8765/docs
```

Hier siehst du alle verfügbaren Funktionen der API.

---

## Schritt 10: Backup einrichten (optional, empfohlen)

### NAS mounten (wenn du ein Netzwerkspeicher hast)

```bash
# Einmalig: Mount-Punkt erstellen
sudo mkdir -p /mnt/nas_backup

# Einmalig: Login-Daten speichern
nano ~/.smbcredentials
```

Inhalt:
```
username=dein_nas_benutzer
password=dein_nas_passwort
```

```bash
chmod 600 ~/.smbcredentials
```

### Backup-Script anpassen

```bash
nano ~/grow_controller/grow_backup.sh
```

Ändere:
```bash
DB_SOURCE="/home/pi/grow_controller/data/grow_telemetry.db"
BACKUP_DIR="/mnt/nas_backup/grow_controller"
```

### Automatisches Backup (täglich um 3 Uhr nachts)

```bash
crontab -e
```

Wähle den Editor `nano` (Option 1).

Füge am Ende hinzu:
```
0 3 * * * /home/pi/grow_controller/grow_backup.sh >> /home/pi/grow_controller/logs/backup.log 2>&1
```

> **Was bedeutet das?**
> - `0 3 * * *` = Jeden Tag um 03:00 Uhr
> - `>>` = Schreibe die Ausgabe in eine Log-Datei
> - `2>&1` = Schreibe auch Fehler in die Log-Datei

Erstelle das Logs-Verzeichnis:
```bash
mkdir -p ~/grow_controller/logs
```

---

## Wartung — Was du ab und zu machen musst

### Services neu starten

Wenn du die Konfiguration geändert hast:

```bash
sudo systemctl restart grow-telemetry.service
sudo systemctl restart grow-dashboard.service
```

### Update nach Git-Änderung

Wenn es ein Update gibt:

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
# Gesamter Speicher
df -h

# Größe der Datenbank
du -sh data/
```

Die Datenbank wächst mit ca. 5–10 MB pro Woche.

### Temperatur des Pi prüfen

```bash
vcgencmd measure_temp
```

Sollte unter 70°C bleiben. Wenn höher: Bessere Belüftung oder Kühlkörper.

---

## Fehlersuche

| Problem | Was du prüfst | Lösung |
|---------|---------------|--------|
| Service startet nicht | `sudo journalctl -u grow-telemetry.service -n 50` | `.env`-Datei prüfen, Pfade prüfen |
| "Connection refused" | `sudo systemctl status grow-telemetry` | Service läuft nicht? Neustarten |
| ESP32: `POST -> 0` | `curl http://pi-ip:8765/api/v1/health` | Firewall, IP-Adresse prüfen |
| Dashboard nicht erreichbar | `sudo systemctl status grow-dashboard` | Port 8501, Firewall prüfen |
| Datenbank sehr groß | `du -sh data/` | Retention prüfen (90 Tage Standard) |
| Pi ist sehr langsam | `htop` | Programme beenden, RAM prüfen |
| WLAN-Verbindung bricht ab | `ip addr` | Feste IP korrekt? Router-Log prüfen |

---

## Glossar

| Begriff | Einfache Erklärung |
|---------|-------------------|
| **cron** | Ein Planer auf Linux. Startet Programme zu festen Zeiten (z.B. täglich um 3 Uhr). |
| **daemon** | Ein Hintergrund-Programm, das ständig läuft. |
| **DHCP** | Dynamische IP-Vergabe durch den Router. |
| **Ethernet** | Kabelgebundenes Netzwerk (stabiler als WLAN). |
| **IP-Adresse** | Die "Adresse" eines Geräts im Netzwerk (z.B. 192.168.178.50). |
| **journalctl** | Ein Programm, um Log-Dateien (Protokolle) anzusehen. |
| **Mount** | Ein Netzlaufwerk mit einem lokalen Ordner verbinden. |
| **NAS** | "Network Attached Storage" = Netzwerkspeicher (z.B. Festplatte am Router). |
| **Port** | Eine "Türnummer" für Netzwerk-Verbindungen (z.B. 8765, 8501, 22). |
| **SD-Karte** | Der Speicher des Raspberry Pi (wie eine Festplatte). |
| **SSH** | "Secure Shell" = verschlüsselte Fernverbindung zu einem Computer. |
| **systemd** | Das Programm, das auf Linux andere Programme verwaltet und beim Start lädt. |
| **ufw** | "Uncomplicated Firewall" = einfache Firewall für Linux. |

---

## Schnell-Checkliste

### Raspberry Pi vorbereiten
- [ ] Raspberry Pi OS Lite auf SD-Karte geschrieben
- [ ] SSH aktiviert im Imager
- [ ] WLAN oder Ethernet konfiguriert
- [ ] Pi startet und ist erreichbar (`ssh pi@raspberrypi.local`)
- [ ] System aktualisiert (`sudo apt update && sudo apt upgrade -y`)
- [ ] Feste IP-Adresse eingerichtet (Router oder `/etc/dhcpcd.conf`)

### Projekt einrichten
- [ ] `git clone` erfolgreich
- [ ] Python 3.11+ installiert
- [ ] Virtuelle Umgebung erstellt und aktiviert
- [ ] `pip install -r requirements.txt` erfolgreich
- [ ] Alle 28 Tests bestanden

### Konfiguration
- [ ] `.env` erstellt und alle Werte gesetzt
- [ ] `IOT_TELEMETRY_SERVER_HOST` = Pi-IP
- [ ] `device_secrets.h` generiert
- [ ] Datenbank-Verzeichnis erstellt

### Automatischer Start
- [ ] `grow-telemetry.service` erstellt
- [ ] `grow-dashboard.service` erstellt
- [ ] Beide Services `enabled` und `active`
- [ ] Firewall-Ports 8765 und 8501 geöffnet

### Test
- [ ] Health-Check vom PC aus erfolgreich
- [ ] Dashboard im Browser erreichbar
- [ ] Login funktioniert
- [ ] ESP32 geflasht, zeigt `POST telemetry -> 200`

### Backup (optional)
- [ ] NAS mounten konfiguriert
- [ ] `grow_backup.sh` angepasst
- [ ] Cron-Job eingerichtet