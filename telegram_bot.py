import os
import asyncio
from telegram import Update
from telegram.ext import Application, CommandHandler, ContextTypes

# Initialize sensor simulator
from sensor_simulator import SensorSimulator
sensor_simulator = SensorSimulator()

# Replace 'YOUR_BOT_TOKEN' with the actual token you receive from BotFather
TOKEN = os.environ.get('TELEGRAM_BOT_TOKEN', 'YOUR_BOT_TOKEN')

async def start(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    await update.message.reply_text('Welcome to the Growroom Control System Bot! Use /status to get current readings.')

async def status(update: Update, context: ContextTypes.DEFAULT_TYPE) -> None:
    current_data = sensor_simulator.get_sensor_data()
    status_message = f"""
Current Growroom Status:
🌡️ Air Temperature: {current_data['air_temperature']}°C
💧 Water Temperature: {current_data['water_temperature']}°C
💨 Humidity: {current_data['humidity']}%
⚡ EC: {current_data['ec']} mS/cm
🧪 pH: {current_data['ph']}
"""
    await update.message.reply_text(status_message)

async def send_alert(message: str) -> None:
    # Replace 'YOUR_CHAT_ID' with the actual chat ID where you want to send alerts
    chat_id = os.environ.get('TELEGRAM_CHAT_ID', 'YOUR_CHAT_ID')
    async with Application.builder().token(TOKEN).build() as app:
        await app.bot.send_message(chat_id=chat_id, text=message)

async def run_bot():
    application = Application.builder().token(TOKEN).build()
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("status", status))
    await application.initialize()
    await application.start()
    await application.run_polling()

if __name__ == '__main__':
    asyncio.run(run_bot())
