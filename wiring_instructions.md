# Verdrahtung — Growroom Control System

Diese Anleitung erklärt Schritt für Schritt, wie du alle Kabel anschließt. **Du brauchst keine Elektronik-Vorkenntnisse.** Wenn ein Begriff unklar ist, findest du unten eine Erklärung im Glossar.

**Was du baust:** Ein ESP32-Mikrocontroller liest Sensoren (Temperatur, Luftfeuchte, pH, EC) und schaltet über Relais Heizung, Pumpe und Lüfter. Alles wird per WLAN an deinen Server geschickt.

```
+------------------+          +-----------------------+          +------------------+
|   230V Steckdose |          |    ESP32 DevKit V1    |          |   Sensoren       |
|   (Wand)         |          |    (kleiner Computer) |          |   (Messen)       |
|                  |          |                       |          |                  |
|  Heizung 500W    |<--SSR--->|  GPIO 36  -----> DHT22#1       |                  |
|  (warm)          |          |  (liest Temperatur)   |          |                  |
|                  |          |  GPIO 39  -----> DHT22#2       |                  |
|  Pumpe 20W       |<--SSR--->|  GPIO 34  -----> EC-Sonde      |                  |
|  (Wasser)        |          |  (Nährstoffe)         |          |                  |
|                  |          |  GPIO 35  -----> pH-Sonde       |                  |
|  Lüfter 15W      |<--SSR--->|  (Säuregehalt)        |          |                  |
|  (Frischluft)    |          |  GPIO 21  -----> OLED SDA      |                  |
|                  |          |  (Display)            |          |                  |
|  Abluft 25W      |<--SSR--->|  GPIO 22  -----> OLED SCL      |                  |
|  (Abluft)        |          |  (Display)            |          |                  |
+------------------+          |  GPIO  4  -----> Relais Heizung |                  |
                              |  (schaltet 230V ein/aus)        |                  |
                              |  GPIO  5  -----> Relais Pumpe   |                  |
                              |  GPIO 16  -----> Relais Lüfter  |                  |
                              |  GPIO 17  -----> Relais Abluft  |                  |
                              +-----------------------+          +------------------+
                                        |
                                        | WiFi (Funk)
                                        v
                              +-----------------------+
                              |  Dein PC / Raspberry Pi|
                              |  (Server + Dashboard)  |
                              +-----------------------+
```

---

## Wichtig vor dem Start

### Was ist ein ESP32?

Der ESP32 ist ein kleiner Computer auf einer Platine. Er hat:
- **Pins** = kleine Metall-Löcher zum Anschließen von Kabeln
- **GPIO** = programmierbare Pins (General Purpose Input/Output)
- **WLAN** = eingebautes WiFi-Modul
- **3.3V** = seine Betriebsspannung (wichtig!)

**Der ESP32 verträgt maximal 3.3 Volt.** Mehr Spannung zerstört ihn sofort.

### Was du brauchst (Einkaufsliste)

| Teil | Anzahl | Was es macht | ca. Preis |
|------|--------|--------------|-----------|
| ESP32 DevKit V1 | 1x | Der "Hauptcomputer" | 8–12 € |
| DHT22 / AM2302 | 2x | Misst Temperatur + Luftfeuchte | 3–5 €/Stk |
| OLED 0.96" I2C | 1x | Kleines Display | 3–5 € |
| SSR-Relais DC-Input | 4x | Schaltet 230V ohne Klicken | 4–8 €/Stk |
| Netzteil 5V/3A | 1x | Stromversorgung | 5–10 € |
| Steckbrett + Kabel | 1 Set | Zum Testen ohne Löten | 5–10 € |
| Widerstände 10kΩ | 5x | Kleine elektronische Teile | 1 € |
| Multimeter | 1x | Zum Messen von Spannung | 10–20 € |

> **Tipp:** Kauf zuerst ein "ESP32 Starter Kit". Darin sind meist schon Kabel, Widerstände und ein Steckbrett enthalten.

