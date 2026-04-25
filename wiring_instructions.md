# Verdrahtung — Growroom Control System

Stand: v2.0.0. Die hier festgelegte Pin-Belegung muss **exakt** zum Firmware-Sketch (`firmware/growroom_telemetry_client/growroom_telemetry_client.ino`) passen.

```
+-----------+          +-----------------------+          +------------------+
|   230V    |          |    ESP32 DevKit V1    |          |   Sensoren       |
|   Netz    |          |                       |          |                  |
|           |          |  GPIO 36  -----> DHT22 #1 (Luft) |                  |
|  Heizung  |<--SSR--->|  GPIO 39  -----> DHT22 #2       |                  |
|  Pumpe    |<--SSR--->|  GPIO 34  -----> EC-Sonde       |                  |
|  Lüfter   |<--SSR--->|  GPIO 35  -----> pH-Sonde       |                  |
|  Abluft   |<--SSR--->|  GPIO 21  -----> OLED SDA (I2C) |                  |
|           |          |  GPIO 22  -----> OLED SCL (I2C) |                  |
+-----------+          |  GPIO  4  -----> Relay Heizung   |                  |
                       |  GPIO  5  -----> Relay Pumpe     |                  |
                       |  GPIO 16  -----> Relay Umluft    |                  |
                       |  GPIO 17  -----> Relay Abluft    |                  |
                       +-----------------------+          +------------------+
                                |
                                v
                       WiFi ----> Python Server
                       (Telemetry API + Dashboard)
```

---

## 1. Sensoren — feste GPIO-Zuordnung

### 1.1 Temperatur / Luftfeuchte (DHT22 / AM2302)

| Funktion | ESP32 Pin | Hinweis |
|----------|-----------|---------|
| Sensor 1 VCC | 3.3V | Max. 3.3V — nie 5V anschließen |
| Sensor 1 GND | GND | |
| Sensor 1 DATA | **GPIO 36** | Nur-Input-Pin (VP). Pull-up **10kΩ** zwischen DATA und 3.3V |
| Sensor 2 DATA | **GPIO 39** | Nur-Input-Pin (VN). Pull-up **10kΩ** zwischen DATA und 3.3V |

> **Wichtig:** GPIO 36 (VP) und 39 (VN) sind **reine Eingangspins**. Sie können keine Relais schalten.

### 1.2 EC- und pH-Sensoren (analog)

EC- und pH-Sonden liefern ein schwaches analoges Signal. Ein **LM358** (oder ein fertiges EC/pH-Modul) puffert und verstärkt das Signal auf ESP32-kompatible Pegel.

| Funktion | ESP32 Pin | Hinweis |
|----------|-----------|---------|
| EC-Signal | **GPIO 34** | ADC1_CH6. Nur-Input. Signal muss 0–3.3V liegen |
| pH-Signal | **GPIO 35** | ADC1_CH7. Nur-Input. Signal muss 0–3.3V liegen |

> **ADC-Spannungsgrenze:** Nie mehr als **3.3V** an einen ADC-Pin anlegen — der ESP32 wird zerstört. Ein Spannungsteiler oder Zener-Diode (3.3V) als Schutz empfohlen.

#### LM358 Grundschaltung (EC-Beispiel — pH identisch auf Stufe 2)

```
EC-Sonde          LM358 (Stufe 1)              ESP32
  (+) -----> Pin 2 (Inverting In)    Pin 1 (Output) -----> GPIO 34
  (-) -----> GND                     Pin 3 (Non-Inv) ---> 2.5V Ref (Spannungsteiler)
                                     Pin 8 (V+) --------> 5V
                                     Pin 4 (GND) -------> GND

Feedback-Widerstand (Rf) zwischen Pin 1 und Pin 2 bestimmt Verstärkung.
Beispiel: Rf = 10kΩ, Ri = 1kΩ → Verstärkung = 10x
```

### 1.3 OLED Display (I2C, SSD1306 oder SH1106)

| Funktion | ESP32 Pin | Hinweis |
|----------|-----------|---------|
| VCC | 3.3V | |
| GND | GND | |
| SDA | **GPIO 21** | Feste I2C-Pins des ESP32-DevKit |
| SCL | **GPIO 22** | Feste I2C-Pins des ESP32-DevKit |

> **I2C-Adresse:** Meist 0x3C. Falls Display nicht reagiert, 0x3D testen.

---

## 2. Relais — feste GPIO-Zuordnung

Die Firmware definiert vier Relais-Pins. Diese steuern **nur Logik-Level** (HIGH/LOW) — niemals Last direkt am GPIO.

| Funktion | ESP32 Pin | Firmware-Define | Last-Beispiel |
|----------|-----------|-----------------|---------------|
| Heizung | **GPIO 4** | `PIN_RELAY_HEATER 4` | 500W Heizstab |
| Wasserpumpe | **GPIO 5** | `PIN_RELAY_PUMP 5` | 20W Umwälzpumpe |
| Umluft / Zirkulation | **GPIO 16** | `PIN_RELAY_CIRCULATION 16` | 15W Lüfter |
| Abluft | **GPIO 17** | `PIN_RELAY_EXHAUST 17` | 25W Abluft |

### Relais-Treiber-Anschluss (SSR = Solid State Relay)

```
ESP32 GPIO 4 -----> SSR-Steuer-Eingang (+)
ESP32 GND --------> SSR-Steuer-Eingang (-)

SSR-Ausgang:
  L (Phase) -----> SSR-L1
  SSR-L2 --------> Heizung
  N (Null) ------> Heizung (direkt)
```

> **SSR-Typ:** DC-Input (3–32V), AC-Output (bis 250V, passend zur Last). Kein mechanisches Relais für Heizung — SSR ist verschleißfrei.

