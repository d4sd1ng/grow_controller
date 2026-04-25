# Installation — Growroom Control System v2.0.0

Diese Anleitung führt dich Schritt für Schritt durch die Installation. **Keine Programmier-Vorkenntnisse nötig.** Wenn ein Begriff unklar ist, schau ins Glossar am Ende.

**Was du am Ende hast:** Ein Server-Programm auf deinem PC oder Raspberry Pi, das Sensordaten von ESP32-Geräten empfängt und ein Web-Dashboard anzeigt.

---

## Übersicht: Was passiert in welcher Reihenfolge?

```
Schritt 1: Python und Tools installieren
     |
Schritt 2: Das Projekt herunterladen (git clone)
     |
Schritt 3: Virtuelle Umgebung erstellen
     |
Schritt 4: Konfigurationsdatei (.env) anlegen
     |
Schritt 5: Secrets für den ESP32 exportieren
     |
Schritt 6: Server starten und testen
     |
Schritt 7: Dashboard öffnen
     |
Schritt 8 (optional): ESP32 flashen
```

> **Tipp:** Mache einen Schritt nach dem anderen. Wenn etwas nicht funktioniert, nicht weitergehen, sondern das Problem lösen.

---

## Schritt 1: Python installieren

### Was ist Python?

Python ist eine Programmiersprache. Das Server-Programm ist in Python geschrieben. Du musst Python auf deinem Computer installieren, damit das Programm läuft.

### Installation

1. Gehe auf https://www.python.org/downloads/
2. Lade **Python 3.11** oder neuer herunter
3. Starte die Installation
4. **Wichtig:** Setze den Haken bei **"Add Python to PATH"** (unten im Installer)
5. Klicke "Install Now"

### Prüfen, ob Python funktioniert

Öffne die Eingabeaufforderung (Windows-Taste + R, dann `cmd` eingeben) und tippe:

```
python --version
```

Wenn du siehst: `Python 3.11.x` → Perfekt!
Wenn du siehst: `'python' is not recognized` → Python wurde nicht im PATH hinzugefügt. Installiere neu mit "Add to PATH".

### Git installieren

Git ist ein Tool, um Programmcode aus dem Internet herunterzuladen.

1. Gehe auf https://git-scm.com/download/win
2. Lade Git herunter und installiere es
3. Alle Standardeinstellungen belassen

Prüfen:
```
git --version
```

Sollte anzeigen: `git version 2.x.x`

---

## Schritt 2: Das Projekt herunterladen

### Was ist ein Repository?

Ein Repository (kurz: Repo) ist wie ein Ordner mit Programmcode im Internet. Mit `git clone` lädst du den Ordner auf deinen Computer herunter.

### Befehle eingeben

Öffne die Eingabeaufforderung (cmd) oder PowerShell und tippe:

```bash
# In deinen Dokumente-Ordner wechseln
cd Documents

# Das Projekt herunterladen
git clone https://github.com/d4sd1ng/grow_controller.git

# In den Projektordner wechseln
cd grow_controller
```

Du solltest jetzt einen Ordner `grow_controller` in deinen Dokumenten haben.

---

## Schritt 3: Virtuelle Umgebung erstellen

### Was ist eine virtuelle Umgebung?

Stell dir vor, du installierst ein Programm. Das Programm braucht bestimmte Zusatzprogramme ("Bibliotheken"). Wenn du viele Programme installierst, können sich die Bibliotheken in die Quere kommen.

Eine **virtuelle Umgebung** ist wie ein abgetrennter Raum: Darin installierst du nur die Bibliotheken, die dieses eine Programm braucht. Sie stören nichts anderes auf deinem Computer.

### Erstellen der Umgebung

In der Eingabeaufforderung (du bist noch im `grow_controller`-Ordner):

```bash
python -m venv .venv
```

Das erstellt einen neuen Ordner `.venv` im Projektordner. Das kann 1–2 Minuten dauern.

### Die Umgebung aktivieren