### Drei goldene Regeln

1. **Nie mehr als 3.3V an den ESP32** — er würde sofort kaputt gehen
2. **Alle GND (Minus) müssen verbunden sein** — sonst funktioniert nichts richtig
3. **230V nur mit Schutz** — nie direkt am ESP32, immer über SSR-Relais

---

## Teil 1: Den ESP32 mit dem Computer verbinden (nur zum Programmieren)

Bevor du irgendwelche Sensoren anschließt, verbindest du den ESP32 mit deinem PC:

1. **USB-Kabel** in den ESP32 stecken (das kleine Quadrat auf der Platine)
2. **USB-Kabel** in den PC stecken
3. Im Gerätemanager (Windows) sollte unter "Ports (COM & LPT)" ein neuer Port erscheinen, z.B. "USB-SERIAL CH340 (COM3)"

> **Wichtig:** Einige USB-Kabel übertragen nur Strom, keine Daten. Wenn kein Port erscheint, probiere ein anderes Kabel.

---

## Teil 2: Den ESP32 mit Strom versorgen

Der ESP32 braucht **3.3 Volt**. Das bekommst du so:

### Variante A: Über USB (zum Testen)
- Einfach das USB-Kabel am PC lassen
- Der PC liefert 5V, ein kleiner Chip auf dem ESP32 wandelt das in 3.3V um
- **Nachteil:** Funktioniert nur solange der PC an ist

### Variante B: Eigenes Netzteil (für den Dauerbetrieb)
- Kauf ein **Netzteil 5V/3A** (z.B. ein altes Handy-Ladegerät)
- Oder ein **Netzteil 12V** mit einem "Step-Down-Modul" (wandelt 12V in 5V um)
- Das 5V-Netzteil schließt du an den **5V-Pin** und **GND** des ESP32 an

```
Netzteil 5V/3A
     |
     +---> 5V-Pin auf ESP32
     |
     +---> GND-Pin auf ESP32
```

> **Was bedeutet GND?** GND = Ground = Masse = Minus-Pol = 0 Volt. Alle GND-Punkte im Schaltkreis müssen miteinander verbunden sein, damit der Strom einen geschlossenen Kreis hat. Stell dir vor wie Wasser: Es muss von Plus nach Minus fließen können.

---

## Teil 3: Temperatur- und Luftfeuchte-Sensoren anschließen (DHT22)

### Was ist ein DHT22?

Der DHT22 ist ein kleiner blauer oder weißer Sensor mit 4 Beinchen. Er misst:
- **Temperatur** (wie warm ist es?)
- **Luftfeuchte** (wie viel Wasser ist in der Luft?)

### Schritt-für-Schritt Anschluss

Jeder DHT22 hat 4 Beinchen. Von vorne gesehen (mit dem Gitter nach vorne):

```
DHT22 von vorne (Gitternetzseite):

  Pin 1    Pin 2    Pin 3    Pin 4
  VCC      DATA     (leer)   GND
  (Plus)   (Signal)          (Minus)
```

**Beinchen 1 (VCC)** → An **3.3V** am ESP32
**Beinchen 2 (DATA)** → An **GPIO 36** (erster Sensor) oder **GPIO 39** (zweiter Sensor)
**Beinchen 3** → Nicht benutzt (manchmal fehlt es auch)
**Beinchen 4 (GND)** → An **GND** am ESP32

### Was ist ein Pull-up-Widerstand?

Der DATA-Pin des DHT22 braucht einen kleinen Helfer: einen **10kΩ-Widerstand**. Der sorgt dafür, dass das Signal stabil ist und nicht "flattert".

**So schließt du ihn an:**

```
ESP32 3.3V ----------+---------+
                     |         |
                     |      [10kΩ]  <-- Widerstand
                     |         |
ESP32 GPIO 36 -------+---------+----> DHT22 Pin 2 (DATA)
                     |
ESP32 GND -----------+----> DHT22 Pin 4 (GND)
                              DHT22 Pin 1 (VCC)
```

