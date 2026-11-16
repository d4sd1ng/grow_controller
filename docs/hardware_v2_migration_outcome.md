# Abgleich aller 31 historischen Dateipfade

Stand 2026-11-16. Quelle: GitHub Compare `main...hardware-v2-firmware` nach
PR #9. **Alle 53 Commits bleiben im alten Branch vollständig erhalten.**
Diese Datei beschreibt, was im Integrationsbranch umgesetzt ist; sie
behauptet ausdrücklich **keinen** bestandenen CI-Test und **keine**
Wiederherstellung von hardwarewirksamen Alt-Funktionen.

| Historischer Dateipfad | Festgestellter Umgang |
|---|---|
| `.env.example` | Portiert: Sensor-Port 8081 und vorhandene Parameter; kein Geheimnis übernommen |
| `.gitignore` | Identisch zwischen Branches, keine Änderung nötig |
| `README.md` | Altes Architekturkapitel nicht kopiert; aktuelle Version bleibt gültig |
| `current_state.md` | Historischer Plan, nicht als heutiger Hardwarestand übernehmen |
| `data_manager.py` | Alte Lichtwert-Speicherung über heutiges extras_json in beiden DB-Backends erhalten |
| `decision_log.md` | Historische Entscheidungen bleiben im Git-Verlauf |
| `env_config.py` | Sensor-API- und Legacy-PostgreSQL-Getter übernommen; aktuelle DB-Auswahl bewahrt |
| `esp32_controller.py` | Lichtkanal in moderner Simulation bereits vorhanden, Wasserpumpe bleibt |
| `export_firmware_secrets_from_env.py` | Alte Sketch-Pfade durch moderne Sensor/Remote/C3 ersetzt; nicht rückgängig gemacht |
| `firmware/README.md` | Moderne Firmwarestruktur beibehalten; Alttext ist nicht aktueller Stand |
| `firmware/cyd_grow_display/cyd_grow_display.ino` | Historischer CYD-Sketch; moderner Remote-Abgleich durchgeführt, keine Altdatei überlagert |
| `firmware/cyd_remote_control/cyd_remote_control.ino` | Historischer Remote-Sketch; heutiger Remote benutzt mobile Server-API |
| `firmware/growroom_telemetry_client/growroom_telemetry_client.ino` | Historischer Sensor-Sketch; moderne Sensor-API-Endpunkte gegen Vergleich geprüft |
| `firmware/ph_calibration_tool/ph_calibration_tool.ino` | Aktuelle Kalibrier-Implementierung auf main bereits vorhanden |
| `firmware/powerbox_actuator/powerbox_actuator.ino` | Veralteter Powerbox-Sketch; echte Aktorfunktionen NICHT durch Altcode überschrieben |
| `installation_instructions.md` | Historische Installationsangaben nicht ungeprüft über aktuelle Geräte kopiert |
| `main.py` | Lesendes Sensor-ESP-Livepanel portiert, keine zusätzliche Hardwaresteuerung aktiviert |
| `ml_controller.py` | Licht + Wasserpumpe als getrennte historische Datenfelder unterstützt, Unbekannte werden nicht als AUS klassifiziert |
| `project_overview.md` | Historische Architektur bleibt im Git-Verlauf |
| `requirements.txt` | Bisherige psycopg-pool-Abhängigkeit wieder berücksichtigt |
| `schedule_manager.py` | Alte automatische Entfernung von Wasserpumpenplänen nicht übernommen; bestehende Konfiguration bleibt unangetastet |
| `schedules.json` | Aktuelle benutzerseitige Zeitpläne beibehalten, keine automatisch durchgeführte Löschung |
| `sensor_node_client.py` | Kompletten historischen Sensor-HTTP-Client übernommen; GET aktiv, POST nicht neu aktiviert |
| `system_tests.py` | Bestehende Tests erhalten; erweiterte neue Migrationssuite ergänzt |
| `task_contract.md` | Historische Spezifikation nicht als heutiger Vertrag ausgegeben |
| `telemetry_api.py` | Live-GET in control/state + mobile/summary übernommen, sichere DB-Fallbacks |
| `telemetry_payload.py` | VPD, TDS, einzelne DHT-/DS18-Felder, Licht- und Lampenkühlungsstatus, Pumpenalias unterstützt |
| `telemetry_store.py` | Alte Lichtwerte in Zusatzdatenlesepfad + Timeseries; optionales SQLite-Kopierwerkzeug erstellt |
| `test_extended.py` | Bestehende Tests erhalten; separate Regressionstests für Legacy-Verhalten |
| `wiring_instructions.md` | Alte Verdrahtung ist nicht aktueller Sensor-PCB-Entwurf |
| `wiring_plan_v2.md` | Historischer Pinout: keine unbestätigte Beschaltung ins heutige PCB übertragen |

## Neu hinzugefügte Artefakte im Integrationsbranch

- `sensor_node_client.py` — alter Sensor-HTTP-Client als wiederverwendbares Modul.
- `scripts/migrate_hardware_v2_sqlite.py` — Quelle nur lesend,
  Ziel immer **neue** Datei, Lichtzustand nach `extras_json`,
  fehlende Pumpenspalte ergänzt, keine Umdeutung `light` → `pump`.
- `tests/test_hardware_v2_server_migration.py` —
  17 Regressionstests, einschließlich GET-API, ML, Nullwerte.
- `tests/test_legacy_sqlite_migration.py` —
  3 Tests für Datenübernahme, Überschreibvermeidung und ungültiges JSON.
- `.github/workflows/server-integration-tests.yml` —
  Python-Syntaxcheck und Unit-Tests, zusätzlich einfacher Runner-Probe.
- `docs/hardware_v2_migration_inventory.md` —
  Ursprünglicher 31-Pfade-Diff.

## Grenzen und offene Freigaben

**Wichtig:** Die historischen Schaltaktionen über Dashboard,
Sensor-ESP und separate Powerbox wurden *nicht* als kompatibel
bestätigt. Die aktuelle `Sensor`/`Remote`/`C3`-Firmware samt
Interlocks bleibt maßgeblich. Keine alte PIN- oder Netzspannungslogik
wurde wieder aktiviert.

Ein alter SQLite-Datenbankbestand wurde hier **nicht eingelesen oder
verändert**, weil kein Original-Datenbankfile zur Verfügung steht.
Das Migrationswerkzeug ist explizit und nicht destruktiv;
vor produktiver Nutzung sind Probelauf und Datenabgleich erforderlich.

Die CI-Läufe in GitHub Actions haben derzeit `failure` ohne
ausgeführte Schritte/Logs. Ein *separater, nur aus einem Python-
Ausdruck bestehender Runner-Probe* scheiterte im gleichen Lauf.
Das beweist **nicht**, dass der Anwendungscode korrekt oder falsch
ist. Tests sind bis zu einem ausgeführten Testlauf **unbestätigt**.

Die Branch-Historie sollte erst bereinigt werden, wenn der
kompatible Code vollständig abgenommen wurde. Ein Merge allein
beseitigt keine inhaltlich widersprüchliche Hardwaresteuerung.
