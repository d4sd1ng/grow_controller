# Verdrahtung — aktueller Stand

Verbindlicher Kleinspannungsplan: **[wiring_plan_v2.md](wiring_plan_v2.md)** (Inhalt wurde am 2026-10-12 auf den **Ein-ESP32-Ausbau V3** aktualisiert; der Dateiname bleibt fuer bestehende Verlinkungen erhalten).

- Ein ESP32 mit integriertem OLED uebernimmt Sensorik und die vier SSR-Steuersignale.
- DS18B20 auf GPIO23; DHT22 auf GPIO25/26/27; BH1750 auf GPIO13/14.
- SSR GPIO17/18/19/33; keine zweite ESP32-Firmware im Betrieb.
- PH4502C und TDS laufen ueber den ADS1115 an GPIO21/22.
- DHT22 und BH1750 sind derzeit **noch nicht verkabelt**.
- 230-V-Installation und Lastkreisverdrahtung gehoeren nicht zu diesem Kleinspannungsplan.

Alte Zwei-ESP32-Verbindungsplaene sind nicht mehr gueltig.