**Windows (Eingabeaufforderung / cmd):**
```bash
.venv\Scripts\activate.bat
```

**Windows (PowerShell):**
```powershell
.venv\Scripts\Activate.ps1
```

**Wenn eine Fehlermeldung kommt** (PowerShell Execution Policy):
```powershell
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```
Dann den Aktivierungsbefehl nochmals.

**Wenn es funktioniert**, sieht dein Prompt so aus:
```
(.venv) C:\Users\DeinName\Documents\grow_controller>
```

Das `(.venv)` bedeutet: Die virtuelle Umgebung ist aktiv. **Ab jetzt arbeitest du immer in dieser Umgebung.**

### Bibliotheken installieren

```bash
pip install -r requirements.txt
```

Das lädt und installiert alle benötigten Zusatzprogramme:
- Streamlit (das Web-Dashboard)
- FastAPI + Uvicorn (die API für die ESP32)
- Pandas + Plotly (Datenverarbeitung und Charts)
- scikit-learn (Machine Learning)
- Und weitere...

Das dauert 2–5 Minuten. Warte, bis der Prompt wieder erscheint.

### Test: Läuft alles?

```bash
python -m unittest system_tests test_extended
```

Erwartetes Ergebnis:
```
Ran 28 tests in 2.345s
OK
```

Wenn `OK` steht: Alles funktioniert!
Wenn Fehler auftreten: Kopiere die Fehlermeldung und erstelle ein Issue auf GitHub.

---

## Schritt 4: Konfigurationsdatei anlegen

### Was ist .env?

`.env` ist eine Textdatei mit Einstellungen (Passwörter, WLAN-Daten, etc.). Sie heißt nur `.env` — ohne Namen vor dem Punkt. Das ist unter Windows ungewöhnlich, aber normal in der Programmierwelt.

**Wichtig:** `.env` steht nicht im Git-Repository. Deine Passwörter sind also sicher.

### Die Vorlage kopieren

Im `grow_controller`-Ordner gibt es eine Datei `.env.example`. Die ist eine Vorlage.

**Windows:**
```bash
copy .env.example .env
```

**Oder über den Explorer:**
1. Rechtsklick auf `.env.example`
2. "Kopieren"
3. Rechtsklick in den leeren Bereich
4. "Einfügen"
5. Die neue Datei umbenennen in `.env`

### .env bearbeiten

Öffne `.env` mit einem Texteditor (Notepad, Notepad++, VS Code).

**Diese Werte musst du auf jeden Fall ändern:**

```env
# Dein Dashboard-Passwort (ändern!)
DASHBOARD_PASSWORD=growroom2024

# Ein geheimer Schlüssel für die Kommunikation (32 Zeichen, selbst erfinden)
TELEMETRY_API_KEY=MeinGeheimerSchluessel1234567890

# Dein WLAN-Name
IOT_WIFI_SSID=MeinHeimWLAN

# Dein WLAN-Passwort
IOT_WIFI_PASSWORD=MeinWLANPasswort2024

# Die IP-Adresse deines Servers (siehe unten)
IOT_TELEMETRY_SERVER_HOST=192.168.178.50
```

### Wie finde ich die IP-Adresse meines Servers?

**Wenn der Server dein PC ist:**

1. Drücke Windows-Taste + R
2. Tippe `cmd` und Enter
3. Tippe: `ipconfig`
4. Suche nach "IPv4-Adresse" unter deiner WLAN-Verbindung
5. Das ist deine IP, z.B. `192.168.178.50`

**Wenn der Server ein Raspberry Pi ist:**

1. Am Pi ein Terminal öffnen
2. Tippe: `hostname -I`
3. Die erste Zahl ist die IP, z.B. `192.168.178.50`

> **Wichtig:** Der Server braucht eine **feste IP-Adresse**. Sonst ändert sie sich bei jedem Neustart und die ESP32-Geräte finden den Server nicht mehr.