> **Einfach erklärt:** Der Widerstand "zieht" den DATA-Pin auf 3.3V hoch, wenn keine Daten gesendet werden. Das ist wie ein Türstopper: Er hält die Tür offen, wenn niemand dagegen drückt.

### Zusammenfassung DHT22-Anschluss

| DHT22-Beinchen | Wohin? | Farbe (Vorschlag) |
|----------------|--------|-------------------|
| Pin 1 (VCC) | ESP32 3.3V | Rot |
| Pin 2 (DATA) | ESP32 GPIO 36 | Gelb |
| Pin 4 (GND) | ESP32 GND | Schwarz |
| 10kΩ-Widerstand | Zwischen 3.3V und DATA | — |

> **Wichtig:** Der DHT22 darf **nie an 5V** angeschlossen werden! Nur 3.3V, sonst geht er kaputt.

---

## Teil 4: EC- und pH-Sensoren (für die Nährstoffe)

### Was sind EC und pH?

- **EC** = Electrical Conductivity = Leitfähigkeit des Wassers. Je mehr Dünger drin ist, desto höher der EC-Wert.
- **pH** = Säuregehalt. Cannabis wächst am besten bei pH 5.8–6.5.

### Die Sonden liefern ein schwaches Signal

Die EC- und pH-Sonden geben ein sehr schwaches elektrisches Signal ab — zu schwach, damit der ESP32 es direkt lesen kann. Du brauchst einen **Verstärker** dazwischen.

### Der LM358-Verstärker (oder fertiges Modul kaufen)

**Für Anfänger empfohlen:** Kauf ein fertiges **EC/pH-Sensor-Modul** für Arduino/ESP32. Das Modul enthält bereits den Verstärker und alle nötigen Bauteile. Du schließt einfach an:

```
Fertiges EC/pH-Modul:

Modul VCC -----> ESP32 3.3V (oder 5V, je nach Modul! Datenblatt lesen!)
Modul GND -----> ESP32 GND
Modul EC-Out --> ESP32 GPIO 34
Modul pH-Out --> ESP32 GPIO 35
Sonde EC -----> An EC-Anschluss des Moduls
Sonde pH -----> An pH-Anschluss des Moduls
```

> **Vorsicht:** Prüfe im Datenblatt deines Moduls, ob es 3.3V oder 5V braucht! Wenn 5V: Nicht direkt an ESP32-Pins anschließen, sondern nur an die 5V-Stromversorgung.

### Was ist ein ADC?

**ADC** = Analog-Digital-Wandler. Der ESP32 kann keine stufenlosen Spannungen direkt verstehen. Der ADC wandelt eine Spannung (z.B. 1.5 Volt) in eine Zahl (z.B. 1861) um, die der Computer versteht.

**Wichtig:** Der ADC des ESP32 verträgt maximal **3.3V**. Wenn dein Modul mehr ausgibt, brauchst du einen Spannungsteiler (zwei Widerstände) dazwischen.

---

## Teil 5: OLED-Display anschließen

### Was ist ein OLED?

OLED = Organic Light Emitting Diode. Ein kleines Display (oft 0.96 Zoll / 2,4 cm), auf dem du Text und Zahlen sehen kannst. Es zeigt z.B. Temperatur, Luftfeuchte und Relais-Status an.

### I2C — die Datenstraße

I2C ist ein Standard, über den mehrere Geräte mit nur 2 Kabeln kommunizieren können. Stell dir vor wie eine Autobahn: Viele Autos (Geräte) fahren auf derselben Straße (2 Kabeln).

**Das OLED hat 4 Anschlüsse:**

| OLED-Pin | Wohin? | Was es macht |
|----------|--------|--------------|
| VCC | ESP32 3.3V | Stromversorgung |
| GND | ESP32 GND | Masse (Minus) |
| SDA | ESP32 GPIO 21 | Daten-Senden |
| SCL | ESP32 GPIO 22 | Daten-Takt (Taktgeber) |

