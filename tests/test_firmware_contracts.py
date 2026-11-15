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

    def test_headless_sensor_and_visible_readings(self):
        # Sensor board has no OLED; I2C must still serve ADS1115 and BH1750.
        for removed in ("Adafruit_GFX.h", "Adafruit_SSD1306.h",
                        "SSD1306_", "OLED_ADDR", "OLED_REFRESH_MS",
                        "oledOk", "drawOled(", "display.begin("):
            with self.subTest(removed=removed):
                self.assertNotIn(removed, self.sensor)
        self.assertIn("Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, 100000)", self.sensor)
        self.assertIn("ads.begin(0x48, &Wire)", self.sensor)
        self.assertIn("bhWire.begin(PIN_BH_SDA, PIN_BH_SCL, 50000)", self.sensor)
        self.assertIn("ds18b20.requestTemperatures();", self.sensor)
        self.assertIn("sensors.lux = readBh1750();", self.sensor)
        self.assertIn("static void printLiveSensorValues()", self.sensor)
        self.assertEqual(self.sensor.count("  printLiveSensorValues();"), 2)
        self.assertIn('updateSmallReading(homeWater, reading["water_temperature"]', self.remote)
        self.assertIn('updateSmallReading(homeLux, reading["extras"]["illuminance_lux"]', self.remote)

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
        self.assertIn('grow_clock::evaluateCycle', self.sensor)
        self.assertIn('stopCompletedCycle()', self.sensor)
        self.assertIn('if (state.finished && !cycleStopped)', self.sensor)
        self.assertIn('if (growPhase != desiredPhase)', self.sensor)
        self.assertIn('prefs.putBool("stopped", stopped)', self.sensor)
        self.assertIn('d["cycle_stopped"]', self.c3)
        self.assertIn('doc["cycle_stopped"]', self.sensor)
        self.assertIn('reading["extras"]["cycle_stopped"]', self.remote)
        self.assertIn('setGrowWeekManually(selected)', self.sensor)
        self.assertIn('setGrowWeekManually(week)', self.sensor)
        self.assertNotIn('die Zeit-basierte Wochenfortschaltung ist separat', self.sensor)

    def test_three_dht_sensor_fusion_for_climate_and_vpd(self):
        self.assertIn('#include "air_fusion.h"', self.sensor)
        self.assertIn('grow_air::fuseThree(samples)', self.sensor)
        self.assertIn('updateFusedAir();', self.sensor)
        self.assertIn('sensors.airValidSensorCount = combined.validCount;', self.sensor)
        self.assertIn('const float airC = sensors.air.tempC;', self.sensor)
        self.assertIn('const float rh = sensors.air.rh;', self.sensor)
        self.assertIn('const float t = sensors.air.tempC;', self.sensor)
        self.assertIn('if (!sensors.air.ok || !isfinite(sensors.vpd)', self.sensor)
        self.assertIn('if (!sensors.air.ok && targets.heater)', self.sensor)
        self.assertIn('reading["air_measurement_quality"]', self.sensor)
        self.assertIn('root["air_measurement_quality"]', self.sensor)
        self.assertIn('reading["air_sensor_warning"]', self.sensor)
        self.assertIn('root["air_sensor_warning"]', self.sensor)
        self.assertIn('reading["air_valid_sensor_count"]', self.sensor)
        self.assertIn('root["air_valid_sensor_count"]', self.sensor)
        self.assertIn('validDht == 0U', self.remote)
        self.assertIn('validDht == 1U', self.remote)
        self.assertIn('validDht == 2U', self.remote)
        self.assertNotIn('if (!sensors.dht[0].ok || !isfinite(sensors.vpd)', self.sensor)
        self.assertNotIn('const float airC = sensors.dht[0].tempC;', self.sensor)

    def test_calibration_exclusive_sensor_acquisition(self):
        # Only the calibration sampler may read ADS1115 during a capture.
        loop = self.sensor.split("void loop() {", 1)[1]
        self.assertIn("pollCalibrationCapture();", loop)
        self.assertIn(
            "if (!calibrationActive()) {\n"
            "    sampleTdsBackground(now);\n"
            "    pollMlx90640(now);\n"
            "  }", loop)
        self.assertIn(
            "if (!calibrationActive() && now - lastSensorRead >= SENSOR_READ_MS)",
            loop)
        self.assertIn(
            "if (!calibrationActive() && now - lastTelemetry >= TELEMETRY_INTERVAL_MS)",
            loop)
        self.assertIn("if (!calibrationActive() && !sensors.adsOk", loop)
        self.assertIn("I2C-Scan waehrend Kalibrierung gesperrt.", self.sensor)
        self.assertIn("static void endCalibrationCapture()", self.sensor)
        self.assertIn("resetEcFilterAfterCalibration();", self.sensor)
        self.assertIn("ds18b20.requestTemperatures();\n  float waterC = NAN;", self.sensor)
        self.assertIn("localServer.handleClient();", loop)
        self.assertIn("applyActuators();", loop)
        self.assertIn("KALIBRIERMODUS ENDE: normale Messungen wieder freigegeben.", self.sensor)

    def test_integrated_ph_ec_calibration(self):
        self.assertIn('#include "calibration_capture.h"', self.sensor)
        for command in ("c == '7'", "c == '4'", "c == 'e'", "c == 's'", "c == 'x'"):
            with self.subTest(command=command):
                self.assertIn(command, self.sensor)
        self.assertIn('handleCalibrationSerial(c)', self.sensor)
        self.assertIn('pollCalibrationCapture();', self.sensor)
        self.assertIn('static void finishCalibration()', self.sensor)
        self.assertIn('grow_cal::PH_SAMPLES', self.sensor)
        self.assertIn('grow_cal::EC_SAMPLES_PER_WINDOW', self.sensor)
        self.assertIn('grow_cal::EC_WINDOWS', self.sensor)
        self.assertIn('grow_cal::median30', self.sensor)
        self.assertIn('grow_cal::ecCalibrationFactor', self.sensor)
        self.assertIn('if (!validDsTempC(sensors.waterTempC))', self.sensor)
        self.assertIn('p.begin("phads_v1", false)', self.sensor)
        self.assertIn('p.begin("ecads_v1", false)', self.sensor)
        self.assertIn('p.putFloat("factor", factor)', self.sensor)
        self.assertIn('p.putFloat("ref_v", voltage)', self.sensor)
        self.assertIn('p.putFloat("temp_c", temperatureC)', self.sensor)
        self.assertIn('p.putBool("valid", true)', self.sensor)
        self.assertIn('p.putBool(hk, true)', self.sensor)
        self.assertIn('reading["calibration_active"]', self.sensor)
        self.assertIn('root["calibration_active"]', self.sensor)
        # Sicherheits- und Kalibrierdaten bleiben unabhaengig vom C3.
        self.assertIn("HARDWARE_OUTPUTS_ARMED = false", self.c3)
        self.assertNotIn('preferences.clear()', self.sensor)

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
