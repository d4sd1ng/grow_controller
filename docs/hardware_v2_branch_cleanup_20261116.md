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

## Aufräummaßnahme

Der Branch `hardware-v2-firmware` darf jetzt auf den
aktuellen `main`-Stand **mit Force-with-Lease** zurückgesetzt
werden, da sein Original-HEAD im oben genannten Archiv
gesichert und alle relevanten Serveränderungen separat
über PR #11 übernommen wurden. Dies ist ein **Branch-Reset**,
**kein vollständiger 53-Commit-Code-Merge**. Die
historischen Commits verbleiben im Archiv, nicht zwingend
im aktiven `main`-Arbeitsbaum.

Die Archivkopie nicht löschen, solange weitere historische
Funktionen gebraucht oder geprüft werden.

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
