#!/usr/bin/env python3
"""Migrate a hardware-v2 SQLite snapshot to the current server schema.

This utility NEVER modifies the source and NEVER updates environment settings.
Use the destination database only after a separate review and a full backup.
Only data fields are migrated; hardware control commands are not executed.
"""
from __future__ import annotations

import argparse
import json
import os
import sqlite3
import sys
from pathlib import Path


def table_columns(conn: sqlite3.Connection, table: str) -> set[str]:
    if table not in {"readings", "app_control"}:
        raise ValueError("unsupported table")
    return {row[1] for row in conn.execute(f"PRAGMA table_info({table})")}


def migrate(source: Path, destination: Path) -> dict[str, int]:
    source = source.expanduser().resolve(strict=True)
    destination = destination.expanduser().resolve()
    if not source.is_file():
        raise ValueError("source must be a SQLite file")
    if source == destination:
        raise ValueError("source and destination must differ")
    if destination.exists():
        raise FileExistsError(f"destination already exists: {destination}")
    if not destination.parent.is_dir():
        raise FileNotFoundError("destination directory does not exist")
    tmp = destination.with_name(destination.name + ".partial")
    if tmp.exists():
        raise FileExistsError(f"temporary destination exists: {tmp}")

    try:
        # Input is opened in read-only mode, then copied with SQLite backup API.
        with sqlite3.connect(source.as_uri() + "?mode=ro", uri=True) as original:
            with sqlite3.connect(str(tmp)) as db:
                original.backup(db)

        with sqlite3.connect(str(tmp)) as db:
            db.row_factory = sqlite3.Row
            if not table_columns(db, "readings") or not table_columns(db, "app_control"):
                raise ValueError("expected readings and app_control tables")
            before = db.execute("SELECT COUNT(*) FROM readings").fetchone()[0]
            changed = 0
            with db:
                reading_columns = table_columns(db, "readings")
                control_columns = table_columns(db, "app_control")
                if "light" in reading_columns:
                    # Preserve original booleans in extras before removing
                    # hardware-v2's incompatible required light column.
                    last_id = -1
                    while True:
                        rows = db.execute(
                            "SELECT id, light, extras_json FROM readings "
                            "WHERE id > ? ORDER BY id LIMIT 500",
                            (last_id,),
                        ).fetchall()
                        if not rows:
                            break
                        for row in rows:
                            raw = row["extras_json"]
                            try:
                                extras = json.loads(raw) if raw else {}
                            except (TypeError, ValueError) as exc:
                                raise ValueError(
                                    f"invalid extras_json in reading id={row['id']}"
                                ) from exc
                            if extras is None:
                                extras = {}
                            if not isinstance(extras, dict):
                                raise ValueError(
                                    f"extras_json is not an object in reading id={row['id']}"
                                )
                            if "light" not in extras:
                                extras["light"] = bool(row["light"])
                                db.execute(
                                    "UPDATE readings SET extras_json=? WHERE id=?",
                                    (json.dumps(extras), row["id"]),
                                )
                                changed += 1
                        last_id = rows[-1]["id"]
                    db.execute("ALTER TABLE readings DROP COLUMN light")
                if "pump" not in table_columns(db, "readings"):
                    db.execute(
                        "ALTER TABLE readings ADD COLUMN pump INTEGER NOT NULL DEFAULT 0"
                    )
                if "pump" not in control_columns:
                    # Never reinterpret historical light as a pump command.
                    db.execute(
                        "ALTER TABLE app_control ADD COLUMN pump INTEGER NOT NULL DEFAULT 0"
                    )
                after = db.execute("SELECT COUNT(*) FROM readings").fetchone()[0]
                if before != after:
                    raise RuntimeError("migration changed reading count")
                result = db.execute("PRAGMA integrity_check").fetchone()[0]
                if result != "ok":
                    raise RuntimeError(f"integrity check failed: {result}")
        # Atomic rename only after validation; input and any destination remain safe.
        os.replace(tmp, destination)
        return {"readings": before, "light_values_preserved": changed}
    except BaseException:
        if tmp.exists():
            tmp.unlink()
        raise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path, help="existing legacy SQLite DB")
    parser.add_argument("destination", type=Path, help="NEW output SQLite path")
    args = parser.parse_args()
    try:
        result = migrate(args.source, args.destination)
    except (OSError, ValueError, RuntimeError, sqlite3.Error) as exc:
        print(f"Migration aborted; source unchanged: {exc}", file=sys.stderr)
        return 1
    print(json.dumps(result, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
