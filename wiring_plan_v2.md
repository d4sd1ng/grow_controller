# Wiringplan V2 — Grow Controller

Stand: 2026-10-06

Dieser Plan beschreibt ausschließlich die **Kleinspannungs-/Steuerseite**. Die 230-V-Lastseite der SSR ist bewusst nicht enthalten.

## 1. Architektur

```text
Sensor-Kasten ESP32 (ESP32 OLED-0.96 V3.0)
  ├─ 3x DHT22
  ├─ 2x DS18B20
  ├─ ADS1115
  │   ├─ A0 <- PH4502C PO über Spannungsteiler
  │   └─ A1 <- CQRobot CQRSENTDS01 Signal
  ├─ BH1750 auf separatem I2C-Bus
  └─ integriertes OLED

              WLAN

Strom-Kasten ESP32
  └─ 4x Kleinspannungs-Steuersignal direkt zu den SSR-Eingängen
      ├─ SSR1 Licht
      ├─ SSR2 Umluft links + rechts
      ├─ SSR3 Abluft
      └─ SSR4 Heizstrahler

              WLAN

Cheap Yellow Display (CYD)
  └─ Anzeige / Bedienung

              WLAN

Homeserver / Raspberry Pi
  └─ FastAPI + PostgreSQL + Dashboard / Historie
```

Die Wasserpumpe läuft dauerhaft und erhält **keinen SSR-Kanal und keinen ESP32-GPIO**.

---

## 2. Sensor-Kasten — ESP32 GPIO-Belegung

| Funktion | ESP32 |
|---|---:|
| internes OLED SDA | GPIO21 |
| internes OLED SCL | GPIO22 |
| ADS1115 SDA | GPIO21 |
| ADS1115 SCL | GPIO22 |
| DHT22 #1 Pflanzenzone | GPIO25 |
| DHT22 #2 Unter-/Zuluftzone | GPIO26 |
| DHT22 #3 Ober-/Abluftzone | GPIO27 |
| DS18B20 gemeinsamer 1-Wire-Bus | GPIO32 |
| BH1750 SDA, separater I2C-Bus | GPIO13 |
| BH1750 SCL, separater I2C-Bus | GPIO14 |
| Reserve | GPIO33 |

GPIO34–39 werden für die DHT22 nicht verwendet.

### Versorgungsschienen

| Schiene | Verbraucher |
|---|---|
| 3.3 V | ADS1115, 3x DHT22, 2x DS18B20, BH1750 |
| 5 V | PH4502C, CQRobot CQRSENTDS01 |
| GND | gemeinsame Kleinspannungs-Masse aller Sensor-/Messmodule |

---

## 3. DHT22

Jeder DHT22 bekommt einen eigenen Datenpin.

| Sensor | DATA | VCC | GND |
|---|---|---|---|
| DHT22 #1 | GPIO25 | 3.3 V | GND |
| DHT22 #2 | GPIO26 | 3.3 V | GND |
| DHT22 #3 | GPIO27 | 3.3 V | GND |

Je DHT22: **4.7 kOhm Pull-up von DATA nach 3.3 V**, vorzugsweise im Sensorkasten am Controllerende.

---

## 4. DS18B20

Beide DS18B20 liegen gemeinsam auf einem 1-Wire-Bus.

```text
3.3 V --------+---- DS18B20 #1 VDD
              +---- DS18B20 #2 VDD
              |
            4.7 kOhm
              |
GPIO32 -------+---- DS18B20 #1 DATA
              +---- DS18B20 #2 DATA

GND ----------+---- DS18B20 #1 GND
              +---- DS18B20 #2 GND
```

Nur **ein** 4.7-kOhm-Pull-up für den gemeinsamen Bus.

Die beiden Sensoren werden später anhand ihrer 64-Bit-1-Wire-Adressen eindeutig zugeordnet.

---

## 5. ADS1115

ADS1115 wird mit 3.3 V betrieben.