> **SDA und SCL** sind die I2C-"Autobahn". Auf dem ESP32 DevKit V1 sind das immer GPIO 21 und 22. Das gilt für alle I2C-Geräte (OLED, manchmal auch Lichtsensor BH1750).

---

## Teil 6: Relais anschließen (230V schalten)

### Was ist ein Relais?

Ein Relais ist ein elektrischer Schalter. Der ESP32 kann keine 230V-Steckdose direkt schalten — dafür ist er viel zu schwach. Stattdessen schaltet der ESP32 ein Relais, und das Relais schaltet die 230V.

**Stell dir vor:** Der ESP32 drückt auf einen Lichtschalter (Relais), und der Lichtschalter macht das Licht an (230V-Gerät).

### SSR vs. mechanisches Relais

| | SSR (Solid State Relay) | Mechanisches Relais |
|---|-------------------------|---------------------|
| **Geräusch** | Lautlos | Klackt beim Schalten |
| **Lebensdauer** | Fast unbegrenzt | Verschleißt nach ~100.000 Schaltungen |
| **Preis** | 4–8 € | 2–4 € |
| **Empfohlen für** | Heizung (schaltet oft) | Selten schaltende Geräte |

**Für dieses Projekt: SSR kaufen.** Die Heizung schaltet oft ein und aus — ein mechanisches Relais würde schnell kaputt gehen.

### SSR anschließen

Ein SSR hat zwei Seiten:
- **Eingangsseite (DC, schwach)** = wird vom ESP32 gesteuert
- **Ausgangsseite (AC, stark)** = schaltet die 230V

```
SSR-Eingang (links):
  + -----> ESP32 GPIO 4 (oder 5, 16, 17)
  - -----> ESP32 GND

SSR-Ausgang (rechts):
  L (Phase) -----> von der Steckdose
  L' -------------> zur Heizung
  
  N (Null) -------> direkt von Steckdose zur Heizung (nicht durch das Relais!)
```

### Was ist Phase und Nullleiter?

- **Phase (L)** = die "scharfe" Leitung, die Spannung führt (230V gegen Erde)
- **Nullleiter (N)** = die "Rückleitung", die zum Kraftwerk zurückführt
- **Erde (PE)** = Schutzleiter, leitet Strom bei Fehler ab

> **Lebenswichtig:** Die 230V-Seite darf nur von einer Elektrofachkraft verdrahtet werden! Wenn du unsicher bist: Lass es einen Profi machen. Strom kann töten.

### Sicherungen — der Schutz vor Feuer

Jeder 230V-Stromkreis braucht eine **Sicherung**. Sie schaltet ab, wenn zu viel Strom fließt (z.B. bei einem Kurzschluss).

**So berechnest du die richtige Sicherung:**

```
Strom (Ampere) = Leistung (Watt) / Spannung (Volt)

Beispiel Heizung:
  500 Watt / 230 Volt = 2.2 Ampere
  → Sicherung: 5A (etwas höher als der Normalstrom)
```

| Gerät | Leistung | Strom | Sicherung |
|-------|----------|-------|-----------|
| Heizung | 500W | 2.2A | **5A träge** |
| Wasserpumpe | 20W | 0.1A | **2A träge** |
| Umluft-Lüfter | 15W | 0.1A | **1A träge** |
| Abluft-Lüfter | 25W | 0.1A | **1A träge** |

> **"Träge"** bedeutet: Die Sicherung schaltet nicht sofort bei einem kurzen Stromstoß ab (z.B. beim Einschalten eines Motors), sondern erst bei dauerhaft zu hohem Strom.

---

## Teil 7: Die komplette Verkabelung

### Stromversorgung (alle Komponenten)

