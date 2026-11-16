# Migration des Branches hardware-v2-firmware (16.11.2026)

## Grundsatz

**Alle 53 zusätzlichen historischen Commits bleiben erhalten und sind als
zu berücksichtigende Entwicklungsarbeit eingestuft.** Der alte Branch wird
nicht gelöscht. Die Dateiänderungen werden jedoch nicht ungeprüft an
`main` angehängt: `main` enthält seit PR #9 die neuere
`firmware/Sensor`, `firmware/Remote` und `firmware/C3` Struktur sowie
den Sensor-PCB-Prüfkandidaten.

**GitHub-Vergleich (Stand dieser Inventarisierung):**
`hardware-v2-firmware` ist 53 Commits vor `main`, aber auch 231 Commits
hinter `main`. 31 Dateipfade weisen Unterschiede **gegenüber dem
gemeinsamen Merge-Vorfahren** auf. Diese Vergleichsstatus beweisen für sich
genommen **nicht**, ob eine Datei im heutigen `main` fehlt.

- <https://github.com/d4sd1ng/grow_controller/pull/10> — kompletter historischer
  Branch, Draft-PR zur lückenlosen Nachverfolgung.
- <https://github.com/d4sd1ng/grow_controller/pull/11> — aktive, selektive
  Portierung auf aktuellem `main`.

## Vollständige Pfad-Inventarliste (31)

