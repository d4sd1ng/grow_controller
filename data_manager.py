import math
import random
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any

import pandas as pd

from env_config import get_grow_data_db_path, get_grow_data_retention_days
from telemetry_store import STORE_COLUMNS, TelemetryStore, get_shared_telemetry_store
from vpd_calculator import calculate_vpd

_LOG_COLUMNS = [
    "timestamp",
    *STORE_COLUMNS,
]


def _json_safe_extras(data_point: dict[str, Any]) -> dict[str, Any]:
    """Fields not in the fixed log schema (ESP32 extras, nested relay state, …)."""
    core = set(_LOG_COLUMNS)
    out: dict[str, Any] = {}
    for k, v in data_point.items():
        if k in core or k == "timestamp":
            continue
        if isinstance(v, (dict, list, str, int, float, bool)) or v is None:
            out[k] = v
        else:
            out[k] = str(v)
    return out


class DataManager:
    """
    Normalizes incoming readings (local sim + optional ESP payload), persists to SQLite,
    and serves time-range queries for charts. One physical ESP32 can fill the same row:
    map its JSON keys into these fields in your ingest layer later.
    """

    def __init__(
        self,
        db_path: str | Path | None = None,
        retention_days: int | None = None,
    ) -> None:
        if db_path is not None:
            rd = retention_days if retention_days is not None else get_grow_data_retention_days()
            self._store = TelemetryStore(db_path, rd)
        else:
            self._store = get_shared_telemetry_store()

    def _normalize_row(self, data_point: dict[str, Any]) -> dict[str, Any]:
        row: dict[str, Any] = {col: data_point.get(col) for col in _LOG_COLUMNS if col != "timestamp"}
        row["timestamp"] = data_point.get("timestamp", datetime.now())
        for key in ("light", "heater", "circulation_vent", "exhaust_vent"):
            if row.get(key) is None:
                row[key] = False
            else:
                row[key] = bool(row[key])
        return row

    def add_data_point(self, data_point: dict[str, Any]) -> None:
        row = self._normalize_row(data_point)
        extras = _json_safe_extras(data_point)
        self._store.insert_reading(row, extras if extras else None)

    def ping_db(self) -> None:
        self._store.ping()

    def get_historical_data(self, start_time: datetime, end_time: datetime) -> pd.DataFrame:
        return self._store.query_range(start_time, end_time)

    def get_latest_reading(self) -> dict[str, Any] | None:
        return self._store.fetch_latest()

    def get_control_state(self) -> dict[str, Any]:
        return self._store.get_control_state()

    def set_control_mode(self, mode: str) -> None:
        self._store.set_control_mode(mode)

    def set_control_targets_partial(self, updates: dict[str, Any], *, switch_to_manual: bool = True) -> None:
        self._store.set_control_targets_partial(updates, switch_to_manual=switch_to_manual)

    def sync_relay_targets_from_automation(self, snapshot: dict[str, bool]) -> None:
        self._store.sync_relay_targets_from_automation(snapshot)

    def add_alert(self, severity: str, message: str, source: str | None = None) -> None:
        self._store.append_alert(severity, message, source)

    def get_alerts_recent(self, limit: int = 50) -> list[dict[str, Any]]:
        return self._store.fetch_alerts_recent(limit)

    def get_telemetry_series(
        self,
        *,
        start: datetime,
        end: datetime,
        max_points: int = 200,
    ) -> list[dict[str, Any]]:
        """Downsampled time series from SQLite for charts (e.g. CYD / web)."""
        df = self._store.query_range(start, end)
        if df.empty:
            return []
        n = len(df)
        cap = max(10, min(2000, max_points))
        if n > cap:
            step = max(1, math.ceil(n / cap))
            df = df.iloc[::step]
        out: list[dict[str, Any]] = []
        for ts_idx, row in df.iterrows():
            t = row.get("air_temperature")
            rh = row.get("humidity")
            if pd.isna(t) or pd.isna(rh):
                continue
            tf = float(t)
            rhf = float(rh)
            ts_str = ts_idx.isoformat() if hasattr(ts_idx, "isoformat") else str(ts_idx)
            point: dict[str, Any] = {
                "ts": ts_str,
                "air_temperature": tf,
                "humidity": rhf,
                "vpd": calculate_vpd(tf, rhf),
            }
            for key in ("ec", "ph", "water_temperature"):
                v = row.get(key)
                if v is not None and not pd.isna(v):
                    point[key] = float(v)
            out.append(point)
        return out

    def generate_mock_data(self) -> None:
        end_time = datetime.now()
        start_time = end_time - timedelta(days=7)
        date_range = pd.date_range(start=start_time, end=end_time, freq="5min")

        mock_data = pd.DataFrame(
            {
                "timestamp": date_range,
                "air_temperature": [round(random.uniform(20, 30), 1) for _ in range(len(date_range))],
                "water_temperature": [round(random.uniform(18, 28), 1) for _ in range(len(date_range))],
                "case_temperature": [round(random.uniform(22, 32), 1) for _ in range(len(date_range))],
                "humidity": [round(random.uniform(40, 70)) for _ in range(len(date_range))],
                "light_level": [round(random.uniform(0, 2000)) for _ in range(len(date_range))],
                "ec": [round(random.uniform(0.5, 3.0), 2) for _ in range(len(date_range))],
                "ph": [round(random.uniform(5.5, 7.5), 2) for _ in range(len(date_range))],
                "circulation_vent": [random.choice([True, False]) for _ in range(len(date_range))],
                "exhaust_vent": [random.choice([True, False]) for _ in range(len(date_range))],
                "heater": [random.choice([True, False]) for _ in range(len(date_range))],
                "light": [random.choice([True, False]) for _ in range(len(date_range))],
            }
        )
        mock_data.set_index("timestamp", inplace=True)
        self._store.bulk_load_dataframe(mock_data)

    def export_to_csv(self, start_time: datetime, end_time: datetime, file_path: str) -> None:
        """Export telemetry data to CSV file or buffer."""
        df = self.get_historical_data(start_time, end_time)
        if df.empty:
            raise ValueError("No data available for the specified time range")
        import io
        if hasattr(file_path, 'write'):
            file_path.write(df.to_csv(index=True))
        else:
            df.to_csv(file_path, index=True)

    def export_to_json(self, start_time: datetime, end_time: datetime, file_path: str) -> None:
        """Export telemetry data to JSON file."""
        df = self.get_historical_data(start_time, end_time)
        if df.empty:
            raise ValueError("No data available for the specified time range")
        df.reset_index(inplace=True)
        df['timestamp'] = df['timestamp'].apply(lambda x: x.isoformat() if hasattr(x, 'isoformat') else str(x))
        df.to_json(file_path, orient='records', indent=2)