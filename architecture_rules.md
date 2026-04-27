# Architektur-Regeln — Growroom Control System

Diese Regeln haben oberste Prioritaet. Alle Aenderungen am Repository muessen sich daran orientieren.

---

## Minimal-Invasive-Rule

Wenn ein Fehler klar einem Modul zuordenbar ist, wird nur dieses Modul geaendert.
Keine Seiteneffekte in anderen Komponenten.

## Interface-Rule

Oeffentliche Rueckgaben, JSON-Felder, Funktionsvertraege und Dateiformate werden nicht still geaendert.
Breaking Changes erfordern Freigabe und Versionsanpassung.

## Dependency-Rule

Keine neue Bibliothek, kein neues Framework, kein neuer externer Dienst ohne Freigabe.
Fuer Firmware: Neue Arduino-Libraries muessen dokumentiert und getestet werden.

## Separation-Rule

UI, Orchestrierung, Datenladen, Kernlogik, Reporting und Runtime-Steuerung bleiben getrennt.

- **Firmware**: Sensor-Lesung, Regelung, Relais-Schaltung, Display, Netzwerk sind separate Module.
- **Server**: API, Dashboard, Datenbank, ML sind separate Module.

## State-Rule

Kein versteckter Zustand.
Keine implizite Entscheidung.
Keine Seiteneffekte ohne Kennzeichnung.

## Data-Rule

Alle strukturierten Daten bleiben definiert, versionierbar und nachvollziehbar.
JSON nur mit festen Feldern und klarer Bedeutung.
Telemetry-Payload hat definiertes Schema.

## Determinism-Rule

Keine Zufallslogik ohne Seed.
Keine zeitabhaengige Logik ohne explizite Eingabe.
Keine stillen Heuristikwechsel.
Hysterese-Grenzen sind hart codiert oder konfiguriert, nicht dynamisch berechnet ohne Grund.

## Evidence-Rule

Aktueller Repo-Zustand wird aus Runtime-Dateien, Report-Artefakten und belegbaren Logs abgeleitet.
Statusdokumente muessen aktualisiert werden, sobald neue Artefakte alte Annahmen widerlegen.

## Top-Down-Rule

Die Reihenfolge bleibt:

```
Sensor-Lesung -> Validierung -> Regelung (Hysterese/PID) -> Relais-Schaltung -> Telemetry -> Dashboard/Logging
```

Kein Einstieg in spaetere Stufen ohne fruehere Freigabe.
Beispiel: Relais-Logik darf nicht vor Sensor-Validierung implementiert werden.

## Ownership-Rule

Jede Schicht hat genau eine Kernverantwortung:

| Schicht | Verantwortung |
|---------|--------------|
| **Sensoren** | Erzeugt Messwerte (Temp, RH, EC, pH, Lux, Case-Temp) |
| **Validierung** | Prueft Plausibilitaet, erkennt Sensor-Fehler |
| **Regelung** | Erzeugt Hysterese-Entscheidungen oder PID-Ausgaben |
| **Relais** | Schaltet Ausgaenge basierend auf Regelung oder Manual-Override |
| **Telemetry** | Sendet Daten an Server, empfaengt Control-State |
| **API** | Empfaengt Telemetry, stellt Control-State bereit |
| **Dashboard** | Zeigt Daten, erlaubt MANUAL-Override |

Keine Schicht darf die Kernverantwortung einer anderen still uebernehmen.

## Hard-Definition-Rule

Weiche Begriffe duerfen nicht direkt in Produktionslogik eingehen.
Jeder zentrale Fachbegriff braucht:
- Definition
- Input
- Regel
- Output
- Invalidation
- Failure Case

Beispiele: VPD, EC, pH, Hysterese, Notfall-Kuehlung, MANUAL-Override.

## State-Machine-Rule

Fuer Steuerungs-Modi (AUTO/MANUAL), Relais-Zustaende und Alarm-Zustaende sind nur definierte Zustandsfamilien erlaubt.
Freitext darf Diagnose ergaenzen, aber keine Primaerlogik tragen.

## Reason-Code-Rule

Jede Relais-Freigabe und jede Relais-Blockade muss ueber feste reason_codes und blocker_codes nachvollziehbar sein.
Keine freien Texte als primaere Entscheidungsbasis.

