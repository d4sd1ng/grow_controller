"""
HTTP API: telemetry ingest, latest readings, relay targets (portable CYD), alerts.
Shared SQLite with Streamlit (`get_shared_telemetry_store`).

Run: python run_telemetry_api.py
"""

from __future__ import annotations

import logging
import time
from collections import defaultdict
from datetime import datetime, timedelta
from typing import Any

from fastapi import Body, FastAPI, Header, HTTPException, Query, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import JSONResponse

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

# Simple in-memory rate limiter: {ip: [(timestamp, count)]}
_rate_limit_store: dict[str, list[float]] = defaultdict(list)
_RATE_LIMIT_REQUESTS = 60  # max requests per window
_RATE_LIMIT_WINDOW = 60  # seconds


@app.middleware("http")
async def rate_limit_middleware(request: Request, call_next):
    """Block clients exceeding 60 requests/minute per IP."""
    client_ip = request.client.host if request.client else "unknown"
    now = time.time()
    window_start = now - _RATE_LIMIT_WINDOW

    # Clean old entries
    _rate_limit_store[client_ip] = [
        ts for ts in _rate_limit_store[client_ip] if ts > window_start
    ]

    if len(_rate_limit_store[client_ip]) >= _RATE_LIMIT_REQUESTS:
        return JSONResponse(
            status_code=429,
            content={"detail": "Rate limit exceeded. Try again later."},
        )

    _rate_limit_store[client_ip].append(now)
    response = await call_next(request)
    return response

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


