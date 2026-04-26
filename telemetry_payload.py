"""
Normalize JSON bodies from the grow-room ESP before they hit DataManager.
Supports flat keys, nested `relays` / `relay_states`, water_pump → pump alias,
and device_info with I2C scan results.
"""

from __future__ import annotations

from typing import Any

_RELAY_KEYS = frozenset({"heater", "pump", "circulation_vent", "exhaust_vent"})

# Valid ranges for sensor values (prevent unrealistic/erroneous data)
_VALUE_RANGES = {
    "air_temperature": (-5.0, 60.0),
    "water_temperature": (-5.0, 50.0),
    "case_temperature": (-10.0, 80.0),
    "humidity": (0.0, 100.0),
    "light_level": (0.0, 200000.0),
    "ec": (0.0, 10.0),
    "ph": (0.0, 14.0),
}


def _validate_range(key: str, value: float | None) -> float | None:
    """Clamp numeric values to expected ranges. Return None if invalid."""
    if value is None:
        return None
    if key not in _VALUE_RANGES:
        return value
    try:
        v = float(value)
    except (TypeError, ValueError):
        return None
    low, high = _VALUE_RANGES[key]
    if v < low or v > high:
        return None  # Reject out-of-range values
    return v


def normalize_ingest_payload(raw: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(raw, dict):
        raise TypeError("payload must be a JSON object")
    out: dict[str, Any] = dict(raw)

    # Validate numeric sensor fields
    for key in _VALUE_RANGES.keys():
        if key in out:
            out[key] = _validate_range(key, out[key])

    nested = raw.get("relays")
    if nested is None:
        nested = raw.get("relay_states")
    if isinstance(nested, dict):
        for k in _RELAY_KEYS:
            if k in nested:
                out[k] = bool(nested[k])

    if "water_pump" in raw and "pump" not in raw:
        out["pump"] = bool(raw["water_pump"])

    # Preserve device_info (I2C scan results, firmware version, etc.)
    device_info = raw.get("device_info")
    if isinstance(device_info, dict):
        out["device_info"] = device_info

    return out
