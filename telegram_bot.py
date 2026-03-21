import asyncio
import logging

from telegram import Update
from telegram.ext import Application, CommandHandler, ContextTypes

from env_config import get_telegram_bot_token, load_dotenv_if_present
from iot_sensor import IoTSensor

load_dotenv_if_present()

logger = logging.getLogger(__name__)

_sensor = IoTSensor()


def _require_token() -> str:
    token = get_telegram_bot_token()
    if not token:
        raise RuntimeError(
            "TELEGRAM_BOT_TOKEN is not set. Copy .env.example to .env and add your bot token."
        )
    return token


async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text(
        "Welcome to the Growroom Control System Bot! Use /status to get current readings."
    )


async def status(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    current_data = _sensor.get_sensor_data()
    status_message = f"""
Current Growroom Status:
🌡️ Air Temperature: {current_data['air_temperature']}°C
💧 Water Temperature: {current_data['water_temperature']}°C
💨 Humidity: {current_data['humidity']}%
⚡ EC: {current_data['ec']} mS/cm
🧪 pH: {current_data['ph']}
"""
    await update.message.reply_text(status_message)


async def create_application() -> Application:
    application = Application.builder().token(_require_token()).build()
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("status", status))
    return application


async def run_bot() -> None:
    application = await create_application()
    await application.run_polling()


if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO)
    asyncio.run(run_bot())