### Schutzbeschaltung pro Relais-Kanal

- **Freilaufdiode** nur bei mechanischen Relais (nicht bei SSR nötig)
- **Feinsicherung** pro Lastkreis (siehe Tabelle unten)
- **FI-Schalter** in der Hauptverteilung
- **Metallgehäuse** schutzgeerdet

| Last | Sicherung | Bemessung |
|------|-----------|-----------|
| Heizung 500W | 5A träge | 500W / 230V = 2.2A → 5A |
| Wasserpumpe 20W | 2A träge | 20W / 230V = 0.09A → 2A |
| Umluft 15W | 1A träge | |
| Abluft 25W | 1A träge | |

---

## 3. Stromversorgung — Übersicht

```
Netzteil 5V/3A (oder 12V mit Step-Down)
    |
    +---> Step-Down 3.3V -----> ESP32 3.3V-Pin
    |                           +---> Sensoren VCC (3.3V)
    |                           +---> OLED VCC (3.3V)
    |
    +---> 5V-Rail -------------> LM358 V+ (Pin 8)
                                  +---> SSR-Steuer-Eingänge (3–32V kompatibel)

GND (alle Massepunkte verbunden!):
    ESP32 GND <---> Sensoren GND <---> OLED GND <---> LM358 GND <---> Netzteil GND
```

> **Ein gemeinsames GND** für alle Komponenten ist Pflicht. Andernfalls treten "Geister-Spannungen" auf.

---

## 4. Pin-Übersicht — alles auf einen Blick

| Pin | Funktion | Richtung | Spannung | Hinweis |
|-----|----------|----------|----------|---------|
| GPIO 36 | DHT22 #1 DATA | In | 3.3V | VP, nur Input |
| GPIO 39 | DHT22 #2 DATA | In | 3.3V | VN, nur Input |
| GPIO 34 | EC-Signal | In (ADC) | 0–3.3V | ADC1_CH6 |
| GPIO 35 | pH-Signal | In (ADC) | 0–3.3V | ADC1_CH7 |
| GPIO 21 | OLED SDA | I2C | 3.3V | Feste I2C-Pin |
| GPIO 22 | OLED SCL | I2C | 3.3V | Feste I2C-Pin |
| GPIO 4 | Relay Heizung | Out | 3.3V | Logik-Level |
| GPIO 5 | Relay Pumpe | Out | 3.3V | Logik-Level |
| GPIO 16 | Relay Umluft | Out | 3.3V | Logik-Level |
| GPIO 17 | Relay Abluft | Out | 3.3V | Logik-Level |

---

## 5. Ändern der Pin-Belegung

Wenn du die GPIO-Zuordnung ändern musst:

1. **Firmware anpassen** in `firmware/growroom_telemetry_client/growroom_telemetry_client.ino`:
   ```cpp
   #define PIN_RELAY_HEATER 4      // <-- ändern
   #define PIN_RELAY_PUMP 5        // <-- ändern
   // usw.
   ```

2. **Dieses Dokument anpassen** — beide Dateien müssen übereinstimmen.

3. **Wiring-Checkliste durchführen** (siehe unten).

> **Empfehlung:** Ändere nur wenn nötig. Die vorgegebene Belegung ist im Sketch und im Dokument abgestimmt.

---

## 6. Checkliste vor dem ersten Einschalten

### 6.1 Spannungsprüfung (mit Multimeter, ESP32 **nicht** angeschlossen)

- [ ] Netzteil liefert stabil **3.3V** (±0.1V)
- [ ] Alle ADC-Eingänge (GPIO 34, 35) messen **0V** (keine Überspannung)
- [ ] Alle Relais-Steuer-Eingänge messen **0V** (kein Dauer-High)

### 6.2 Verbindungsprüfung (ESP32 angeschlossen, 230V **noch nicht**)

- [ ] Durchgang GND: Alle GND-Punkte sind verbunden (< 1Ω)
- [ ] Kein Kurzschluss 3.3V → GND (Widerstand > 1kΩ)
- [ ] Durchgang je Signal-Leitung: Pin am ESP32 → korrekter Pin am Modul

### 6.3 Software-Check (nur 3.3V, 230V **noch nicht**)

- [ ] ESP32 flashen mit `growroom_telemetry_client.ino`
- [ ] Serieller Monitor (115200 Baud): WiFi-Verbindung bestätigt
- [ ] Serieller Monitor: Relais-Status zeigt alle `LOW` (aus)
- [ ] Server (Telemetry API) erreichbar: `GET http://<server-ip>:8765/api/v1/health` → `{"ok":true}`

### 6.4 230V-Check (nur nach erfolgreichem 6.3)

- [ ] 230V-Verdrahtung von Fachkraft geprüft
- [ ] FI-Schalter testen
- [ ] Jede Sicherung korrekt dimensioniert
- [ ] Metallgehäuse geerdet
- [ ] Erst **einen** Relais-Kanal testen, dann die anderen

---

## 7. Fehlersuche

| Symptom | Ursache | Lösung |
|---------|---------|--------|
| DHT22 liefert keine Werte | Pull-up fehlt | 10kΩ zwischen DATA und 3.3V |
| ADC-Werte springen | Kein gemeinsames GND | Alle GND verbunden? |
| Relais schaltet nicht | SSR braucht mehr Strom | Direkt an 5V statt 3.3V testen |
| OLED zeigt nichts | Falsche I2C-Adresse | Scanner-Sketch: Adresse prüfen |
| ESP32 rebootet | Spannungseinbruch | Netzteil zu schwach (min. 1A bei 3.3V) |
| WiFi nicht verbunden | Falsche Credentials | `device_secrets.h` prüfen |
