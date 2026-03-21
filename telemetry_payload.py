"""
Normalize JSON bodies from the grow-room ESP before they hit DataManager.
Supports flat keys, nested `relays` / `relay_states`, and water_pump → pump alias.
"""

from __future__ import annotations

from typing import Any

_RELAY_KEYS = frozenset({"heater", "pump", "circulation_vent", "exhaust_vent"})


def normalize_ingest_payload(raw: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(raw, dict):
        raise TypeError("payload must be a JSON object")
    out: dict[str, Any] = dict(raw)

    nested = raw.get("relays")
    if nested is None:
        nested = raw.get("relay_states")
    if isinstance(nested, dict):
        for k in _RELAY_KEYS:
            if k in nested:
                out[k] = bool(nested[k])

    if "water_pump" in raw and "pump" not in raw:
        out["pump"] = bool(raw["water_pump"])

    return out