| ADS1115 | Verbindung |
|---|---|
| VDD | 3.3 V |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |
| ADDR | GND -> Adresse 0x48 |
| A0 | pH-Signal |
| A1 | TDS-Signal |
| A2 | Reserve |
| A3 | Reserve |

ADS1115 und das interne OLED teilen sich GPIO21/22. Beide sitzen im Sensorkasten; die Leitungen bleiben kurz.

---

## 6. PH4502C

| PH4502C | Verbindung |
|---|---|
| VCC / V+ | 5 V |
| GND | GND |
| PO | über Spannungsteiler an ADS1115 A0 |
| DO | nicht verwendet |
| TO | nicht verwendet |

### Spannungsteiler PO -> ADS1115 A0

```text
PH4502C PO
    |
   10 kOhm
    |
    +---------- ADS1115 A0
    |
   20 kOhm
    |
   GND
```

Der Teiler reduziert 0–5 V auf maximal ca. 0–3.33 V.

Die pH-Berechnung wird später per Kalibrierpunkten in der Firmware eingestellt; kein einfaches lineares 0–14-Mapping des alten Codes.

---

## 7. CQRobot CQRSENTDS01

| CQRobot-Leitung | Verbindung |
|---|---|
| Rot | 5 V |
| Schwarz | GND |
| Grün / Signal | ADS1115 A1 |

Das Modul liefert laut Hersteller maximal ca. 2.3 V am Analogausgang und benötigt deshalb am ADS1115 A1 keinen Spannungsteiler.

Der separate DS18B20 im Nährstofftank liefert die Wassertemperatur, die die Firmware für die temperaturbezogene TDS-Auswertung verwenden kann.

---

## 8. BH1750

BH1750 wird wegen der längeren Leitung nicht auf denselben I2C-Leitungen wie ADS1115/OLED geführt.

| BH1750 | Verbindung |
|---|---|
| VCC | 3.3 V |
| GND | GND |
| SDA | GPIO13 |
| SCL | GPIO14 |
| ADDR | Standard / Low -> 0x23 |

Firmware: eigener ESP32-I2C-Controller, Busgeschwindigkeit zunächst 50 kHz.

Für die Leitung zum BH1750: CAT5/CAT6 verwenden. SDA und SCL jeweils mit einer GND-Ader verdrillen. Leitung praktisch so kurz wie möglich halten.

---

## 9. Strom-Kasten — ESP32 und 4 SSR-Steuereingänge

Der Stromkasten-ESP32 schaltet ausschließlich die Kleinspannungs-Eingänge der vier SSR.

### GPIO-Zuordnung

| GPIO | Kanal |
|---:|---|
| GPIO17 | SSR1 Licht |
| GPIO18 | SSR2 Umluft links + rechts |
| GPIO19 | SSR3 Abluft |
| GPIO23 | SSR4 Heizstrahler |

Die vorhandenen FOTEK SSR-10DA sind am Eingang mit **3–32 VDC** beschriftet. Damit liegt der 3.3-V-HIGH-Pegel des ESP32 innerhalb der angegebenen Eingangsspannung und es ist **keine zusätzliche ULN2803A-Treiberstufe erforderlich**.

### Direkte SSR-Ansteuerung

| ESP32 | SSR-Eingang |
|---|---|
| GPIO17 | SSR1 INPUT + (Licht) |
| GPIO18 | SSR2 INPUT + (Umluft) |
| GPIO19 | SSR3 INPUT + (Abluft) |
| GPIO23 | SSR4 INPUT + (Heizstrahler) |

Weitere Verbindungen:

- jeweiliger SSR INPUT - -> ESP32 GND
- je GPIO 17/18/19/23 ein 10-kOhm-Pulldown nach GND, damit die SSR beim ESP32-Reset sicher aus bleiben

Logik: GPIO HIGH -> SSR EIN.

---

## 10. 230-V-Seite — Funktionsblock

Die Netzseite wird im Plan nur als **Funktionsblock** dokumentiert, nicht als Klemmen-für-Klemmen-Anleitung.

