"""LAN client for the hardware-v2 Sensor-ESP local HTTP API."""

from __future__ import annotations

import logging
from typing import Any

import httpx

from env_config import (
    get_iot_sensor_api_key,
    get_iot_sensor_host,
    get_iot_sensor_port,
)

logger = logging.getLogger(__name__)

_TIMEOUT = 2.5


def is_configured() -> bool:
    return bool(get_iot_sensor_host())


def _base_url() -> str | None:
    host = get_iot_sensor_host()
    if not host:
        return None
    if host.startswith("http://") or host.startswith("https://"):
        return host.rstrip("/")
    return f"http://{host}:{get_iot_sensor_port()}"


def _headers() -> dict[str, str]:
    key = get_iot_sensor_api_key()
    return {"X-Telemetry-Key": key} if key else {}


def get_summary() -> dict[str, Any] | None:
    base = _base_url()
    if not base:
        return None
    try:
        response = httpx.get(
            f"{base}/api/local/summary",
            headers=_headers(),
            timeout=_TIMEOUT,
        )
        response.raise_for_status()
        payload = response.json()
        return payload if isinstance(payload, dict) and payload.get("ok") else None
    except (httpx.HTTPError, ValueError) as exc:
        logger.debug("Sensor-ESP summary unavailable: %s", exc)
        return None


def set_targets(updates: dict[str, bool]) -> bool:
    base = _base_url()
    if not base:
        return False
    allowed = {"light", "heater", "circulation_vent", "exhaust_vent"}
    body = {key: bool(value) for key, value in updates.items() if key in allowed}
    if not body:
        return False
    try:
        response = httpx.post(
            f"{base}/api/local/control/targets",
            json=body,
            headers=_headers(),
            timeout=_TIMEOUT,
        )
        return response.status_code == 200
    except httpx.HTTPError as exc:
        logger.warning("Sensor-ESP target update failed: %s", exc)
        return False


def set_mode(mode: str) -> bool:
    normalized = mode.strip().lower()
    if normalized not in {"auto", "manual"}:
        raise ValueError("mode must be auto or manual")
    base = _base_url()
    if not base:
        return False
    try:
        response = httpx.post(
            f"{base}/api/local/control/mode",
            json={"mode": normalized},
            headers=_headers(),
            timeout=_TIMEOUT,
        )
        return response.status_code == 200
    except httpx.HTTPError as exc:
        logger.warning("Sensor-ESP mode update failed: %s", exc)
        return False