Beispiel-Reason-Codes:
- `hysteresis_temp_low` — Heizung AN wegen Temperatur-Hysterese
- `hysteresis_rh_high` — Zirkulation AN wegen Luftfeuchte
- `manual_override` — Relais durch Dashboard/CYD manuell gesetzt
- `emergency_temp` — Abluft AN wegen Ueberhitzung
- `sensor_error` — Relais im sicheren Zustand wegen Sensor-Fehler

## Input-Validation-Rule

Fehlende Pflichtdaten werden nicht als neutral behandelt.
Fehlende Pflichtdaten erzeugen einen expliziten Fehlerzustand.

Beispiel: DHT22-Fehler -> `air_temp = null`, `reason = "dht_timeout"`, Regelung faellt auf sicheren Zustand zurueck.

## Execution-Sequence-Rule

Ein Steuer-Befehl ist nur gueltig, wenn die definierte Kette vollstaendig ist:

```
Sensor-Lesung -> Plausibilitaetspruefung -> Regel-Entscheidung -> Relais-Schaltung -> Bestaetigung
```

Einzelne Signale (z.B. nur ein Temperaturwert ohne RH) reichen nicht fuer eine vollstaendige Entscheidung.

## Precedence-Rule

Hoehere Ebenen schlagen niedrigere:

1. **Notfall-Shutdown** (z.B. Ueberhitzung, Sensor-Fehler)
2. **MANUAL-Override** (Dashboard/CYD)
3. **AUTO-Regelung** (ESP32 Hysterese)
4. **Server-Automatik** (Python PID/Hysterese — nur Fallback/Simulation)

Notfall-Shutdown schlaegt alles.
MANUAL-Override schlaegt AUTO.
AUTO-Regelung auf dem ESP32 schlaegt Server-Automatik.

## Change-Size-Rule

Pro Schritt maximal 3 Dateien.
Maximal 1 logisch zusammenhaengender Patch.
Maximal 1 oeffentliches Interface.

## Validation-Rule

Nach jeder Aenderung muessen definierte Checks laufen.
Mindestens Syntaxpruefung und ein passender Smoke-Run.
Fuer Firmware: Kompilier-Check in Arduino IDE.
Fuer Python: `python -m unittest` muss passen.

## Stop-Rule

Beim ersten echten Fehler sofort stoppen.
Keine Folgeaenderungen auf kaputter Basis.

## Report-Rule

Vor jedem Patch kurz nennen:
- Ziel
- betroffene Dateien
- Risiko
- kleinster naechster Schritt

## Staleness-Rule

Sensor-Daten und Steuer-Entscheidungen brauchen explizite Timeout- und Cancel-Regeln.
Ein Sensor-Wert darf nicht unbegrenzt gueltig bleiben.

Beispiel: DHT22-Wert aelter als 30 Sekunden -> als veraltet markieren, Regelung auf sicheren Modus.

## Config-Explicitness-Rule

Schwellenwerte wie Hysterese-Grenzen, Soll-Temperaturen, ADC-Kalibrierung, EC/pH-Validierungsgrenzen und Alert-Timeout gehoeren in explizite Konfiguration oder dokumentierte Defaults.
Keine versteckten Magic Numbers ohne Fachbezug.

## Metrics-Rule

Sensor-Kennzahlen, Relais-Zustaende und Alert-Daten duerfen sich nicht still fachlich widersprechen.
Wenn Artefakte widerspruechlich sind, wird zuerst die Kennzahlenherkunft geklaert, bevor an Regel-Logik geschraubt wird.

## Quality-Rule

Vorhandene Messwerte sind kein Erfolgskriterium fuer sich.
Wenn die Sensor-Qualitaet fachlich unzureichend ist (z.B. unkalibrierte EC/pH-Werte), wird das ausdruecklich so dokumentiert und nicht als Fortschritt schoengeredet.

## Decision-Trace-Rule

Relais-Schaltungen und Steuer-Entscheidungen muessen nachvollziehbar sein.
Pflicht sind strukturierte Zustaende, reason_codes und Invalidierungsreferenzen.
Ohne Decision Trace gilt die Diagnose als unvollstaendig.

## Nicht erlaubt

- Grossrefactor ohne Freigabe
- stille Interface-Aenderung
- neue Abhaengigkeit ohne Freigabe
- gleichzeitige Aenderungen an mehreren Schichten
- Reparatur auf unmarkierter Vermutung
- Weiterarbeit auf nachweislich veralteter Statusannahme
- stille Neutral-Fallbacks bei fehlenden Pflichtdaten