| Datei | GitHub-Differenzstatus | Migrationsstatus |
|---|---|---|
| `.env.example` | modified | Konfiguration/Abhängigkeiten: gezielt abgleichen |
| `.gitignore` | modified | Konfiguration/Abhängigkeiten: gezielt abgleichen |
| `README.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |
| `current_state.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |
| `data_manager.py` | modified | Server-/Datenfunktion: gezielten Diff und Integrationstest durchführen |
| `decision_log.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |
| `env_config.py` | modified | Teilweise integriert im PR #11; Tests offen |
| `esp32_controller.py` | modified | Server-/Datenfunktion: gezielten Diff und Integrationstest durchführen |
| `export_firmware_secrets_from_env.py` | modified | Konfiguration/Abhängigkeiten: gezielt abgleichen |
| `firmware/README.md` | modified | Historische Firmware: Funktionsabgleich mit Sensor/Remote/C3 nötig |
| `firmware/cyd_grow_display/cyd_grow_display.ino` | modified | Historische Firmware: Funktionsabgleich mit Sensor/Remote/C3 nötig |
| `firmware/cyd_remote_control/cyd_remote_control.ino` | modified | Historische Firmware: Funktionsabgleich mit Sensor/Remote/C3 nötig |
| `firmware/growroom_telemetry_client/growroom_telemetry_client.ino` | modified | Historische Firmware: Funktionsabgleich mit Sensor/Remote/C3 nötig |
| `firmware/ph_calibration_tool/ph_calibration_tool.ino` | added | Historische Firmware: Funktionsabgleich mit Sensor/Remote/C3 nötig |
| `firmware/powerbox_actuator/powerbox_actuator.ino` | added | Historische Firmware: Funktionsabgleich mit Sensor/Remote/C3 nötig |
| `installation_instructions.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |
| `main.py` | modified | Server-/Datenfunktion: gezielten Diff und Integrationstest durchführen |
| `ml_controller.py` | modified | Server-/Datenfunktion: gezielten Diff und Integrationstest durchführen |
| `project_overview.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |
| `requirements.txt` | modified | Konfiguration/Abhängigkeiten: gezielt abgleichen |
| `schedule_manager.py` | modified | Server-/Datenfunktion: gezielten Diff und Integrationstest durchführen |
| `schedules.json` | modified | Konfiguration: Schema/Daten/Kompatibilität prüfen |
| `sensor_node_client.py` | added | Teilweise integriert im PR #11; Tests offen |
| `system_tests.py` | modified | Testübernahme bzw. Testanpassung prüfen |
| `task_contract.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |
| `telemetry_api.py` | modified | Teilweise integriert im PR #11; Tests offen |
| `telemetry_payload.py` | modified | Teilweise integriert im PR #11; Tests offen |
| `telemetry_store.py` | modified | Server-/Datenfunktion: gezielten Diff und Integrationstest durchführen |
| `test_extended.py` | modified | Testübernahme bzw. Testanpassung prüfen |
| `wiring_instructions.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |
| `wiring_plan_v2.md` | modified | Dokumentationsabgleich mit aktuellem Stand nötig |

## Bereits direkt aus den Quellen abgeglichene Funktionen

- `sensor_node_client.py`: auf `main` zuvor nicht vorhanden;
  vom alten Branch übernommen. Der Client enthält
  `get_summary`, `set_targets` und `set_mode`. **Lediglich der
  Lesepfad ist in der Server-API aktiviert.** Kein neuer
  hardwaresteuernder Pfad wurde aktiviert.
- `env_config.py`: fehlende `IOT_SENSOR_HOST`,
  `IOT_SENSOR_PORT`, `IOT_API_KEY`-Getter portiert.
  Bereits vorhandene `GROW_DB_BACKEND`- und
  `GROW_POSTGRES_DSN`-Getter blieben erhalten.
- `telemetry_api.py`: die alten **lesenden**
  `/api/v1/control/state`- und `/api/v1/mobile/summary`-Fallbacks
  für den Sensor-HTTP-Status sind im aktuellen Server integriert.
  Die schreibenden Steuer-APIs bleiben bis zum Abgleich mit
  dem aktuellen Sensor-/C3-Interlock unverändert.
- `telemetry_payload.py`: alte Validierungsbereiche für
  VPD, TDS, einzelne DHT22-Sensoren, DS18B20 und
  Messspannungen portiert. `light` und
  `lamp_cooling` aus verschachtelter Telemetrie
  ergänzt; historischer `water_pump`-Alias beibehalten.
- `telemetry_store.py`: alter Branch speichert `light`
  als eigenständige SQLite-Spalte statt `pump`.
  Aktuelles `main` hat SQLite- und optionale
  PostgreSQL-Anbindung. Eine Übernahme erfordert eine
  kompatible Migration bereits vorhandener Bestandsdaten,
  nicht das Ersetzen des Schemas.
- `schedule_manager.py`: alter Branch entfernt
  `water_pump`-Zeitpläne; `main` enthält diesen Legacy-Eintrag
  teilweise noch. Beim Vereinheitlichen dürfen keine
  aktiven Gerätebefehle ungeprüft verändert werden.
- **Firmware:** historische OLED-/Powerbox-Codes sind nicht der
  aktuelle Ersatz für das Sensor-/Remote-/C3-Layout.
  Die enthaltenen Funktionen sind zu prüfen, nicht die
  veralteten Sketch-Verzeichnisse wortgleich zurückzubringen.

## Freigabebedingungen

1. Regressionstests und Python-Syntaxprüfung von PR #11
   **erfolgreich**, derzeit noch **nicht bestätigt**.
2. Server-API-Schnittstellen und Rückfallebenen mit den
   tatsächlichen aktuellen Firmware-Endpunkten abgleichen.
3. Bestehende Postgres- und SQLite-Datenhaltung samt
   Datenmigration getrennt testen; keine historischen
   Tabellen oder Felder stillschweigend löschen.
4. Aktorsteuerung nur nach gesondertem Interlock- und
   Schnittstellentest integrieren; nicht mit Datenabruf
   gleichsetzen.
5. Jede der oben genannten 31 Pfadabweichungen abhaken
   und dokumentieren, welche Funktion heute wo erhalten ist.
6. Erst nach abgeschlossener Migration `hardware-v2-firmware`
   archivieren oder auf ausdrückliche Entscheidung löschen.

**Die alte Branch-Historie bleibt die maßgebliche Quelle für
noch nicht portierte Funktionen.**
