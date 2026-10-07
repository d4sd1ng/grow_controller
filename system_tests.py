
import unittest
from datetime import datetime, timedelta

from fastapi.testclient import TestClient
from iot_sensor import IoTSensor
from data_manager import DataManager
from vpd_calculator import calculate_vpd, is_vpd_in_range, get_vpd_range, calculate_target_humidity
from telemetry_payload import normalize_ingest_payload
from esp32_controller import ESP32Controller, simulate_wlan_communication as esp32_wlan
from gpio_simulator import GPIO, OutputDevice
from pid_controller import PIDController

class TestGrowroomControlSystem(unittest.TestCase):
    def setUp(self):
        self.iot_sensor = IoTSensor()
        self.data_manager = DataManager(db_path=":memory:")
        self.esp32_sensor = ESP32Controller("sensor1")
        self.esp32_relay = ESP32Controller("relay1")
        self.heater = OutputDevice(16)
        self.circulation_vent = OutputDevice(17)
        self.exhaust_vent = OutputDevice(18)
        self.light = OutputDevice(23)

    def test_iot_sensor_data(self):
        sensor_data = self.iot_sensor.read_sensor_data()
        self.assertIn('air_temperature', sensor_data)
        self.assertIn('water_temperature', sensor_data)
        self.assertIn('humidity', sensor_data)
        self.assertIn('ec', sensor_data)
        self.assertIn('ph', sensor_data)

    def test_esp32_sensor_controller(self):
        sensor_data = esp32_wlan(self.esp32_sensor, "send")
        self.assertIn('sensor_data', sensor_data)
        self.assertIn('esp32_sensor1_am2302_1_temperature', sensor_data['sensor_data'])
        self.assertIn('esp32_sensor1_am2302_1_humidity', sensor_data['sensor_data'])
        self.assertIn('esp32_sensor1_ec', sensor_data['sensor_data'])
        self.assertIn('esp32_sensor1_ph', sensor_data['sensor_data'])

    def test_esp32_relay_controller(self):
        relay_data = esp32_wlan(self.esp32_relay, "send")
        self.assertIn('sensor_data', relay_data)
        self.assertIn('esp32_relay_states', relay_data)
        self.assertIn('esp32_relay1_light_intensity', relay_data['sensor_data'])

        command = {
            'relay_states': {
                'heater': True,
                'circulation_vent': False,
                'exhaust_vent': True,
                'light': True
            }
        }
        result = esp32_wlan(self.esp32_relay, "receive", command)
        self.assertEqual(result, {'status': 'success'})
        self.assertTrue(self.esp32_relay.relay_states['heater'])
        self.assertFalse(self.esp32_relay.relay_states['circulation_vent'])
        self.assertTrue(self.esp32_relay.relay_states['exhaust_vent'])
        self.assertTrue(self.esp32_relay.relay_states['light'])

    def test_vpd_calculations(self):
        vpd = calculate_vpd(25, 60)
        self.assertTrue(0 < vpd < 2)
        self.assertTrue(is_vpd_in_range(vpd))
        vpd_range = get_vpd_range('early_grow')
        self.assertEqual(len(vpd_range), 4)
        target_humidity = calculate_target_humidity(25, 'early_grow')
        self.assertTrue(40 < target_humidity < 80)

    def test_control_state_manual_targets(self):
        dm = DataManager(db_path=":memory:", retention_days=365)
        st = dm.get_control_state()
        self.assertEqual(st["control_mode"], "auto")
        dm.set_control_targets_partial({"heater": True}, switch_to_manual=True)
        st2 = dm.get_control_state()
        self.assertEqual(st2["control_mode"], "manual")
        self.assertTrue(st2["targets"]["heater"])
        dm.set_control_mode("auto")
        self.assertEqual(dm.get_control_state()["control_mode"], "auto")

    def test_telemetry_series_from_db(self):
        dm = DataManager(db_path=":memory:", retention_days=365)
        base = datetime.now() - timedelta(hours=2)
        for i in range(30):
            dm.add_data_point(
                {
                    "timestamp": base + timedelta(minutes=i * 3),
                    "air_temperature": 22.0 + i * 0.05,
                    "water_temperature": 21.0,
                    "case_temperature": 22.0,
                    "humidity": 50 + i,
                    "light_level": 100,
                    "ec": 1.0,
                    "ph": 6.0,
                    "circulation_vent": False,
                    "exhaust_vent": False,
                    "heater": False,
                    "light": False,
                }
            )
        end = datetime.now()
        start = end - timedelta(hours=4)
        series = dm.get_telemetry_series(start=start, end=end, max_points=15)
        self.assertGreaterEqual(len(series), 10)
        self.assertIn("vpd", series[0])
        self.assertIn("air_temperature", series[0])

    def test_alerts_log(self):
        dm = DataManager(db_path=":memory:", retention_days=365)
        dm.add_alert("warn", "test alert", "unit")
        al = dm.get_alerts_recent(5)
        self.assertEqual(len(al), 1)
        self.assertIn("test alert", al[0]["message"])

    def test_normalize_ingest_nested_relays(self):
        p = normalize_ingest_payload(
            {
                "air_temperature": 24.0,
                "relays": {"heater": True, "light": False},
            }
        )
        self.assertTrue(p["heater"])
        self.assertFalse(p["light"])

    def test_data_manager_ping_db(self):
        dm = DataManager(db_path=":memory:", retention_days=365)
        dm.ping_db()

    def test_api_health_endpoint(self):
        import telemetry_api as ta

        ta._dm = DataManager(db_path=":memory:", retention_days=365)
        try:
            with TestClient(ta.app) as client:
                r = client.get("/api/v1/health")
            self.assertEqual(r.status_code, 200)
            body = r.json()
            self.assertTrue(body.get("ok"))
            self.assertEqual(body.get("database"), "ok")
        finally:
            ta._dm = None

    def test_get_latest_reading(self):
        dm = DataManager(db_path=":memory:", retention_days=365)
        self.assertIsNone(dm.get_latest_reading())
        now = datetime.now()
        dm.add_data_point(
            {
                "timestamp": now,
                "air_temperature": 21.5,
                "water_temperature": 20.0,
                "case_temperature": 22.0,
                "humidity": 50,
                "light_level": 0,
                "ec": 1.0,
                "ph": 6.0,
                "circulation_vent": False,
                "exhaust_vent": False,
                "heater": False,
                "light": False,
            }
        )
        latest = dm.get_latest_reading()
        self.assertIsNotNone(latest)
        self.assertEqual(float(latest["air_temperature"]), 21.5)

    def test_telemetry_sqlite_roundtrip(self):
        dm = DataManager(db_path=":memory:", retention_days=365)
        now = datetime.now()
        dm.add_data_point(
            {
                "timestamp": now,
                "air_temperature": 25.0,
                "water_temperature": 22.0,
                "case_temperature": 23.0,
                "humidity": 55,
                "light_level": 100,
                "ec": 1.2,
                "ph": 6.0,
                "circulation_vent": False,
                "exhaust_vent": False,
                "heater": True,
                "pump": False,
                "esp_extra_probe": 12.3,
            }
        )
        hist = dm.get_historical_data(now - timedelta(minutes=2), now + timedelta(minutes=2))
        self.assertFalse(hist.empty)
        self.assertEqual(float(hist.iloc[0]["air_temperature"]), 25.0)
        self.assertTrue("extras_json" in hist.columns)
        self.assertIn("esp_extra_probe", hist.iloc[0]["extras_json"])

    def test_gpio_control(self):
        self.heater.on()
        self.assertTrue(GPIO.input(self.heater.pin))
        self.heater.off()
        self.assertFalse(GPIO.input(self.heater.pin))

    def test_pid_controller(self):
        pid = PIDController(kp=1.0, ki=0.1, kd=0.05, setpoint=25.0)
        self.assertEqual(pid.setpoint, 25.0)
        output = pid.compute(24.0)
        self.assertGreater(output, 0)
        output = pid.compute(26.0)
        self.assertLess(output, 0)
        pid.set_setpoint(27.0)
        self.assertEqual(pid.setpoint, 27.0)
        self.assertEqual(pid.integral, 0)

if __name__ == '__main__':
    unittest.main()
