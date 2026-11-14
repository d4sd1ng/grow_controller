"""Static firmware contract checks; run from repository root:
    python3 -m unittest discover -s tests -p 'test_firmware_contracts.py' -v
These tests do not replace an Arduino compiler or hardware smoke test.
"""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]
SENSOR = ROOT / "firmware" / "Sensor" / "Sensor.ino"
REMOTE = ROOT / "firmware" / "Remote" / "Remote.ino"
C3 = ROOT / "firmware" / "C3" / "C3.ino"


class FirmwareContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.sensor = SENSOR.read_text(encoding="utf-8")
        cls.remote = REMOTE.read_text(encoding="utf-8")
        cls.c3 = C3.read_text(encoding="utf-8")

    def test_sketch_files_and_entrypoints(self):
        for name, source in [("Sensor", self.sensor), ("Remote", self.remote), ("C3", self.c3)]:
            with self.subTest(device=name):
                self.assertEqual(len(re.findall(r"\bvoid\s+setup\s*\(", source)), 1)
                self.assertEqual(len(re.findall(r"\bvoid\s+loop\s*\(", source)), 1)
                self.assertIn('#include "device_secrets.h"', source)
                self.assertIn("#error", source)
        self.assertEqual(sorted(p.name for p in (ROOT / "firmware").glob("*/*.ino")),
                         ["C3.ino", "Remote.ino", "Sensor.ino"])

    def test_sensor_pinout(self):
        pins = {"PIN_DHT1": 25, "PIN_DHT2": 26, "PIN_DHT3": 27,
                "PIN_ONEWIRE": 23, "PIN_PHASE_BUTTON": 16,
                "PIN_PUMP_SERVICE_BUTTON": 19,
                "PIN_I2C_SDA": 21, "PIN_I2C_SCL": 22,
                "PIN_BH_SDA": 13, "PIN_BH_SCL": 14,
                "PIN_RGB_RED": 33, "PIN_RGB_GREEN": 4,
                "PIN_RGB_BLUE": 17, "PIN_PH_BUZZER": 5}
        for name, pin in pins.items():
            with self.subTest(pin=name):
                self.assertRegex(self.sensor, rf"\b{name}\s*=\s*{pin}\s*;")

    def test_five_pumps_and_locked_dosing(self):
        self.assertIn("DOSING_PUMP_COUNT = 5;", self.sensor)
        self.assertIn("PUMP_COUNT = 5;", self.remote)
        self.assertIn("MAX_DOSE_MS[5] = {0, 0, 0, 0, 0}", self.c3)
        self.assertIn("HARDWARE_OUTPUTS_ARMED = false", self.c3)
        self.assertIn("TOUCH_ACTUATION_VERIFIED = false", self.remote)
        self.assertIn('c3ApiError(503, "dosing_not_implemented")', self.sensor)

    def test_c3_sensor_status_contract(self):
        self.assertIn('"/api/local/actuator-status"', self.sensor)
        self.assertIn('sensorUrl("/api/local/actuator-status")', self.c3)
        for field in ["pump_count", "outputs_enabled", "pump_active",
                      "pump_ack_seq", "failsafe"]:
            with self.subTest(field=field):
                self.assertIn(f'd["{field}"]', self.c3)
                self.assertIn(f'doc["{field}"]', self.sensor)
        self.assertIn('d.createNestedObject("actual")', self.c3)
        self.assertIn('doc["actual"]', self.sensor)
        self.assertIn('"/api/local/actuators"', self.sensor)
        self.assertIn('sensorUrl("/api/local/actuators")', self.c3)

    def test_phase_vpd_climate_control(self):
        # W1-2, transition W3-4, flowering W5-13. Actuators only requested.
        for pattern in (
            r'growWeek >= 1 && growWeek <= 2',
            r'growWeek >= 3 && growWeek <= 4',
            r'growWeek >= 5 && growWeek <= 13',
            r'\{0\.4f, 0\.8f, "early_growth", true\}',
            r'\{0\.8f, 1\.2f, "transition", true\}',
            r'\{1\.2f, 1\.6f, "flowering", true\}',
            r'VPD_LEAF_AIR_OFFSET_C = 2\.0f',
            r'const bool tooHumid',
            r'const bool tooDry',
            r'targets\.heater = false',
            r'targets\.circulation = false',
            r'targets\.exhaust = false',
            r'if \(!sensors\.dht\[0\]\.ok',
            r'VPD_HYST = 0\.05f',
        ):
            with self.subTest(pattern=pattern):
                self.assertRegex(self.sensor, pattern)
        self.assertIn('control["vpd_target_min_kpa"]', self.sensor)
        self.assertIn('control["vpd_target_max_kpa"]', self.sensor)
        self.assertIn('d["pump_service_off_confirmed"]', self.c3)
        self.assertIn('doc["pump_service_off_confirmed"]', self.sensor)
        self.assertIn('reading["extras"]["vpd"]', self.remote)
        # Nicht automatisch den Leistungscontroller freischalten.
        self.assertIn("HARDWARE_OUTPUTS_ARMED = false", self.c3)

    def test_automatic_phase_week_clock(self):
        self.assertIn('#include "phase_clock_math.h"', self.sensor)
        self.assertIn('configTime(0, 0, "pool.ntp.org", "time.google.com")', self.sensor)
        self.assertIn('loadPhaseClock();', self.sensor)
        self.assertIn('pollPhaseClock();', self.sensor)
        self.assertIn('phaseStartEpoch', self.sensor)
        self.assertIn('PHASE_CLOCK_NVS', self.sensor)
        self.assertIn('phaseDurationElapsed', self.sensor)
        self.assertIn('phaseClockStatus()', self.sensor)
        self.assertIn('"/api/local/control/phase-clock"', self.sensor)
        self.assertIn('grow_clock::evaluate', self.sensor)
        self.assertIn('setGrowWeekManually(selected)', self.sensor)
        self.assertIn('setGrowWeekManually(week)', self.sensor)
        self.assertNotIn('die Zeit-basierte Wochenfortschaltung ist separat', self.sensor)

    def test_remote_lvgl_and_read_interfaces(self):
        self.assertIn("SCREEN_W = 320", self.remote)
        self.assertIn("SCREEN_H = 240", self.remote)
        self.assertIn('"/api/v1/mobile/summary"', self.remote)
        self.assertIn('"/api/v1/actuator/state"', self.remote)
        self.assertIn("tft.fillScreen(TFT_BLACK)", self.remote)
        self.assertNotIn("lv_label_set_text_static(", self.remote)

    def test_credentials_exporter(self):
        exporter = (ROOT / "export_firmware_secrets_from_env.py").read_text()
        for role in ("Sensor", "Remote", "C3"):
            self.assertIn(f'root / "firmware" / "{role}"', exporter)
        for obsolete in ("cyd_grow_display", "cyd_remote_control",
                         "growroom_telemetry_client", "esp32_c3_power_controller"):
            self.assertNotIn(f'root / "firmware" / "{obsolete}"', exporter)


if __name__ == "__main__":
    unittest.main()
