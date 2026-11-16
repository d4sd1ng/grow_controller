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
            light INTEGER NOT NULL,
            extras_json TEXT
        );
        CREATE TABLE app_control (
            id INTEGER PRIMARY KEY CHECK(id=1),
            control_mode TEXT NOT NULL DEFAULT 'auto',
            light INTEGER NOT NULL DEFAULT 0
        );
        INSERT INTO app_control(id, control_mode, light)
        VALUES (1, 'manual', 1);
        """)
        db.execute(
            "INSERT INTO readings (ts, light, extras_json) VALUES (?, ?, ?)",
            ("2026-10-01T12:00:00", 1, extras_json),
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
