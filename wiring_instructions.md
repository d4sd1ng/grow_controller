# Verdrahtung — Hinweis

Diese Datei enthielt den alten Hardwarestand mit einem einzelnen ESP32, alten GPIOs und einer geschalteten Wasserpumpe. Dieser Stand ist **veraltet und darf für den Aufbau nicht mehr verwendet werden**.

Der verbindliche Plan ist:

**[wiring_plan_v2.md](wiring_plan_v2.md)**

Dort stehen aktuell:

- Sensor-ESP: 3x DHT22, 2x DS18B20, ADS1115, PH4502C, CQRobot TDS, BH1750 und OLED;
- alle GPIO-Zuordnungen;
- alle vorgesehenen Pull-up-/Pulldown-Widerstände;
- der 10-kOhm/20-kOhm-Spannungsteiler für den pH-Eingang;
- Stromkasten-ESP mit vier SSR-Kanälen;
- LIGHT / CIRC / EXH / HEAT;
- Wasserpumpe als Dauerläufer ohne SSR/GPIO;
- 230-V-Seite als Funktionsblock;
- Kabelplanung und Sensorpositionen;
- pH-Kalibrierung und Service-Sketch.

Keine Verdrahtungsangabe aus einer älteren Version dieser Datei sollte parallel zum V2-Plan verwendet werden.
