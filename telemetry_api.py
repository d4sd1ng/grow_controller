"""
HTTP API: telemetry ingest, latest readings, relay targets (portable CYD), alerts.
Shared SQLite with Streamlit (`get_shared_telemetry_store`).

Run: python run_telemetry_api.py
"""

from __future__ import annotations

import logging
from datetime import datetime, timedelta
from typing import Any

from fastapi import Body, FastAPI, Header, HTTPException, Query
from fastapi.middleware.cors import CORSMiddleware

from data_manager import DataManager
from env_config import get_telemetry_api_key, get_telemetry_cors_origins, load_dotenv_if_present
from telemetry_payload import normalize_ingest_payload

load_dotenv_if_present()

logger = logging.getLogger(__name__)

app = FastAPI(title="Grow telemetry & control", version="0.2.0")

_origins = get_telemetry_cors_origins()
app.add_middleware(
    CORSMiddleware,
    allow_origins=_origins or ["*"],
    allow_credentials="*" not in _origins,
    allow_methods=["*"],
    allow_headers=["*"],
)

_dm: DataManager | None = None


def get_dm() -> DataManager:
    global _dm
    if _dm is None:
        _dm = DataManager()
    return _dm


def _configured_api_key() -> str | None:
    return get_telemetry_api_key()


def _require_api_key(x_telemetry_key: str | None) -> None:
    expected = _configured_api_key()
    if expected is None:
        return
    if not x_telemetry_key or x_telemetry_key.strip() != expected:
        raise HTTPException(status_code=401, detail="Invalid or missing X-Telemetry-Key")


@app.get("/api/v1/health")
def health() -> dict[str, Any]:
    """Uptime/load-balancer check; runs `SELECT 1` on the telemetry DB (no API key)."""
    try:
        get_dm().ping_db()
    except Exception as e:
        logger.exception("health check failed: %s", e)
        raise HTTPException(status_code=503, detail="db_unavailable") from e
    return {"ok": True, "status": "healthy", "database": "ok"}


@app.post("/api/v1/telemetry")
def post_telemetry(
    body: dict[str, Any] = Body(...),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, bool]:
    _require_api_key(x_telemetry_key)
    try:
        merged = normalize_ingest_payload(body)
    except TypeError as e:
        raise HTTPException(status_code=400, detail=str(e)) from e
    get_dm().add_data_point(merged)
    return {"ok": True}


@app.get("/api/v1/telemetry/latest")
def get_latest(
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    _require_api_key(x_telemetry_key)
    row = get_dm().get_latest_reading()
    if row is None:
        raise HTTPException(status_code=404, detail="no_readings")
    return {"ok": True, "reading": row}


@app.get("/api/v1/telemetry/series")
def get_telemetry_series(
    minutes: int | None = Query(default=None, ge=1, le=60 * 24 * 90),
    hours: float | None = Query(default=None, ge=0.05, le=24 * 90),
    max_points: int = Query(default=200, ge=10, le=2000),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """Zeitreihe aus SQLite (gleiche DB wie Streamlit). Standard: letzte 3 h."""
    _require_api_key(x_telemetry_key)
    end = datetime.now()
    if hours is not None:
        start = end - timedelta(hours=hours)
    elif minutes is not None:
        start = end - timedelta(minutes=minutes)
    else:
        start = end - timedelta(hours=3)
    points = get_dm().get_telemetry_series(start=start, end=end, max_points=max_points)
    return {
        "ok": True,
        "start": start.isoformat(timespec="seconds"),
        "end": end.isoformat(timespec="seconds"),
        "count": len(points),
        "points": points,
    }


@app.get("/api/v1/control/state")
def get_control_state(
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    _require_api_key(x_telemetry_key)
    st = get_dm().get_control_state()
    return {"ok": True, **st}


@app.post("/api/v1/control/targets")
def post_control_targets(
    body: dict[str, Any] = Body(...),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, bool]:
    _require_api_key(x_telemetry_key)
    get_dm().set_control_targets_partial(body, switch_to_manual=True)
    return {"ok": True}


@app.post("/api/v1/control/mode")
def post_control_mode(
    body: dict[str, Any] = Body(...),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, bool]:
    _require_api_key(x_telemetry_key)
    mode = body.get("mode")
    if not isinstance(mode, str):
        raise HTTPException(status_code=400, detail="mode required")
    try:
        get_dm().set_control_mode(mode)
    except ValueError as e:
        raise HTTPException(status_code=400, detail=str(e)) from e
    return {"ok": True}


@app.get("/api/v1/alerts/recent")
def get_alerts_recent(
    limit: int = Query(default=40, ge=1, le=200),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    _require_api_key(x_telemetry_key)
    return {"ok": True, "alerts": get_dm().get_alerts_recent(limit)}


@app.post("/api/v1/alerts")
def post_alert(
    body: dict[str, Any] = Body(...),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, bool]:
    _require_api_key(x_telemetry_key)
    msg = body.get("message")
    if not msg or not isinstance(msg, str):
        raise HTTPException(status_code=400, detail="message required")
    sev = body.get("severity", "warn")
    if not isinstance(sev, str):
        sev = "warn"
    src = body.get("source")
    src_s = str(src) if src is not None else None
    get_dm().add_alert(sev, msg, src_s)
    return {"ok": True}


@app.get("/api/v1/mobile/summary")
def get_mobile_summary(
    alert_limit: int = Query(default=20, ge=1, le=100),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """One round-trip for portable CYD: control + last reading + recent alerts."""
    _require_api_key(x_telemetry_key)
    dm = get_dm()
    ctrl = dm.get_control_state()
    return {
        "ok": True,
        "control": ctrl,
        "reading": dm.get_latest_reading(),
        "alerts": dm.get_alerts_recent(alert_limit),
    }
