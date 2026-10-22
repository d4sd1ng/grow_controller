#!/usr/bin/env python3
"""Host-seitiger Regressionstest fuer die pH-Startalarm-Logik.

Extrahiert die echten C++-Funktionen aus dem Produktions-Sketch und kompiliert
sie mit g++ gegen kleine Arduino/Sensor-Stubs (keine Hardware erforderlich).
Ausfuehren: python3 tests/test_ph_alarm_startup.py
"""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SKETCH = ROOT / "firmware/growroom_telemetry_client/growroom_telemetry_client.ino"
source = SKETCH.read_text(encoding="utf-8")
begin_marker = "enum class PhAlarm : uint8_t { Normal, Low, High, Fault };"
end_marker = "static bool phTestActive = false;"
assert source.count(begin_marker) == 1, "pH-Zustandsblock nicht eindeutig"
assert source.count(end_marker) == 1, "pH-Testblock nicht eindeutig"
production_code = source.split(begin_marker, 1)[1].split(end_marker, 1)[0]
production_code = begin_marker + production_code

prelude = r"""
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <math.h>
static constexpr float PH_LOW = 5.5f;
static constexpr float PH_HIGH = 6.2f;
static constexpr float PH_HYST = 0.10f;
static constexpr uint8_t PH_CONFIRM = 3;
struct { float ph = NAN; bool adsOk = false; } sensors;
bool phCalibrated = false;
struct FakeSerial {
  template<typename ...Args>
  void printf(const char *, Args...) {}
} Serial;
"""
tests = r"""
static void resetTest() {
  phAlarm = PhAlarm::Fault;
  phPending = PhAlarm::Fault;
  phSamples = 0;
  phAlarmConfirmed = false;
  phAlertPending = false;
  phCalibrated = false;
  sensors.adsOk = false;
  sensors.ph = NAN;
}

static void readValid(float ph) {
  phCalibrated = true;
  sensors.adsOk = true;
  sensors.ph = ph;
  updatePhAlarm();
}

int main() {
  // Spaetes WLAN: selbst mit millis > 10 s darf ein einzelner gueltiger
  // Startwert keinen voreiligen sensor_fault-POST ausloesen.
  resetTest();
  readValid(5.80f);
  assert(!phAlarmConfirmed && !phAlertPending);
  readValid(5.80f);
  assert(!phAlarmConfirmed && !phAlertPending);
  readValid(5.80f);
  assert(phAlarmConfirmed && phAlarm == PhAlarm::Normal);
  assert(!phAlertPending);  // auch kein erfundenes "wieder normal"

  // Echter Startfehler: fehlender Sensor oder Kalibrierung wird signalisiert.
  resetTest();
  sensors.adsOk = true;
  sensors.ph = 5.80f;
  updatePhAlarm();
  assert(phAlarmConfirmed && phAlarm == PhAlarm::Fault && phAlertPending);

  // pH zu hoch: erst nach drei realen Messungen bestaetigt.
  resetTest();
  readValid(6.40f); readValid(6.40f);
  assert(!phAlarmConfirmed && !phAlertPending);
  readValid(6.40f);
  assert(phAlarmConfirmed && phAlarm == PhAlarm::High && phAlertPending);

  // pH zu niedrig: identische Bestaetigung.
  resetTest();
  readValid(5.30f); readValid(5.30f);
  assert(!phAlarmConfirmed && !phAlertPending);
  readValid(5.30f);
  assert(phAlarmConfirmed && phAlarm == PhAlarm::Low && phAlertPending);

  // Ein nachtraeglich ausfallender Sensor erzeugt sofort einen echten Alarm.
  resetTest();
  readValid(5.80f); readValid(5.80f); readValid(5.80f);
  assert(!phAlertPending);
  sensors.adsOk = false;
  updatePhAlarm();
  assert(phAlarm == PhAlarm::Fault && phAlertPending);

  // Recovery vom echten Sensorfehler nach drei gueltigen Messungen.
  phAlertPending = false; // simuliert erfolgreichen POST
  readValid(5.80f); readValid(5.80f);
  assert(phAlarm == PhAlarm::Fault && !phAlertPending);
  readValid(5.80f);
  assert(phAlarm == PhAlarm::Normal && phAlertPending);

  // Einzelne Stoerung waehrend der Startbestaetigung: gueltige Zaehler
  // duerfen nicht faelschlich bis Normal durchgezaehlt werden.
  resetTest();
  readValid(5.80f);
  sensors.adsOk = false;
  updatePhAlarm();
  assert(phAlarmConfirmed && phAlertPending && phAlarm == PhAlarm::Fault);

  std::puts("PASS: 7 pH-Startup-/Fehler-/Recovery-Szenarien");
}
"""
with tempfile.TemporaryDirectory(prefix="grow_ph_alarm_") as tmp:
    source_cpp = Path(tmp) / "ph_alarm_test.cpp"
    binary = Path(tmp) / "ph_alarm_test"
    source_cpp.write_text(prelude + production_code + tests, encoding="utf-8")
    subprocess.run(
        ["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
         str(source_cpp), "-o", str(binary)],
        check=True,
    )
    subprocess.run([str(binary)], check=True)