**So richtest du eine feste IP ein (Router):**
1. Router-Webinterface öffnen (meist http://192.168.178.1 oder http://fritz.box)
2. Anmelden
3. Suche nach "DHCP-Reservierung" oder "Feste IP"
4. Wähle deinen PC/Pi aus der Geräteliste
5. Weise ihm immer dieselbe IP zu

### Die anderen Einstellungen

Diese kannst du so lassen (funktionieren mit den Standardwerten):

```env
TELEMETRY_API_PORT=8765          # Port der API
TELEMETRY_API_HOST=0.0.0.0       # API lauscht auf allen Netzwerken
GROW_DATA_DB=data/grow_telemetry.db   # Wo die Daten gespeichert werden
GROW_DATA_RETENTION_DAYS=90      # Daten älter als 90 Tage werden gelöscht
```

**Telegram (optional):**

Wenn du Benachrichtigungen auf dein Handy bekommen willst:

```env
TELEGRAM_BOT_TOKEN=123456789:ABCdefGHIjklMNOpqrSTUvwxyz
TELEGRAM_CHAT_ID=987654321
```

> **Wie bekomme ich Telegram-Daten?**
> 1. Schreibe @BotFather auf Telegram
> 2. Erstelle einen neuen Bot
> 3. Du bekommst einen Token
> 4. Schreibe @userinfobot
> 5. Du bekommst deine Chat-ID

**Speichern** und schließen.

---

## Schritt 5: Secrets für den ESP32 exportieren

### Was ist ein "Secret"?

Secrets = Geheimnisse = Passwörter, WLAN-Daten, etc. Der ESP32-Mikrocontroller kann die `.env`-Datei nicht lesen. Er braucht eine eigene Datei mit denselben Daten.

### Der Export-Befehl

Stelle sicher, dass die virtuelle Umgebung aktiv ist (`(.venv)` im Prompt).

```bash
python export_firmware_secrets_from_env.py
```

Erwartete Ausgabe:
```
OK: E:\Documents\grow_controller\firmware\growroom_telemetry_client\device_secrets.h
OK: E:\Documents\grow_controller\firmware\cyd_remote_control\device_secrets.h
OK: E:\Documents\grow_controller\firmware\cyd_grow_display\device_secrets.h
```

Dieser Befehl erstellt drei Dateien namens `device_secrets.h` — eine für jeden ESP32-Sketch. Diese Datei enthält:
- WLAN-Name und Passwort
- Die Server-IP-Adresse
- Den geheimen API-Key
- Port und HTTPS-Einstellungen

> **Wichtig:** `device_secrets.h` wird nie ins Git-Repository hochgeladen (steht in `.gitignore`). Deine Passwörter bleiben privat.

---

## Schritt 6: Server starten und testen

### Was passiert hier?

Der Server hat zwei Teile:
1. **Telemetry API** (Port 8765) — empfängt Daten von den ESP32-Geräten
2. **Dashboard** (Port 8501) — die Web-Oberfläche, die du im Browser siehst

### Telemetry API starten

Öffne ein neues Terminal-Fenster (die alte läuft weiter).

**Virtuelle Umgebung aktivieren** (wieder `(.venv)` im Prompt):
```bash
# Windows
cd Documents\grow_controller
.venv\Scripts\activate.bat

# Server starten
python run_telemetry_api.py
```

Du solltest sehen:
```
INFO:     Uvicorn running on http://0.0.0.0:8765 (Press CTRL+C to quit)
```

> **Das Fenster muss offen bleiben!** Wenn du es schließt, stoppt der Server.

### Test: Funktioniert die API?

Öffne ein **drittes** Terminal-Fenster:

```bash
curl http://localhost:8765/api/v1/health
```

> **Was ist curl?** Ein Programm, das Webseiten im Terminal abruft. Wenn curl nicht installiert ist: Lade es von https://curl.se/windows/ herunter.

Erwartetes Ergebnis:
```json
{"ok":true,"db":"connected","version":"2.0.0"}
```

Wenn das kommt: Die API läuft!

### Alternative ohne curl

Öffne deinen Browser und gehe zu:
```
http://localhost:8765/api/v1/health
```

Du solltest `{"ok":true}` sehen.

---

## Schritt 7: Dashboard starten

### Was ist ein Dashboard?

Ein Dashboard ist wie ein Kontrollpanel — eine Webseite, auf der du Live-Daten siehst (Temperatur, Luftfeuchte, etc.) und Geräte steuern kannst.

### Dashboard starten

Öffne ein **viertes** Terminal-Fenster:

```bash
cd Documents\grow_controller
.venv\Scripts\activate.bat
streamlit run main.py
```

Du siehst:
```
  You can now view your Streamlit app in your browser.

  Local URL: http://localhost:8501
  Network URL: http://192.168.178.50:8501
```

### Dashboard öffnen

1. Öffne deinen Browser (Chrome, Firefox, Edge)
2. Gehe zu: `http://localhost:8501`
3. Melde dich an:
   - Username: `admin`
   - Password: `DASHBOARD_PASSWORD` aus deiner `.env`-Datei

### Was siehst du im Dashboard?

| Tab | Was du dort findest |
|-----|---------------------|
| **Übersicht** | Live-Werte: Temperatur, Luftfeuchte, VPD, pH, EC |
| **Charts** | Verläufe der letzten 24 Stunden / 7 Tage / 30 Tage |
| **Steuerung** | Relais manuell ein-/ausschalten |
| **Geräte** | Alle verbundenen ESP32-Geräte |
| **ML** | Machine Learning: Modell trainieren für Optimierung |
| **Export** | Daten als CSV oder JSON herunterladen |

---

## Schritt 8: ESP32 flashen (optional, nur wenn du Hardware hast)

### Was ist "flashen"?

"Flashen" bedeutet: Ein Programm auf den ESP32-Mikrocontroller übertragen. Das ist wie eine App auf dein Handy installieren.

### Voraussetzungen

1. **Arduino IDE** installiert (https://www.arduino.cc/en/software)
2. **ESP32-Board-Paket** installiert:
   - Arduino IDE öffnen
   - Datei → Voreinstellungen
   - Bei "Zusätzliche Boardverwalter-URLs" einfügen:
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
         ```
   - Werkzeuge → Board → Boardverwalter
   - Suche nach "ESP32" und installiere "ESP32 by Espressif Systems"
3. **Library installieren**:
   - Sketch → Bibliothek einbinden → Bibliotheken verwalten
   - Suche nach "ArduinoJson"
   - Installiere "ArduinoJson by Benoit Blanchon" (Version 6.x)

### Sketch öffnen

1. Arduino IDE öffnen
2. Datei → Öffnen
3. Navigiere zu: `Documents\grow_controller\firmware\growroom_telemetry_client`
4. Wähle `growroom_telemetry_client.ino`

### Board einstellen

- Werkzeuge → Board → ESP32 Arduino → "ESP32 Dev Module"
- Werkzeuge → Port → Wähle den COM-Port deines ESP32 (z.B. COM3)

### Hochladen

1. Sketch → Hochladen (oder Ctrl+U)
2. Warte, bis "Hochladen abgeschlossen" erscheint
3. Werkzeuge → Serieller Monitor öffnen
4. Baudrate auf **115200** stellen (unten rechts)

Du solltest sehen:
```
WiFi......
Connected: 192.168.178.101
POST telemetry -> 200
```

`POST telemetry -> 200` bedeutet: Der ESP32 hat erfolgreich Daten an deinen Server geschickt!

---

## Schritt 9: Firewall einrichten (nur wenn nötig)

Wenn der ESP32 keine Verbindung zum Server bekommt, blockiert möglicherweise die Firewall.

### Windows-Firewall öffnen

1. PowerShell als Administrator öffnen
2. Eingeben:
```powershell
New-NetFirewallRule -DisplayName "Grow Telemetry API" -Direction Inbound -Protocol TCP -LocalPort 8765 -Action Allow
```

### Wenn das nicht klappt (einfache Alternative)

1. Windows-Sicherheit öffnen
2. Firewall und Netzwerkschutz
3. Erweiterte Einstellungen
4. Eingehende Regeln → Neue Regel
5. Port → TCP → 8765 → Verbindung zulassen

---

## Glossar

| Begriff | Einfache Erklärung |
|---------|-------------------|
| **API** | "Application Programming Interface" = eine Schnittstelle, über die Programme miteinander reden. Hier: Der ESP32 schickt Daten an die API des Servers. |
| **Baud** | Die Geschwindigkeit der Datenübertragung über USB. Hier: 115200 Bits pro Sekunde. |
| **curl** | Ein Programm, das Webseiten im Terminal abruft. Zum Testen, ob ein Server läuft. |
| **Dashboard** | Eine Web-Oberfläche mit Grafiken und Knöpfen. Du öffnest sie im Browser. |
| **.env** | Eine Textdatei mit Einstellungen (Passwörter, etc.). Der Punkt am Anfang macht sie auf Linux/Mac "versteckt". |
| **Git** | Ein Programm, um Code-Versionen zu verwalten und aus dem Internet herunterzuladen. |
| **GitHub** | Eine Webseite, auf der Programmcode gespeichert wird. |
| **PATH** | Eine System-Variable, die sagt, wo Programme liegen. Wenn Python im PATH ist, findest du es von überall aus. |
| **pip** | Der Python-Paketmanager. Installiert Zusatzprogramme mit einem Befehl. |
| **Port** | Eine "Türnummer" für Netzwerkverbindungen. Die API nutzt Port 8765, das Dashboard Port 8501. |
| **Repository** | Ein Ordner mit Programmcode im Internet. |
| **Secret** | Ein Geheimnis wie Passwort oder API-Key. |
| **Streamlit** | Ein Python-Programm, das Web-Dashboards erstellt. |
| **venv** | "Virtual Environment" = ein abgetrennter Bereich für Python-Bibliotheken. |
| **WLAN-SSID** | Der Name deines WLAN-Netzwerks (z.B. "MeinHeimWLAN"). |

---

## Schnell-Checkliste

### Vorbereitung
- [ ] Python 3.11+ installiert (`python --version` zeigt Version)
- [ ] "Add to PATH" bei der Python-Installation aktiviert
- [ ] Git installiert (`git --version` zeigt Version)

### Projekt einrichten
- [ ] `git clone` erfolgreich
- [ ] Virtuelle Umgebung erstellt (`python -m venv .venv`)
- [ ] Virtuelle Umgebung aktiviert (`(.venv)` im Prompt)
- [ ] Bibliotheken installiert (`pip install -r requirements.txt`)
- [ ] Alle 28 Tests bestanden (`python -m unittest system_tests test_extended`)

### Konfiguration
- [ ] `.env` aus `.env.example` kopiert
- [ ] `DASHBOARD_PASSWORD` geändert
- [ ] `TELEMETRY_API_KEY` gesetzt (32 Zeichen)
- [ ] `IOT_WIFI_SSID` = dein WLAN-Name
- [ ] `IOT_WIFI_PASSWORD` = dein WLAN-Passwort
- [ ] `IOT_TELEMETRY_SERVER_HOST` = IP deines Servers
- [ ] `device_secrets.h` generiert (`python export_firmware_secrets_from_env.py`)

### Server
- [ ] Telemetry API läuft (`python run_telemetry_api.py`)
- [ ] Health-Check zeigt `{"ok":true}`
- [ ] Dashboard läuft (`streamlit run main.py`)
- [ ] Login funktioniert im Browser

### ESP32 (optional)
- [ ] Arduino IDE installiert
- [ ] ESP32-Board-Paket installiert
- [ ] ArduinoJson Library installiert
- [ ] Sketch geöffnet
- [ ] `device_secrets.h` im Sketch-Ordner vorhanden
- [ ] Upload erfolgreich
- [ ] Serieller Monitor zeigt `POST telemetry -> 200`