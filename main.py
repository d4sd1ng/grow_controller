import streamlit as st
import asyncio
from datetime import datetime, timedelta
import time
import plotly.graph_objects as go
from iot_sensor import IoTSensor
from data_manager import DataManager
from vpd_calculator import calculate_vpd, is_vpd_in_range, get_vpd_range, calculate_target_humidity
from telegram_bot import send_alert
from esp32_controller import ESP32Controller, simulate_wlan_communication
from gpio_simulator import GPIO, OutputDevice
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

st.set_page_config(page_title="Growroom Control System", page_icon="🌱", layout="wide")

st.title("🌱 Growroom Control System")
st.markdown("Monitor and manage key environmental parameters for optimal plant growth.")

# Initialize GPIO devices
heater = OutputDevice(17)
circulation_vent = OutputDevice(18)
exhaust_vent = OutputDevice(22)
water_pump = OutputDevice(23)

# Initialize sensors, data manager, and ESP32 controller
iot_sensor = IoTSensor()
data_manager = DataManager()
esp32_controller = ESP32Controller()

# Function to control grow room equipment
def control_grow_room_equipment(current_data, target_air_temp, target_humidity, target_ec):
    # Control heater
    if current_data['air_temperature'] < target_air_temp - 1:
        GPIO.output(heater.pin, GPIO.HIGH)
        logger.info("Heater turned ON")
    elif current_data['air_temperature'] > target_air_temp + 1:
        GPIO.output(heater.pin, GPIO.LOW)
        logger.info("Heater turned OFF")

    # Control circulation vent
    if current_data['humidity'] > target_humidity + 5:
        GPIO.output(circulation_vent.pin, GPIO.HIGH)
        logger.info("Circulation Vent turned ON")
    elif current_data['humidity'] < target_humidity - 5:
        GPIO.output(circulation_vent.pin, GPIO.LOW)
        logger.info("Circulation Vent turned OFF")

    # Control exhaust vent
    if current_data['air_temperature'] > target_air_temp + 2:
        GPIO.output(exhaust_vent.pin, GPIO.HIGH)
        logger.info("Exhaust Vent turned ON")
    elif current_data['air_temperature'] < target_air_temp - 2:
        GPIO.output(exhaust_vent.pin, GPIO.LOW)
        logger.info("Exhaust Vent turned OFF")

    # Control water pump
    if current_data['ec'] < target_ec - 0.2:
        GPIO.output(water_pump.pin, GPIO.HIGH)
        logger.info("Water Pump turned ON")
    elif current_data['ec'] > target_ec + 0.2:
        GPIO.output(water_pump.pin, GPIO.LOW)
        logger.info("Water Pump turned OFF")

# The rest of the code remains unchanged
# ...

# Main app logic
if __name__ == "__main__":
    update_real_time_data()
    display_historical_data()
    
    # Footer
    st.markdown("---")
    st.markdown("© 2024 Growroom Control System. All rights reserved.")