@app.get("/api/v1/export/csv")
def export_csv(
    hours: float = Query(default=24, ge=1, le=2160),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> Any:
    """Export telemetry data as CSV download."""
    from fastapi.responses import StreamingResponse
    import io
    _require_api_key(x_telemetry_key)
    dm = get_dm()
    end = datetime.now()
    start = end - timedelta(hours=hours)
    csv_buffer = io.StringIO()
    try:
        dm.export_to_csv(start, end, csv_buffer)
    except ValueError as e:
        raise HTTPException(status_code=404, detail=str(e)) from e
    return StreamingResponse(
        iter([csv_buffer.getvalue()]),
        media_type="text/csv",
        headers={"Content-Disposition": f"attachment; filename=grow_data_{int(time.time())}.csv"},
    )


@app.get("/api/v1/export/json")
def export_json(
    hours: float = Query(default=24, ge=1, le=2160),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> Any:
    """Export telemetry data as JSON download."""
    from fastapi.responses import StreamingResponse
    import io
    _require_api_key(x_telemetry_key)
    dm = get_dm()
    end = datetime.now()
    start = end - timedelta(hours=hours)
    json_buffer = io.StringIO()
    try:
        dm.export_to_json(start, end, json_buffer)
    except ValueError as e:
        raise HTTPException(status_code=404, detail=str(e)) from e
    return StreamingResponse(
        iter([json_buffer.getvalue()]),
        media_type="application/json",
        headers={"Content-Disposition": f"attachment; filename=grow_data_{int(time.time())}.json"},
    )


@app.get("/api/v1/schedules")
def get_schedules(
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """Get all equipment schedules."""
    from schedule_manager import EquipmentSchedule
    _require_api_key(x_telemetry_key)
    scheduler = EquipmentSchedule()
    return {"ok": True, "schedules": scheduler.get_all_schedules()}


@app.post("/api/v1/schedules/{equipment}/toggle")
def toggle_schedule(
    equipment: str,
    body: dict[str, Any] = Body(...),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """Enable or disable a schedule."""
    from schedule_manager import EquipmentSchedule
    _require_api_key(x_telemetry_key)
    scheduler = EquipmentSchedule()
    schedule_name = body.get("name")
    if not schedule_name:
        raise HTTPException(status_code=400, detail="schedule name required")
    
    action = body.get("action", "toggle")
    if action == "enable":
        scheduler.enable_schedule(equipment, schedule_name)
    elif action == "disable":
        scheduler.disable_schedule(equipment, schedule_name)
    else:
        raise HTTPException(status_code=400, detail="action must be 'enable' or 'disable'")
    
    return {"ok": True, "message": f"Schedule {schedule_name} {action}d"}


@app.get("/api/v1/ml/status")
def get_ml_status(
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """Get ML model training status."""
    from ml_controller import MLController
    _require_api_key(x_telemetry_key)
    ml = MLController()
    return {
        "ok": True,
        "trained": ml.is_trained,
        "model_path": str(ml.model_path),
    }


@app.post("/api/v1/ml/train")
def train_ml_model(
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """Train ML model on historical data."""
    from ml_controller import MLController, prepare_data
    _require_api_key(x_telemetry_key)
    dm = get_dm()
    ml = MLController()
    
    end = datetime.now()
    start = end - timedelta(days=7)
    hist = dm.get_historical_data(start, end)
    
    if hist.empty or len(hist) < 10:
        raise HTTPException(status_code=400, detail=f"Not enough data: {len(hist)} rows")
    
    X, y = prepare_data(hist)
    score = ml.train(X, y)
    ml.save_model()
    
    return {"ok": True, "r_squared": score, "samples": len(hist)}


@app.post("/api/v1/ml/predict")
def ml_predict(
    body: dict[str, Any] = Body(...),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """Get ML VPD optimization suggestions."""
    from ml_controller import MLController
    _require_api_key(x_telemetry_key)
    ml = MLController()
    
    if not ml.is_trained:
        raise HTTPException(status_code=400, detail="Model not trained")
    
    growth_stage = body.get("growth_stage", "early_grow")
    vpd_recommendation = ml.predict_optimal_vpd_conditions(body, growth_stage)
    anomalies = ml.detect_anomalies(body)
    
    return {
        "ok": True,
        "vpd_recommendation": vpd_recommendation,
        "anomalies": anomalies,
    }


@app.get("/api/v1/devices")
def list_devices(
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """List all registered ESP32 devices."""
    from multi_device_manager import get_multi_device_manager
    _require_api_key(x_telemetry_key)
    mgr = get_multi_device_manager()
    return {
        "ok": True,
        "count": mgr.get_device_count(),
        "devices": mgr.get_device_list(),
    }


@app.get("/api/v1/devices/i2c")
def list_i2c_devices(
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """List I2C devices detected by ESP32 from latest telemetry reading."""
    _require_api_key(x_telemetry_key)
    latest = get_dm().get_latest_reading()
    if not latest:
        return {"ok": True, "i2c_devices": [], "message": "no readings yet"}
    
    extras = latest.get("extras", {})
    device_info = extras.get("device_info", {})
    i2c_devices = device_info.get("i2c_devices", [])
    
    return {
        "ok": True,
        "device_id": device_info.get("device_id", "unknown"),
        "firmware_version": device_info.get("firmware_version", "unknown"),
        "i2c_device_count": device_info.get("i2c_device_count", 0),
        "has_oled": device_info.get("has_oled", False),
        "has_bh1750": device_info.get("has_bh1750", False),
        "i2c_devices": i2c_devices,
    }


@app.post("/api/v1/devices/register")
def register_device(
    body: dict[str, Any] = Body(...),
    x_telemetry_key: str | None = Header(default=None, alias="X-Telemetry-Key"),
) -> dict[str, Any]:
    """Register a new ESP32 device."""
    from multi_device_manager import get_multi_device_manager
    _require_api_key(x_telemetry_key)
    mgr = get_multi_device_manager()
    
    device_id = body.get("device_id")
    if not device_id:
        raise HTTPException(status_code=400, detail="device_id required")
    
    mgr.register_device(
        device_id=device_id,
        description=body.get("description", ""),
        location=body.get("location", "")
    )
    
    return {"ok": True, "device_id": device_id}
