"""Regression checks for selected hardware-v2 server functions.

Run: python -m unittest discover -s tests -p 'test_hardware_v2_server_migration.py' -v
These tests never connect to the hardware or issue physical actuation commands.
"""
from __future__ import annotations

import os
import unittest
from unittest.mock import Mock, patch

import sensor_node_client
from telemetry_payload import normalize_ingest_payload


class SensorNodeHttpReadTests(unittest.TestCase):
    @patch.dict(os.environ, {"IOT_SENSOR_HOST": "192.0.2.10", "IOT_SENSOR_PORT": "8081",
                             "IOT_API_KEY": "test-key"}, clear=False)
    @patch("sensor_node_client.httpx.get")
    def test_reads_sensor_json_with_key(self, fake_get):
        response = Mock()
        response.json.return_value = {
            "ok": True,
            "control": {"control_mode": "manual", "targets": {"light": True}},
            "reading": {"ph": 6.0, "ec": 1.4},
        }
        fake_get.return_value = response
        result = sensor_node_client.get_summary()
        self.assertEqual(result["reading"]["ph"], 6.0)
        fake_get.assert_called_once_with(
            "http://192.0.2.10:8081/api/local/summary",
            headers={"X-Telemetry-Key": "test-key"},
            timeout=2.5,
        )

    @patch.dict(os.environ, {"IOT_SENSOR_HOST": ""}, clear=False)
    @patch("sensor_node_client.httpx.get")
    def test_unconfigured_sensor_does_not_call_network(self, fake_get):
        self.assertIsNone(sensor_node_client.get_summary())
        fake_get.assert_not_called()

    @patch.dict(os.environ, {"IOT_SENSOR_HOST": "192.0.2.10"}, clear=False)
    @patch("sensor_node_client.httpx.get")
    def test_failed_sensor_get_falls_back_without_exception(self, fake_get):
        fake_get.side_effect = sensor_node_client.httpx.ConnectError("offline")
        self.assertIsNone(sensor_node_client.get_summary())


class HardwareV2PayloadTests(unittest.TestCase):
    def test_preserves_modern_lights_and_legacy_pump_alias(self):
        result = normalize_ingest_payload({
            "ph": 6.1,
            "relays": {"light": True, "heater": False, "lamp_cooling": True},
            "water_pump": False,
            "device_info": {"firmware_version": "test"},
        })
        self.assertTrue(result["light"])
        self.assertTrue(result["lamp_cooling"])
        self.assertFalse(result["pump"])
        self.assertEqual(result["device_info"]["firmware_version"], "test")


class TelemetryReadOnlyIntegrationTests(unittest.TestCase):
    def test_live_sensor_summary_is_prioritized_for_control(self):
        import telemetry_api
        with patch.object(telemetry_api, "_require_api_key"), \
             patch.object(telemetry_api, "get_sensor_summary", return_value={
                 "ok": True, "control": {"control_mode": "manual", "targets": {"light": True}}
             }), \
             patch.object(telemetry_api, "get_dm") as dm:
            result = telemetry_api.get_control_state(x_telemetry_key=None)
            self.assertEqual(result["control_mode"], "manual")
            self.assertTrue(result["targets"]["light"])
            dm.assert_not_called()

    def test_fallback_to_database_when_sensor_unavailable(self):
        import telemetry_api
        fake_dm = Mock()
        fake_dm.get_control_state.return_value = {"control_mode": "auto", "targets": {}}
        fake_dm.get_latest_reading.return_value = {"ph": 6.5}
        fake_dm.get_alerts_recent.return_value = []
        with patch.object(telemetry_api, "_require_api_key"), \
             patch.object(telemetry_api, "get_sensor_summary", return_value=None), \
             patch.object(telemetry_api, "get_dm", return_value=fake_dm):
            result = telemetry_api.get_mobile_summary(
                alert_limit=20, x_telemetry_key=None
            )
        self.assertEqual(result["reading"], {"ph": 6.5})
        self.assertEqual(result["control"]["control_mode"], "auto")

    def test_mobile_summary_uses_live_sensor_reading(self):
        import telemetry_api
        fake_dm = Mock()
        fake_dm.get_control_state.return_value = {"control_mode": "auto", "targets": {}}
        fake_dm.get_latest_reading.return_value = {"ph": 6.5}
        fake_dm.get_alerts_recent.return_value = []
        with patch.object(telemetry_api, "_require_api_key"), \
             patch.object(telemetry_api, "get_sensor_summary", return_value={
                 "ok": True, "control": {"control_mode": "manual", "targets": {"light": True}},
                 "reading": {"ph": 5.9, "ec": 1.5}
             }), \
             patch.object(telemetry_api, "get_dm", return_value=fake_dm):
            result = telemetry_api.get_mobile_summary(
                alert_limit=20, x_telemetry_key=None
            )
        self.assertEqual(result["reading"]["ph"], 5.9)
        self.assertTrue(result["control"]["targets"]["light"])