```
                    Netzteil 5V/3A
                         |
         +---------------+---------------+
         |               |               |
         v               v               v
    ESP32 5V-Pin    SSR-Eingänge     Step-Down
    (wird intern        (+/-)       (wenn 12V-Netzteil)
    zu 3.3V)                            |
         |                              v
         v                         ESP32 3.3V
    Sensoren VCC                       |
    OLED VCC                       Sensoren VCC
                                   OLED VCC

    GND (alle miteinander verbunden!):
    ESP32 GND <---> Sensoren GND <---> OLED GND <---> SSR-GND <---> Netzteil GND
```

> **Nochmal: Alle GND müssen verbunden sein!** Wenn du das vergisst, funktionieren Sensoren nicht oder zeigen falsche Werte.

### Die Pin-Belegung im Überblick

Kopiere diese Tabelle ausgedruckt und häng sie neben deinen Aufbau:

| Kabel-Farbe | ESP32 Pin | Wohin? | Was es macht |
|-------------|-----------|--------|--------------|
| Rot | 3.3V | DHT22 #1 VCC | Strom für Sensor 1 |
| Rot | 3.3V | DHT22 #2 VCC | Strom für Sensor 2 |
| Rot | 3.3V | OLED VCC | Strom für Display |
| Gelb | GPIO 36 | DHT22 #1 DATA | Temperatur/Luftfeuchte 1 |
| Grün | GPIO 39 | DHT22 #2 DATA | Temperatur/Luftfeuchte 2 |
| Orange | GPIO 34 | EC-Modul Out | Nährstoff-Messung |
| Blau | GPIO 35 | pH-Modul Out | Säure-Messung |
| Weiß | GPIO 21 | OLED SDA | Display-Daten |
| Grau | GPIO 22 | OLED SCL | Display-Takt |
| Braun | GPIO 4 | SSR #1 | Heizung ein/aus |
| Rosa | GPIO 5 | SSR #2 | Pumpe ein/aus |
| Lila | GPIO 16 | SSR #3 | Umluft ein/aus |
| Schwarz | GPIO 17 | SSR #4 | Abluft ein/aus |
| Schwarz | GND | Alle GND-Pins | Gemeinsame Masse |

---

## Teil 8: Testen, bevor du 230V anschließt

### 8.1 Nur den ESP32 testen (USB am PC)

1. **Nur** den ESP32 mit USB am PC anschließen
2. **Keine** 230V-Geräte angeschlossen!
3. Arduino IDE öffnen, Sketch hochladen
4. Serieller Monitor öffnen (115200 Baud)

Du solltest sehen:
```
WiFi......
Connected: 192.168.178.101
```

Wenn das kommt: Der ESP32 funktioniert und ist im WLAN.

### 8.2 Sensoren testen (noch ohne 230V)

1. DHT22 anschließen (3.3V, GND, DATA + 10kΩ)
2. Sketch neu hochladen
3. Im Seriellen Monitor sollten Temperatur-Werte erscheinen

Wenn "nan" (not a number) angezeigt wird:
- 10kΩ-Widerstand vergessen?
- Kabel falsch angeschlossen?
- DHT22 defekt?

### 8.3 Relais testen (ohne 230V!)

1. SSR anschließen (nur Eingangsseite: GPIO 4 und GND)
2. **Noch keine 230V an der Ausgangsseite!**
3. Im Seriellen Monitor sollte stehen: `relayHeater: 0` (aus)
4. Wenn der Server sagt "Heizung an", sollte stehen: `relayHeater: 1` (an)
5. Am SSR-Eingang kannst du mit dem Multimeter messen: ca. 3.3V = an, 0V = aus

### 8.4 Erst dann 230V

Nur wenn alle Tests bestanden sind:

1. **Strom an der Steckdose ausschalten** (Sicherung raus)
2. 230V-Verdrahtung vom Elektriker prüfen lassen
3. **Einen** Kanal testen (zuerst die Pumpe, die ist ungefährlich)
4. Wenn das funktioniert: Nächsten Kanal testen

---

## Teil 9: Wenn etwas nicht funktioniert

