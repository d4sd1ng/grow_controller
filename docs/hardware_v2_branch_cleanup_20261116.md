# Bereinigung hardware-v2-firmware — 16.11.2026

## Ausgangslage

- Historischer Branch: `hardware-v2-firmware`, alter HEAD
  `1cdeabe403927b4614d18e946273d2fa8019dd4b`.
- 53 nicht auf `main` enthaltene historische Commits.
- Entwicklung auf `main` inzwischen weit fortgeschritten:
  `firmware/Sensor`, `firmware/Remote`, `firmware/C3`,
  neue Sensor-PCB, optionaler PostgreSQL-Speicher.
- Vor Bereinigung wurde die gesamte alte Branch-Historie
  unter **`archive/hardware-v2-firmware-20261116`** gesichert.
  Archiv und alter Branch waren nach GitHub-Vergleich *identisch*.

## Portierung

- PR #9: aktive Sensor-/Remote-/C3-Firmware und PCB-Stand nach
  `main` gemergt.
- PR #11: historischer Sensor-HTTP-Lesepfad und Konfiguration,
  Telemetrie-Feldkompatibilität, Lichtwerte für SQLite/PostgreSQL,
  ML-Behandlung historischer Zustände, read-only Dashboard,
  nicht-destruktive SQLite-Migrationshilfe, Regressionstests
  nach `main` gemergt.
- Fachliche Disposition aller 31 vom GitHub-Vergleich gemeldeten
  Dateipfade: `docs/hardware_v2_migration_outcome.md`.
- Historische, nicht portierte Hardware-Steuerdateien bleiben
  vollständig über den Archivbranch erreichbar.

## Durchgeführte Branch-Bereinigung

Am 16.11.2026 wurde `hardware-v2-firmware` nach
archivgesichertem Altstand `1cdeabe403927b4614d18e946273d2fa8019dd4b`
per **Force-with-Lease** auf den neuesten `main` gesetzt.
Vorher wurde Draft-PR #10 geschlossen, **ohne** seinen
konfliktbehafteten historischen Dateibaum nach `main` zu mergen.

`fix/restore-serial-sensor-readings-20261114` und
`integrate/hardware-v2-server-20261116` wurden danach mit
**normalem Fast-Forward** auf `main` gesetzt.
GitHub Compare meldete bei diesen **drei aktiven Branches
jeweils 0 ahead und 0 behind**.

`archive/hardware-v2-firmware-20261116` ist absichtlich ein
abweichender Archivbranch mit den vollständigen 53 Commits;
er wird **nicht gelöscht oder überschrieben**.

**Wichtig:** Dieser Branch-Reset ist ein Aufräumen der Branch-Refs,
kein inhaltlicher Vollmerge aller historischen Codezeilen.
Die funktionsfähigen Serverteile wurden gezielt in PR #11
portiert. Der Bestand historischer, hardwarewirksamer Funktionen
bleibt im Archiv und wird nicht automatisch aktiviert.

## Weiterhin nicht nachgewiesen

GitHub Actions erstellt zurzeit fehlgeschlagene Jobs mit
**null ausgeführten Schritten und fehlenden Logs**,
darunter einen unabhängigen Einzeilen-Runner-Probe.
Ein tatsächlicher Python-/Firmware-/KiCad-Testlauf
ist damit **nicht bestätigt**. Keine grüne Freigabe behaupten.
Der SQLite-Kopier-/ALTER-COLUMN-Pfad wurde separat anhand
eines lokalen Beispielschemas geprüft; das ist kein vollständiger
Regressionstest der importierten Anwendung.

Hardwarewirksame alte Powerbox-/Fernsteuerbefehle wurden
nicht automatisch aktiviert. Steuerungs- und Verriegelungstests
der aktuellen Geräte stehen weiterhin aus.
