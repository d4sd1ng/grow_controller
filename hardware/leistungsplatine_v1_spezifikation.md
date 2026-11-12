# Leistungsplatine — konsolidierter Konstruktionsstand, 12.11.2026

> **Nicht fertigungstauglich.** Mechanische Vorgaben und Kanalzahl sind konsolidiert. Es existieren noch keine elektrisch verifizierten KiCad-Schaltplan-/Gerberdaten. Keine Leitungsführung der 230-V-Seite auf dieser Niederspannungsplatine.

## Fixe Eckdaten

- Leiterplatten-Konstruktionsmaß: **180 × 110 mm**
- 4 Befestigungsbohrungen Ø **3,2 mm**, Lochmittenabstand **168 × 98 mm**
- **ESP32-C3 SuperMini ohne Display/OLED**
- **PCA9685** für Ausgangserweiterung über **GPIO7/SDA und GPIO10/SCL**
- **6 Dosierpumpen** à 12 V DC / 3 W, ohne Umpolung, jeweils eine MOSFET-Stufe und ein zweipoliger Anschluss; keine Reservepumpe
- 6 × 3 W = 18 W nominal, 1,5 A bei 12 V nur für die Pumpen; Anlaufströme offen
- 5 Steueranschlüsse für **externe** SSR: 1× SSR25 Licht, 4× SSR10 Abluft, Umluft, Heizung, Wasserpumpe
- **4 PC-Gehäuselüfter**, 120 mm, 12 V, 3-polig; **2 unabhängig geschaltete Zweiergruppen**, jeweils zwei Lüfter parallel
- Nur EIN/AUS, kein PWM-Drehzahlsteller, keine Tach-Auswertung; vier 3-polige Anschlusspositionen, TACH bleibt unverbunden
- Filterwechsel-Pausentaster für Wasserpumpe; blaue LED bei abgeschalteter Wasserpumpe
- 12-V-Versorgung, Abwärtsregler für ESP32-Versorgung

## Bisher dokumentierte Pinbelegung

| Funktion | ESP32-C3 oder PCA9685 |
| --- | --- |
| SSR25 Licht | GPIO0 |
| SSR10 Abluft | GPIO1 |
| SSR10 Umluft | GPIO3 |
| SSR10 Heizung | GPIO4 |
| SSR10 Wasserpumpe | GPIO5 |
| Lüftergruppe A | GPIO6 |
| I²C SDA | GPIO7 |
| I²C SCL | GPIO10 |
| PCA9685 /OE bzw. Dosierfreigabe | GPIO20 |
| Dosierpumpen P1–P6 | PCA9685 OUT0–OUT5 |

**Nur als mögliche Belegung, noch nicht freigegeben:** Lüftergruppe B an PCA9685 OUT6; Filtertaster an GPIO21; blaue LED an PCA9685 OUT7. Die PCA9685-/OE-Gesamtabschaltung würde dann auch die zweite Lüftergruppe und die LED betreffen; Funktionssicherheit muss im fertigen Schaltplan geklärt werden.

## Fertigungssperren

Die Dokumentation hat keine verbindlichen Hersteller-/Teilenummern und Footprints für den PCA9685 (IC vs. Breakout), die acht MOSFET-Stufen, Freilauf-/Schutzbauteile, Buchsenleisten/Klemmen und den 12-V-auf-5-V-Wandler. Ebenso fehlen abgesicherte Lüfter-Stromdaten, reale Pumpen-Anlauf-/Blockierströme und die SSR-Eingangsstrommessung. Ohne diese sind Schaltplan, Leiterbahnbreiten, Stückliste und Gehäusepassung nicht technisch verifizierbar.

Die aktuell im Draft-PR #2 angelegte C3-Firmware nutzt noch keine PCA9685-Ansteuerung und ist bewusst hardwareseitig verriegelt. **Vor Bestellung** sind EDA-Schaltplan, vollständige Footprints, Routing, ERC/DRC, Gerätefreigabe und Fertigungsausgabe erforderlich.