| Problem | Erste Hilfe | Was du prüfst |
|---------|-------------|---------------|
| ESP32 wird nicht erkannt | Anderes USB-Kabel probieren | Manche Kabel laden nur, übertragen keine Daten |
| DHT22 zeigt "nan" | 10kΩ-Widerstand prüfen | Liegt er wirklich zwischen DATA und 3.3V? |
| OLED zeigt nichts | I2C-Adresse prüfen | Adresse 0x3C oder 0x3D? |
| Relais schaltet nicht | Multimeter an SSR-Eingang | Kommen 3.3V an, wenn es eingeschaltet sein soll? |
| ESP32 startet neu und wieder | Stromversorgung prüfen | Netzteil liefert genug Ampere? (min. 1A bei 3.3V) |
| WiFi-Verbindung fehlt | `device_secrets.h` prüfen | SSID und Passwort stimmen? |
| Alles sehr langsam | Zu viele Serial.print | Debug-Ausgaben reduzieren |

---

## Glossar (Begriffe für Anfänger)

| Begriff | Einfache Erklärung |
|---------|-------------------|
| **GPIO** | "General Purpose Input Output" = ein programmierbarer Pin am Mikrocontroller. Kann Eingang (lesen) oder Ausgang (schreiben) sein. |
| **GND** | "Ground" = Masse = Minus-Pol = 0 Volt. Alle GNDs müssen verbunden sein. |
| **VCC** | Die positive Betriebsspannung (hier 3.3V oder 5V). |
| **Pull-up-Widerstand** | Ein Widerstand, der einen Pin auf 3.3V "zieht", wenn niemand anderes etwas macht. Stabilisiert das Signal. |
| **ADC** | "Analog-Digital-Converter" = wandelt eine Spannung in eine Zahl um. |
| **I2C** | Ein Kommunikationsstandard mit 2 Kabeln (SDA + SCL), über den mehrere Geräte sprechen können. |
| **SSR** | "Solid State Relay" = elektronisches Relais ohne bewegliche Teile. Lautlos und langlebig. |
| **Spannungsteiler** | Zwei Widerstände, die eine Spannung herunterteilen (z.B. von 5V auf 3.3V). |
| **Baud** | Die Geschwindigkeit der Datenübertragung (hier 115200 = 115200 Bits pro Sekunde). |
| **Kurzschluss** | Wenn Plus und Minus direkt verbunden sind. Erzeugt viel Wärme und kann Feuer verursachen. |

---

## Checkliste für Anfänger

Drucke diese Seite aus und hak Schritt für Schritt ab:

### Vorbereitung
- [ ] Alle Teile gekauft (siehe Einkaufsliste oben)
- [ ] ESP32 wird vom PC erkannt (COM-Port im Gerätemanager)
- [ ] Arduino IDE installiert + ESP32-Board-Paket

### Verkabelung (ohne 230V!)
- [ ] ESP32 per USB am PC
- [ ] DHT22 #1 an GPIO 36 (inkl. 10kΩ)
- [ ] DHT22 #2 an GPIO 39 (inkl. 10kΩ)
- [ ] OLED an GPIO 21 (SDA) und GPIO 22 (SCL)
- [ ] EC/pH-Modul an GPIO 34 und GPIO 35
- [ ] SSR-Eingänge an GPIO 4, 5, 16, 17 (nur Eingangsseite!)
- [ ] Alle GND-Punkte verbunden
- [ ] Alle 3.3V-Leitungen prüfen (nie 5V an Sensoren!)

### Software-Test
- [ ] Sketch hochgeladen
- [ ] Serieller Monitor zeigt WiFi-Verbindung
- [ ] Temperatur-Werte werden angezeigt
- [ ] Relais-Status ändert sich, wenn Server Befehl sendet

### 230V (nur mit Elektriker!)
- [ ] Elektriker hat Verdrahtung geprüft
- [ ] FI-Schalter testet korrekt
- [ ] Jede Sicherung korrekt dimensioniert
- [ ] Erst Pumpe testen, dann Lüfter, dann Heizung
- [ ] Nach 30 Minuten: Nichts wird warm, was nicht warm werden soll