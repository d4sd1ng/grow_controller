"""Synchronous Telegram outbound messages (Streamlit-safe, no asyncio.run per widget)."""

from __future__ import annotations

import json
import logging
import urllib.error
import urllib.request
from typing import Any

from env_config import get_telegram_bot_token, get_telegram_chat_id, load_dotenv_if_present

load_dotenv_if_present()

logger = logging.getLogger(__name__)


def send_telegram_message(text: str, timeout_sec: float = 15.0) -> dict[str, Any]:
    """
    POST to Telegram Bot API. Returns JSON-compatible dict:
    ok, error (optional), http_status (optional).
    """
    token = get_telegram_bot_token()
    chat_id = get_telegram_chat_id()
    if not token or not chat_id:
        logger.debug("Telegram not configured (TELEGRAM_BOT_TOKEN / TELEGRAM_CHAT_ID); skip send")
        return {"ok": False, "error": "telegram_not_configured"}

    url = f"https://api.telegram.org/bot{token}/sendMessage"
    payload = json.dumps({"chat_id": chat_id, "text": text}).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout_sec) as resp:
            body = resp.read().decode("utf-8")
            data = json.loads(body)
            return {"ok": bool(data.get("ok")), "result": data.get("result"), "raw": data}
    except urllib.error.HTTPError as e:
        err_body = e.read().decode("utf-8", errors="replace")
        logger.warning("Telegram HTTPError %s: %s", e.code, err_body[:500])
        return {"ok": False, "error": "http_error", "http_status": e.code}
    except OSError as e:
        logger.warning("Telegram request failed: %s", e)
        return {"ok": False, "error": "network_error", "detail": str(e)}
