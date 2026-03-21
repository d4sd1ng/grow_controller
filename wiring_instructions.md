# Verdrahtung — Growroom Control System

Stand: ein **ESP32 im Growroom** übernimmt Sensoren, Relais, lokales OLED und spricht mit dem **PC/Server** (`telemetry_api` + SQLite). Ein **Cheap Yellow Display (CYD)** ist optional als **portable Fernbedienung** (nur Strom/USB, kein Growroom-Peripherie-Anschluss nötig).

**Wichtig:** Die GPIO-Zuordnung muss exakt zum **Arduino-Sketch** passen (`#define` / `pinMode`).

**Leitungsführung:** Auf einem kleinen ESP32-Board mit vielen Adern ist es üblich, Pins **nicht** „nach Nummer schön sortiert“, sondern nach **kurzem Weg, wenig Überkreuzen** und wo die Kabel-/Klemmenstränge ohne Chaos am Modul ankommen zu wählen. Das ist völlig in Ordnung — maßgeblich sind elektrische Regeln (z. B. nur Eingang vs. Ausgang) und Übereinstimmung mit dem Code.

---

## 1. ESP32 Growroom (alles auf einem Board)

### 1.1 Temperatur / Luftfeuchte (2× AM2302 / DHT22-artig)

| Signal | ESP32 (Vorschlag) |
|--------|-------------------|
| Sensor 1 VCC | 3,3 V |
| Sensor 1 GND | GND |
| Sensor 1 DATA | GPIO **36** (nur Eingang — geeignet für One-Wire/DHT-Bibliotheken) |
| Sensor 2 DATA | GPIO **39** |
| Pull-up | ca. **4,7–10 kΩ** zwischen DATA und 3,3 V (Datenblatt beachten) |

*Hinweis:* VP/VN sind **keine** Ausgangspins — nur für Sensoren nutzen, nicht für Relais.

### 1.2 EC- und pH-Analogfront (LM358)

Wie Abschnitt **LM358** untan: Ausgänge der Stufen auf ADC-Eingänge des ESP32 (nur Eingang):

| Signal | ESP32 (Vorschlag) |
|--------|-------------------|
| EC (Op-Amp-Ausgang) | GPIO **34** |
| pH (Op-Amp-Ausgang) | GPIO **35** |

Spannungsteiler und Verstärkerschaltung so dimensionieren, dass **nie > 3,3 V** an den ADC-Pins anliegen.

### 1.3 Licht (Beispiel Photowiderstand)

| Signal | ESP32 (Vorschlag) |
|--------|-------------------|
| Spannungsteiler | Fotowiderstand + z. B. 10 kΩ zu GND, Mittelpunkt an **GPIO 32** (ADC) |

Alternativ **BH1750** per I²C (SDA/SCL siehe OLED — Adresse/Bus nicht doppelt belegen).

### 1.4 OLED (I²C, z. B. SSD1306)

| Signal | ESP32 (Vorschlag) |
|--------|-------------------|
| VCC | 3,3 V |
| GND | GND |
| SDA | GPIO **21** |
| SCL | GPIO **22** |

Diese Pins nicht mit **Relais-Treibern** teilen, die z. B. ebenfalls 21/22 belegen könnten — eine Platine, ein I²C-Bus.

### 1.5 Relais / SSR (am selben ESP32)

Die Software kennt u. a. `heater`, `circulation_vent`, `exhaust_vent`, `water_pump` (+ optional `light`). **Logik-Ausgänge** nur über geeignete Treiber (Transistor/Optokoppler/Relaismodul), niemals Last direkt am GPIO.

**Beispiel wie im Projekt-Sketch** `firmware/growroom_telemetry_client` (oft zweckmäßig, wenn die Relais-/Klemmposition so am wenigsten mit Sensoren-/Display-Kabeln kreuzt):