class AdditionalHardwareV2CompatibilityTests(unittest.TestCase):
    def test_legacy_postgres_getters_keep_current_backend(self):
        from env_config import (
            get_grow_database_backend, get_grow_database_schema,
            get_grow_database_url, get_grow_postgres_dsn,
        )
        with patch.dict(os.environ, {
            "GROW_DB_BACKEND": "sqlite",
            "GROW_POSTGRES_DSN": "postgresql://example.invalid/current",
            "GROW_DATABASE_URL": "postgresql://example.invalid/legacy",
            "GROW_DATABASE_SCHEMA": "grow_controller",
        }, clear=False):
            self.assertEqual(get_grow_database_backend(), "sqlite")
            self.assertEqual(
                get_grow_postgres_dsn(), "postgresql://example.invalid/current"
            )
            self.assertEqual(
                get_grow_database_url(), "postgresql://example.invalid/legacy"
            )
            self.assertEqual(get_grow_database_schema(), "grow_controller")

    def test_extended_sensor_validation_is_available(self):
        data = normalize_ingest_payload({
            "vpd": 1.5,
            "tds_ppm": 750,
            "ph_voltage": 2.5,
            "dht1_temperature": 21.3,
            "dht2_humidity": 55.0,
            "ds18b20_1_temperature": 20.1,
        })
        self.assertEqual(data["vpd"], 1.5)
        self.assertEqual(data["tds_ppm"], 750.0)
        self.assertEqual(data["ph_voltage"], 2.5)
        self.assertEqual(data["dht1_temperature"], 21.3)
        self.assertEqual(data["dht2_humidity"], 55.0)
        self.assertEqual(data["ds18b20_1_temperature"], 20.1)

    def test_out_of_range_extra_sensor_reading_is_rejected(self):
        data = normalize_ingest_payload({"vpd": 9, "tds_ppm": -5})
        self.assertIsNone(data["vpd"])
        self.assertIsNone(data["tds_ppm"])


class LegacyLightTelemetryStoreTests(unittest.TestCase):
    def test_sqlite_light_status_without_schema_replacement(self):
        from datetime import datetime
        from telemetry_store import TelemetryStore
        store = TelemetryStore(":memory:", retention_days=0)
        store.insert_reading(
            {"timestamp": datetime.now(), "ph": 6.2},
            extras={"light": True, "lamp_cooling": False},
        )
        row = store.fetch_latest()
        self.assertEqual(row["ph"], 6.2)
        self.assertTrue(row["light"])
        self.assertFalse(row["lamp_cooling"])
        self.assertTrue(row["extras"]["light"])
        self.assertIn("pump", row)

    def test_nonboolean_extra_does_not_become_light_status(self):
        from datetime import datetime
        from telemetry_store import TelemetryStore
        store = TelemetryStore(":memory:", retention_days=0)
        store.insert_reading(
            {"timestamp": datetime.now()},
            extras={"light": "unknown"},
        )
        self.assertNotIn("light", store.fetch_latest())

    def test_sqlite_historical_light_is_nullable(self):
        from datetime import datetime, timedelta
        from telemetry_store import TelemetryStore
        store = TelemetryStore(":memory:", retention_days=0)
        now = datetime.now()
        store.insert_reading({"timestamp": now - timedelta(seconds=1)},
                             extras={"light": True})
        store.insert_reading({"timestamp": now}, extras={"legacy_only": True})
        df = store.query_range(now - timedelta(minutes=1),
                               now + timedelta(minutes=1))
        self.assertEqual(list(df["light"]), [True, None])
        self.assertIn("lamp_cooling", df.columns)

    def test_unknown_light_is_not_coerced_false(self):
        from telemetry_payload import relay_extra_status
        self.assertIsNone(relay_extra_status({"other": True}, "light"))
        self.assertIsNone(relay_extra_status('{"light": "off"}', "light"))
        self.assertTrue(relay_extra_status('{"light": true}', "light"))



if __name__ == "__main__":
    unittest.main()
