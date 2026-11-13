"""
Load optional `.env` (project root) and expose typed getters.

**Regel:** Sensible Werte (Tokens, API-Keys, WLAN-Passwort für Geräte-Export, …) nur in `.env`
— nicht im Quelltext committen. Firmware nutzt optional `device_secrets.h`, erzeugt durch
`export_firmware_secrets_from_env.py` aus denselben Variablen.
"""

from __future__ import annotations

import os
from pathlib import Path


def load_dotenv_if_present() -> None:
    env_path = Path(__file__).resolve().parent / ".env"
    if not env_path.is_file():
        return
    for raw in env_path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, _, value = line.partition("=")
        key, value = key.strip(), value.strip().strip("'").strip('"')
        if key:
            os.environ.setdefault(key, value)


def get_telegram_bot_token() -> str | None:
    v = os.environ.get("TELEGRAM_BOT_TOKEN")
    return v.strip() if v else None


def get_telegram_chat_id() -> str | None:
    v = os.environ.get("TELEGRAM_CHAT_ID")
    return v.strip() if v else None


def get_grow_data_db_path() -> str | Path:
    """
    SQLite path for telemetry. Default: data/grow_telemetry.db under project root.
    Set GROW_DATA_DB=:memory: for tests or ephemeral runs.
    """
    raw = os.environ.get("GROW_DATA_DB", "data/grow_telemetry.db").strip()
    if raw == ":memory:":
        return ":memory:"
    p = Path(raw)
    if not p.is_absolute():
        p = Path(__file__).resolve().parent / p
    return p


def get_grow_database_backend() -> str:
    """Select 'postgres' for the Raspberry Pi; 'sqlite' remains for local tests."""
    backend = os.environ.get("GROW_DB_BACKEND", "sqlite").strip().lower()
    if backend not in ("postgres", "sqlite"):
        raise ValueError("GROW_DB_BACKEND must be 'postgres' or 'sqlite'")
    return backend


def get_grow_postgres_dsn() -> str:
    """Connection URI is kept exclusively in the untracked local .env."""
    return os.environ.get("GROW_POSTGRES_DSN", "").strip()


def get_grow_data_retention_days() -> int:
    v = os.environ.get("GROW_DATA_RETENTION_DAYS", "90").strip()
    try:
        return max(0, int(v))
    except ValueError:
        return 90


def get_telemetry_api_host() -> str:
    """Bind-Adresse für Uvicorn (`TELEMETRY_API_HOST`, Standard `0.0.0.0`)."""
    v = os.environ.get("TELEMETRY_API_HOST", "0.0.0.0").strip()
    return v or "0.0.0.0"


def get_telemetry_api_port() -> int:
    v = os.environ.get("TELEMETRY_API_PORT", "8765").strip()
    try:
        return int(v)
    except ValueError:
        return 8765


def get_telemetry_api_key() -> str | None:
    """Gemeinsamer Key für Header `X-Telemetry-Key`; `None` wenn nicht gesetzt."""
    k = os.environ.get("TELEMETRY_API_KEY", "").strip()
    return k or None


def get_telemetry_cors_origins() -> list[str]:
    """Kommagetrennte Origins oder `*` (Standard)."""
    raw = os.environ.get("TELEMETRY_CORS_ORIGINS", "*").strip()
    parts = [o.strip() for o in raw.split(",") if o.strip()]
    return parts if parts else ["*"]


def get_telemetry_api_reload_enabled() -> bool:
    return os.environ.get("TELEMETRY_API_RELOAD", "").strip() == "1"
