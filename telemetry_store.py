"""
Persistent telemetry (SQLite): one row per sampling tick — sensors + local relay snapshot.
Optional JSON column for extra ESP32 fields without schema migrations.
"""

from __future__ import annotations

import json
import sqlite3
import uuid
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

import pandas as pd

# Columns mirrored in DB (booleans stored as INTEGER 0/1)
STORE_COLUMNS = [
    "air_temperature",
    "water_temperature",
    "case_temperature",
    "humidity",
    "light_level",
    "ec",
    "ph",
    "circulation_vent",
    "exhaust_vent",
    "heater",
    "pump",
]

_CREATE_SQL = """
CREATE TABLE IF NOT EXISTS readings (
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
    pump INTEGER NOT NULL,
    extras_json TEXT
);
CREATE INDEX IF NOT EXISTS idx_readings_ts ON readings(ts);

CREATE TABLE IF NOT EXISTS app_control (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    control_mode TEXT NOT NULL DEFAULT 'auto',
    heater INTEGER NOT NULL DEFAULT 0,
    circulation_vent INTEGER NOT NULL DEFAULT 0,
    exhaust_vent INTEGER NOT NULL DEFAULT 0,
    pump INTEGER NOT NULL DEFAULT 0,
    updated_at TEXT NOT NULL DEFAULT ''
);
INSERT OR IGNORE INTO app_control (id, control_mode, heater, circulation_vent, exhaust_vent, pump, updated_at)
VALUES (1, 'auto', 0, 0, 0, 0, '');

CREATE TABLE IF NOT EXISTS alerts_log (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    ts TEXT NOT NULL,
    severity TEXT NOT NULL,
    message TEXT NOT NULL,
    source TEXT
);
CREATE INDEX IF NOT EXISTS idx_alerts_ts ON alerts_log(ts);
"""

_RELAY_COLS = ("heater", "circulation_vent", "exhaust_vent", "pump")

_shared_production_store: TelemetryStore | None = None


def _ts_iso(value: Any) -> str:
    if isinstance(value, datetime):
        return value.isoformat(timespec="seconds")
    if hasattr(value, "isoformat") and callable(getattr(value, "isoformat")):
        try:
            return value.isoformat(timespec="seconds")
        except TypeError:
            return value.isoformat()
    return str(value)


def _bool_int(x: Any) -> int:
    return 1 if x else 0