Es gibt **eine gemeinsame 230-V-Zuleitung zum Leistungskasten / SSR-Block**. Von dort werden die vier geschalteten Lastkanäle versorgt:

| Kanal | Last |
|---|---|
| SSR1 | Beleuchtung |
| SSR2 | beide Umluftventilatoren gemeinsam |
| SSR3 | Abluftventilator |
| SSR4 | Heizstrahler |

Die Wasserpumpe ist **kein SSR-Kanal** und bleibt dauerhaft versorgt.

```text
230-V-Zuleitung
      |
      +---- geschützter Verteilpunkt
              |
              +---- SSR1 ---- Beleuchtung
              +---- SSR2 ---- Umluft links + rechts
              +---- SSR3 ---- Abluft
              +---- SSR4 ---- Heizstrahler
              |
              +---- Dauerzweig ---- Wasserpumpe
```

Für die Auslegung der gemeinsamen Zuleitung und der vorgeschalteten Schutzorgane ist die **maximal gleichzeitig auftretende Gesamtlast** maßgeblich. Schutzleiter- und Netzspannungsführung bleiben räumlich von der Kleinspannungs-/Sensorsignalseite getrennt.


## 11. Kabelplanung

| Strecke | Planlänge |
|---|---:|
| DHT22 #1 -> Sensorkasten | 3.0 m |
| DHT22 #2 -> Sensorkasten | 3.0 m |
| DHT22 #3 -> Sensorkasten | 3.0 m |
| DS18B20-Bus -> Sensorkasten | ca. 2.5 m plus kurze Abzweige |
| BH1750 -> Sensorkasten | max. ca. 2.5 m, CAT5/CAT6 |
| PH4502C -> ADS1115 | 10–15 cm |
| CQRobot -> ADS1115 | 10–15 cm |
| ADS1115 -> ESP32 | 10–15 cm |

Die originalen pH-/TDS-Sondenleitungen nicht verlängern, solange es nicht zwingend erforderlich ist.

---

## 12. Sensorpositionen

| Sensor | Position |
|---|---|
| DHT22 #1 | zentrale Pflanzenzone, etwa 10–15 cm über aktueller Blatthöhe |
| DHT22 #2 | untere/frontseitige Zone nahe Zuluft, nicht direkt im Luftstrahl |
| DHT22 #3 | obere Zone nahe Abluft/Lampenhöhe, nicht direkt im Saugstrom |
| BH1750 | über/bei der Pflanzenzone, Sicht zur Beleuchtung, mit der Kulturhöhe mitführbar |
| DS18B20 #1 | bei der pH-Sonde |
| DS18B20 #2 | repräsentative Stelle im Nährstofftank |
| pH-Sonde | Nährstofftank / Messbereich |
| TDS-Sonde | Nährstofftank / Messbereich |

---

## 13. Firmware-Folgen dieses Wiringplans

Der aktuelle Repository-Stand ist noch **nicht** kompatibel mit diesem Plan.

Erforderlich sind insbesondere:

| Bereich | Änderung |
|---|---|
| Sensor-ESP Firmware | 3x DHT22, 2x DS18B20, ADS1115, PH4502C, CQRobot, separater BH1750-I2C-Bus |
| Sensor-ESP Firmware | keine lokalen SSR-GPIOs mehr; berechnet Sollzustände und sendet sie über WLAN |
| Strom-ESP Firmware | neuer Aktor-Knoten für vier direkt angesteuerte SSR-Kanäle + Kommunikations-Watchdog/Failsafe |
| CYD | PUMP-Kanal entfernen und durch LIGHT ersetzen |
| Telemetrie | zusätzliche Sensorfelder + Licht-Relaiszustand; Wasserpumpe nicht als schaltbarer Kanal |
| Server | SQLite durch PostgreSQL ersetzen |
| Dashboard/Tests/ML | bisherige pump-Referenzen auf neue Hardwarearchitektur anpassen |

