"""Non-destructive historical SQLite compatibility tests."""
from __future__ import annotations

import json
import sqlite3
import tempfile
import unittest
from pathlib import Path

from scripts.migrate_hardware_v2_sqlite import migrate


def make_legacy_database(path: Path, *, extras_json: str = '{"vpd": 1.2}') -> None:
    with sqlite3.connect(path) as db:
        db.executescript("""
        CREATE TABLE readings (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ts TEXT NOT NULL,
            air_temperature REAL,
            water_temperature REAL,
            case_temperature REAL,
            humidity REAL,
            light_level REAL,
            ec REAL,
            ph REAL,
            circulation_vent INTEGER NOT NULL,
            exhaust_vent INTEGER NOT NULL,
            heater INTEGER NOT NULL,
            light INTEGER NOT NULL,
            extras_json TEXT
        );
        CREATE TABLE app_control (
            id INTEGER PRIMARY KEY CHECK(id=1),
            control_mode TEXT NOT NULL DEFAULT 'auto',
            heater INTEGER NOT NULL DEFAULT 0,
            circulation_vent INTEGER NOT NULL DEFAULT 0,
            exhaust_vent INTEGER NOT NULL DEFAULT 0,
            light INTEGER NOT NULL DEFAULT 0,
            updated_at TEXT NOT NULL DEFAULT ''
        );
        INSERT INTO app_control(id, control_mode, light)
        VALUES (1, 'manual', 1);
        """)
        db.execute(
            "INSERT INTO readings (ts, light, circulation_vent, exhaust_vent, heater, extras_json) VALUES (?, ?, ?, ?, ?, ?)",
            ("2026-10-01T12:00:00", 1, 0, 0, 0, extras_json),
        )


class LegacySqliteMigrationTests(unittest.TestCase):
    def test_preserves_legacy_light_and_source_database(self):
        with tempfile.TemporaryDirectory() as folder:
            src, dst = Path(folder) / "legacy.db", Path(folder) / "modern.db"
            make_legacy_database(src)
            report = migrate(src, dst)
            self.assertEqual(report["readings"], 1)
            self.assertEqual(report["light_values_preserved"], 1)

            with sqlite3.connect(src) as original:
                self.assertIn(
                    "light",
                    [x[1] for x in original.execute("PRAGMA table_info(readings)")],
                )
                self.assertEqual(
                    original.execute("SELECT light FROM readings").fetchone()[0], 1
                )

            with sqlite3.connect(dst) as modern:
                fields = [x[1] for x in modern.execute("PRAGMA table_info(readings)")]
                self.assertIn("pump", fields)
                self.assertNotIn("light", fields)
                data = json.loads(
                    modern.execute("SELECT extras_json FROM readings").fetchone()[0]
                )
                self.assertTrue(data["light"])
                self.assertEqual(data["vpd"], 1.2)
                self.assertEqual(
                    modern.execute("SELECT light FROM app_control").fetchone()[0], 1
                )
                self.assertEqual(
                    modern.execute("SELECT pump FROM app_control").fetchone()[0], 0
                )
                self.assertEqual(
                    modern.execute("PRAGMA integrity_check").fetchone()[0], "ok"
                )
            from telemetry_store import TelemetryStore
            store = TelemetryStore(dst, retention_days=0)
            latest = store.fetch_latest()
            self.assertEqual(latest["timestamp"], "2026-10-01T12:00:00")
            self.assertTrue(latest["light"])
            self.assertFalse(latest["pump"])

    def test_refuses_to_overwrite_destination(self):
        with tempfile.TemporaryDirectory() as folder:
            src, dst = Path(folder) / "legacy.db", Path(folder) / "modern.db"
            make_legacy_database(src)
            dst.write_text("KEEP", encoding="utf-8")
            with self.assertRaises(FileExistsError):
                migrate(src, dst)
            self.assertEqual(dst.read_text(encoding="utf-8"), "KEEP")

    def test_invalid_legacy_json_aborts_without_touching_source(self):
        with tempfile.TemporaryDirectory() as folder:
            src, dst = Path(folder) / "legacy.db", Path(folder) / "modern.db"
            make_legacy_database(src, extras_json="{invalid")
            with self.assertRaises(ValueError):
                migrate(src, dst)
            self.assertFalse(dst.exists())
            self.assertFalse(Path(str(dst) + ".partial").exists())
            with sqlite3.connect(src) as original:
                self.assertEqual(
                    original.execute("SELECT light FROM readings").fetchone()[0], 1
                )


if __name__ == "__main__":
    unittest.main()
