"""CLI entry: loads .env and starts uvicorn (same SQLite DB as Streamlit by default)."""

from __future__ import annotations

import uvicorn

from env_config import (
    get_telemetry_api_host,
    get_telemetry_api_port,
    get_telemetry_api_reload_enabled,
    load_dotenv_if_present,
)

load_dotenv_if_present()


def main() -> None:
    host = get_telemetry_api_host()
    port = get_telemetry_api_port()
    uvicorn.run("telemetry_api:app", host=host, port=port, reload=get_telemetry_api_reload_enabled())


if __name__ == "__main__":
    main()