class TelemetryStore:
    def __init__(self, db_path: str | Path, retention_days: int = 90) -> None:
        self.db_path = db_path
        self.retention_days = retention_days
        sp = str(db_path)
        # Plain ":memory:" is a new empty DB per connection — use shared URI for tests.
        if sp == ":memory:":
            self._shared_memory_name = f"telemetry_{uuid.uuid4().hex}"
        else:
            self._shared_memory_name = None
            Path(db_path).parent.mkdir(parents=True, exist_ok=True)
        self._init_db()

    def _connect(self) -> sqlite3.Connection:
        if self._shared_memory_name is not None:
            return sqlite3.connect(
                f"file:{self._shared_memory_name}?mode=memory&cache=shared",
                uri=True,
            )
        return sqlite3.connect(str(self.db_path))

    def _init_db(self) -> None:
        with self._connect() as conn:
            conn.executescript(_CREATE_SQL)
            conn.execute("PRAGMA journal_mode=WAL;")
            conn.commit()

    def ping(self) -> None:
        """Lightweight connectivity check (same DB as ingest)."""
        with self._connect() as conn:
            row = conn.execute("SELECT 1").fetchone()
            if row is None:
                raise RuntimeError("ping returned no row")

    def insert_reading(self, row: dict[str, Any], extras: dict[str, Any] | None = None) -> None:
        ts = _ts_iso(row["timestamp"])
        extras_json = json.dumps(extras, default=str) if extras else None
        values = (
            ts,
            row.get("air_temperature"),
            row.get("water_temperature"),
            row.get("case_temperature"),
            row.get("humidity"),
            row.get("light_level"),
            row.get("ec"),
            row.get("ph"),
            _bool_int(row.get("circulation_vent")),
            _bool_int(row.get("exhaust_vent")),
            _bool_int(row.get("heater")),
            _bool_int(row.get("pump")),
            extras_json,
        )
        with self._connect() as conn:
            conn.execute(
                """INSERT INTO readings (
                    ts, air_temperature, water_temperature, case_temperature,
                    humidity, light_level, ec, ph,
                    circulation_vent, exhaust_vent, heater, pump, extras_json
                ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)""",
                values,
            )
            conn.commit()
        self._prune_old()

    def fetch_latest(self) -> dict[str, Any] | None:
        """Most recent row as JSON-friendly dict (timestamp ISO, booleans Python bool, extras parsed)."""
        with self._connect() as conn:
            conn.row_factory = sqlite3.Row
            cur = conn.execute(
                """SELECT ts, air_temperature, water_temperature, case_temperature,
                          humidity, light_level, ec, ph,
                          circulation_vent, exhaust_vent, heater, pump, extras_json
                   FROM readings ORDER BY id DESC LIMIT 1"""
            )
            row = cur.fetchone()
        if row is None:
            return None
        d = {k: row[k] for k in row.keys()}
        raw_extras = d.pop("extras_json", None)
        if raw_extras:
            try:
                extras = json.loads(raw_extras)
            except json.JSONDecodeError:
                extras = {}
        else:
            extras = {}
        for col in ("circulation_vent", "exhaust_vent", "heater", "pump"):
            d[col] = bool(d[col])
        ts = d.pop("ts")
        d["timestamp"] = ts if isinstance(ts, str) else _ts_iso(ts)
        d["extras"] = extras
        return d

    def query_range(self, start: datetime, end: datetime) -> pd.DataFrame:
        start_s, end_s = _ts_iso(start), _ts_iso(end)
        with self._connect() as conn:
            df = pd.read_sql_query(
                """SELECT ts, air_temperature, water_temperature, case_temperature,
                          humidity, light_level, ec, ph,
                          circulation_vent, exhaust_vent, heater, pump, extras_json
                   FROM readings
                   WHERE ts >= ? AND ts <= ?
                   ORDER BY ts ASC""",
                conn,
                params=(start_s, end_s),
                parse_dates=["ts"],
            )
        if df.empty:
            return df.set_index(pd.DatetimeIndex([], name="timestamp"))
        for col in ("circulation_vent", "exhaust_vent", "heater", "pump"):
            df[col] = df[col].astype(bool)
        df = df.rename(columns={"ts": "timestamp"})
        df = df.set_index("timestamp")
        return df

    def _prune_old(self) -> None:
        if self.retention_days <= 0:
            return
        cutoff = datetime.now() - timedelta(days=self.retention_days)
        cutoff_s = _ts_iso(cutoff)
        with self._connect() as conn:
            conn.execute("DELETE FROM readings WHERE ts < ?", (cutoff_s,))
            conn.commit()

    def bulk_load_dataframe(self, df: pd.DataFrame) -> None:
        """Replace all readings with mock / imported data (index = timestamp)."""
        with self._connect() as conn:
            conn.execute("DELETE FROM readings")
            rows: list[tuple[Any, ...]] = []
            for ts, rec in df.iterrows():
                ts_iso = _ts_iso(pd.Timestamp(ts).to_pydatetime())
                rows.append(
                    (
                        ts_iso,
                        rec.get("air_temperature"),
                        rec.get("water_temperature"),
                        rec.get("case_temperature"),
                        rec.get("humidity"),
                        rec.get("light_level"),
                        rec.get("ec"),
                        rec.get("ph"),
                        _bool_int(rec.get("circulation_vent")),
                        _bool_int(rec.get("exhaust_vent")),
                        _bool_int(rec.get("heater")),
                        _bool_int(rec.get("pump")),
                        None,
                    )
                )
            conn.executemany(
                """INSERT INTO readings (
                    ts, air_temperature, water_temperature, case_temperature,
                    humidity, light_level, ec, ph,
                    circulation_vent, exhaust_vent, heater, pump, extras_json
                ) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)""",
                rows,
            )
            conn.commit()

    def get_control_state(self) -> dict[str, Any]:
        with self._connect() as conn:
            conn.row_factory = sqlite3.Row
            row = conn.execute("SELECT * FROM app_control WHERE id = 1").fetchone()
        if row is None:
            return {
                "control_mode": "auto",
                "targets": {k: False for k in _RELAY_COLS},
                "water_pump": False,
                "updated_at": _ts_iso(datetime.now()),
            }
        d = {k: row[k] for k in row.keys()}
        mode = (d.get("control_mode") or "auto").lower()
        if mode not in ("auto", "manual"):
            mode = "auto"
        targets = {k: bool(d[k]) for k in _RELAY_COLS}
        targets_out = {**targets, "water_pump": targets["pump"]}
        return {
            "control_mode": mode,
            "targets": targets_out,
            "updated_at": d.get("updated_at") or _ts_iso(datetime.now()),
        }

    def set_control_mode(self, mode: str) -> None:
        m = mode.lower()
        if m not in ("auto", "manual"):
            raise ValueError("control_mode must be auto or manual")
        now = _ts_iso(datetime.now())
        with self._connect() as conn:
            conn.execute(
                "UPDATE app_control SET control_mode = ?, updated_at = ? WHERE id = 1",
                (m, now),
            )
            conn.commit()

    def set_control_targets_partial(self, updates: dict[str, Any], *, switch_to_manual: bool) -> None:
        mapped: dict[str, bool] = {}
        for k, v in updates.items():
            key = "pump" if k == "water_pump" else k
            if key not in _RELAY_COLS:
                continue
            mapped[key] = bool(v)
        if not mapped:
            return
        now = _ts_iso(datetime.now())
        with self._connect() as conn:
            row = conn.execute(
                "SELECT heater, circulation_vent, exhaust_vent, pump, control_mode FROM app_control WHERE id=1"
            ).fetchone()
            if row is None:
                return
            h, c, e, p, cur_mode = int(row[0]), int(row[1]), int(row[2]), int(row[3]), row[4]
            vals = {"heater": h, "circulation_vent": c, "exhaust_vent": e, "pump": p}
            vals.update({k: int(mapped[k]) for k in mapped})
            mode = "manual" if switch_to_manual else (cur_mode or "auto")
            conn.execute(
                """UPDATE app_control SET
                    control_mode = ?, heater = ?, circulation_vent = ?, exhaust_vent = ?, pump = ?, updated_at = ?
                    WHERE id = 1""",
                (mode, vals["heater"], vals["circulation_vent"], vals["exhaust_vent"], vals["pump"], now),
            )
            conn.commit()

    def sync_relay_targets_from_automation(self, snapshot: dict[str, bool]) -> None:
        """Write relay outputs into app_control only while mode is auto (dashboard automation)."""
        now = _ts_iso(datetime.now())
        h = _bool_int(snapshot.get("heater"))
        c = _bool_int(snapshot.get("circulation_vent"))
        e = _bool_int(snapshot.get("exhaust_vent"))
        p = _bool_int(snapshot.get("pump"))
        with self._connect() as conn:
            conn.execute(
                """UPDATE app_control
                   SET heater = ?, circulation_vent = ?, exhaust_vent = ?, pump = ?, updated_at = ?
                   WHERE id = 1 AND control_mode = 'auto'""",
                (h, c, e, p, now),
            )
            conn.commit()

    def append_alert(self, severity: str, message: str, source: str | None = None) -> None:
        sev = severity.lower()
        if sev not in ("info", "warn", "error"):
            sev = "warn"
        now = _ts_iso(datetime.now())
        with self._connect() as conn:
            conn.execute(
                "INSERT INTO alerts_log (ts, severity, message, source) VALUES (?, ?, ?, ?)",
                (now, sev, message, source),
            )
            conn.commit()

    def fetch_alerts_recent(self, limit: int = 50) -> list[dict[str, Any]]:
        lim = max(1, min(200, limit))
        with self._connect() as conn:
            conn.row_factory = sqlite3.Row
            cur = conn.execute(
                "SELECT ts, severity, message, source FROM alerts_log ORDER BY id DESC LIMIT ?",
                (lim,),
            )
            rows = cur.fetchall()
        return [
            {
                "ts": r["ts"],
                "severity": r["severity"],
                "message": r["message"],
                "source": r["source"],
            }
            for r in rows
        ]


def get_shared_telemetry_store() -> TelemetryStore:
    """One DB handle for Streamlit + telemetry_api in the same process (default deploy)."""
    global _shared_production_store
    if _shared_production_store is None:
        from env_config import get_grow_data_db_path, get_grow_data_retention_days

        _shared_production_store = TelemetryStore(
            get_grow_data_db_path(),
            get_grow_data_retention_days(),
        )
    return _shared_production_store