| Funktion | GPIO (Sketch `PIN_RELAY_*`) |
|----------|-----------------------------|
| Heizung | **4** |
| Pumpe | **5** |
| Umluft / Zirkulation | **16** |
| Abluft | **17** |

*Optional Licht:* freier **Output**-Pin + im Code ergänzen (`light`), wenn die Hardware das vorsieht.

**Alternative** (rein logisch aufsteigend, z. B. ein Klemmblock in der Reihenfolge): **16 / 17 / 18 / 19** für die vier Kanäle — nur nutzen, wenn das für deine **physische** Verlegung Sinn ergibt.

Je SSR: gleiche **GND**-Ebene wie ESP32; **Kickback-/Schutz** und **Sicherungen** nach Last (siehe Safety).

---

## 2. LM358 Anschluss (Kurzüberblick)

1. **EC:** Sonde/Signal an Eingang Stufe 1, Ausgang (Pin 1) → GPIO **34**
2. **pH:** Sonde/Signal an Eingang Stufe 2, Ausgang (Pin 7) → GPIO **35**
3. **Versorgung LM358:** V+ (Pin 8) gemäß Schaltung (häufig 5 V), GND (Pin 4) gemeinsam mit System-GND
4. Referenz/Offset und Verstärkung nach Sensor-Modul/Platinen-Doku trimmen

Detaillierte OP-Schaltung bleibt vom **EC/pH-Modul** abhängig — hier nur die **ESP32-ADC-Ziele**.

---

## 3. Stromversorgung

- Ein **geeignetes Netzteil** für ESP32 (3,3 V/reguliert) und ggf. **5 V** für SSR-/Relais-Seite, **gemeinsame Masse**.
- **Netzseite:** FI/LS, **Feinsicherungen** nach Verbrauchern (Heizung, Pumpen, Lüfter getrennt bewerten).
- Keine Mischung: **230 V-Verdrahtung** nur von Fachpersonal / unter Einhaltung der Normen.

---

## 4. Sicherheit und Erdung

- Alle **GND** der Niederspannungsseite zusammenführen; bei Metallgehäuse **Schutzerdung** nach Vorschrift.
- **Feuchtigkeit:** Gehäuse, Kabeldurchführungen, Steckerabdichtung (Growroom).
- **Vor dem Einschalten:** Durchgang, Kurzschluss, Polung, **Max. Last** der SSR prüfen.

---

## 5. Netzwerk / Software-Architektur (kein „zwei ESP32-IPs“ mehr zwingend)

| Gerät | Rolle |
|--------|--------|
| **Growroom-ESP32** | Misst, schaltet Relais, zeigt lokal OLED; **HTTP** zum Server: `GET /api/v1/control/state`, `POST /api/v1/telemetry` (siehe `firmware/`). |
| **PC/Server** | `run_telemetry_api.py` + optional Streamlit; **SQLite** `data/grow_telemetry.db`. |
| **CYD (optional)** | Nur WLAN + Display/Touch; gleicher Server (`GET mobile/summary`, `GET telemetry/series`, `POST control/targets`). **Keine** Growroom-Sensorleitungen nötig. |

- Im Heimnetz: **feste IP** oder DHCP-Reservierung für Server-PC erleichtert die Konfiguration in den Sketches (`SERVER_HOST`).
- API-Schutz: optional `TELEMETRY_API_KEY` / Header `X-Telemetry-Key` (`.env`).

---

## 6. Checkliste vor Inbetriebnahme

- [ ] GPIO-Liste Sketch = GPIO-Liste Schaltplan
- [ ] ADC nie über 3,3 V
- [ ] Relais-Logik getrennt von Roh-Netz, Sicherungen gesetzt
- [ ] Server erreichbar (Ping / Test-GET)
- [ ] Kein Kurzschluss 230 V / polarity / PE

Bei Abweichung von den GPIO-Vorschlägen hier **dieses Dokument und die `#define` in der Firmware** gemeinsam aktualisieren.
