import os
import asyncio
from telegram import Update
from telegram.ext import Application, CommandHandler, ContextTypes

# Initialize sensor simulator
from sensor_simulator import SensorSimulator
sensor_simulator = SensorSimulator()

# Use the provided bot token
TOKEN = "7528424546:AAEraIyxdPlliexKVzXS1bU-nH_uAfBNAa8"

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
    chat_id = "832645763"
    async with Application.builder().token(TOKEN).build() as app:
        await app.bot.send_message(chat_id=chat_id, text=message)

def create_application():
    application = Application.builder().token(TOKEN).build()
    application.add_handler(CommandHandler("start", start))
    application.add_handler(CommandHandler("status", status))
    return application

def run_bot():
    application = create_application()
    application.run_polling()

if __name__ == '__main__':
    run_bot()
