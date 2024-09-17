import streamlit as st
import pandas as pd
import plotly.graph_objects as go
from datetime import datetime, timedelta
from sensor_simulator import SensorSimulator
from data_manager import DataManager
from vpd_calculator import calculate_vpd, get_vpd_range, is_vpd_in_range
from telegram_bot import run_bot, send_alert
import asyncio
import threading
import logging

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Initialize sensor simulator and data manager
sensor_simulator = SensorSimulator()
data_manager = DataManager()

# Set page config
st.set_page_config(
    page_title="Growroom Control System",
    page_icon="🌱",
    layout="wide"
)

# Custom CSS
st.markdown("""
    <style>
    .stApp {
        max-width: 1200px;
        margin: 0 auto;
    }
    </style>
    """, unsafe_allow_html=True)

# Title and description
st.title("🌱 Growroom Control System")
st.markdown("Monitor and manage key environmental parameters for optimal plant growth.")

# Sidebar for setting target values and growth stage
st.sidebar.header("Settings")
growth_stage = st.sidebar.radio("Growth Stage", ("Early Grow", "Late Grow/Early Flowering", "Late Flowering"))

# Log the selected growth stage
logger.info(f"Selected growth stage: {growth_stage}")

# Convert growth stage to lowercase with underscores for internal use
growth_stage_key = growth_stage.lower().replace(" ", "_").replace("/", "_")

# Set target values based on growth stage
target_air_temp = st.sidebar.slider("Target Air Temperature (°C)", 26.0, 29.0, 27.5, 0.1)
target_humidity = st.sidebar.slider("Target Humidity (%)", 40, 70, 60, 1)
target_ec = st.sidebar.slider("Target EC (mS/cm)", 0.5, 3.0, 1.5, 0.1)
target_ph = st.sidebar.slider("Target pH", 5.5, 7.5, 6.5, 0.1)

# Log the target values
logger.info(f"Target values - Air Temp: {target_air_temp}, Humidity: {target_humidity}, EC: {target_ec}, pH: {target_ph}")

# Real-time data display
st.header("Real-time Data")
col1, col2, col3 = st.columns(3)
col4, col5, col6 = st.columns(3)

# Get current sensor data
current_data = sensor_simulator.get_sensor_data()

# Calculate VPD
current_vpd = calculate_vpd(current_data['air_temperature'], current_data['humidity'])
vpd_min, vpd_max = get_vpd_range(growth_stage_key)

# Log the VPD range
logger.info(f"VPD range for {growth_stage}: {vpd_min} - {vpd_max} kPa")

# Function to check if a parameter is out of range and send an alert
async def check_and_alert(param_name, current_value, target_value, threshold):
    if abs(current_value - target_value) > threshold:
        alert_message = f"⚠️ Alert: {param_name} is out of range. Current value: {current_value}, Target value: {target_value}"
        await send_alert(alert_message)
        logger.warning(alert_message)

# Air Temperature
with col1:
    st.metric(
        label="Air Temperature",
        value=f"{current_data['air_temperature']:.1f} °C",
        delta=f"{current_data['air_temperature'] - target_air_temp:.1f} °C"
    )
    asyncio.run(check_and_alert("Air Temperature", current_data['air_temperature'], target_air_temp, 1))

# Water Temperature
with col2:
    st.metric(
        label="Water Temperature",
        value=f"{current_data['water_temperature']:.1f} °C"
    )

# Humidity
with col3:
    st.metric(
        label="Humidity",
        value=f"{current_data['humidity']:.0f}%",
        delta=f"{current_data['humidity'] - target_humidity:.0f}%"
    )
    asyncio.run(check_and_alert("Humidity", current_data['humidity'], target_humidity, 5))

# EC
with col4:
    current_ec = current_data['ec']
    st.metric(
        label="EC",
        value=f"{current_ec:.2f} mS/cm",
        delta=f"{current_ec - target_ec:.2f} mS/cm"
    )
    asyncio.run(check_and_alert("EC", current_ec, target_ec, 0.3))

# pH
with col5:
    st.metric(
        label="pH",
        value=f"{current_data['ph']:.2f}",
        delta=f"{current_data['ph'] - target_ph:.2f}"
    )
    asyncio.run(check_and_alert("pH", current_data['ph'], target_ph, 0.5))

# VPD
with col6:
    st.metric(
        label="VPD",
        value=f"{current_vpd:.2f} kPa",
        delta=f"{current_vpd - (vpd_min + vpd_max) / 2:.2f} kPa"
    )
    if not is_vpd_in_range(current_vpd):
        alert_message = f"⚠️ Alert: VPD is out of range. Current value: {current_vpd:.2f} kPa"
        asyncio.run(send_alert(alert_message))
        logger.warning(alert_message)

# Simulated control actions
st.header("Simulated Control Actions")
col1, col2, col3, col4 = st.columns(4)

with col1:
    if current_data['air_temperature'] < target_air_temp - 0.5:
        sensor_simulator.set_equipment_state('heater', True)
        st.info("🔥 Heater: ON")
        logger.info("Heater turned ON")
    else:
        sensor_simulator.set_equipment_state('heater', False)
        st.info("🔥 Heater: OFF")
        logger.info("Heater turned OFF")

with col2:
    if current_data['humidity'] < target_humidity - 5:
        sensor_simulator.set_equipment_state('circulation_vent', True)
        st.info("🌀 Circulation Vent: ON")
        logger.info("Circulation Vent turned ON")
    else:
        sensor_simulator.set_equipment_state('circulation_vent', False)
        st.info("🌀 Circulation Vent: OFF")
        logger.info("Circulation Vent turned OFF")

with col3:
    if current_data['humidity'] > target_humidity + 5:
        sensor_simulator.set_equipment_state('exhaust_vent', True)
        st.info("💨 Exhaust Vent: ON")
        logger.info("Exhaust Vent turned ON")
    else:
        sensor_simulator.set_equipment_state('exhaust_vent', False)
        st.info("💨 Exhaust Vent: OFF")
        logger.info("Exhaust Vent turned OFF")

with col4:
    if current_ec < target_ec - 0.2 or current_data['ph'] < target_ph - 0.5:
        sensor_simulator.set_equipment_state('pump', True)
        st.info("💧 Nutrient Pump: ON")
        logger.info("Nutrient Pump turned ON")
    else:
        sensor_simulator.set_equipment_state('pump', False)
        st.info("💧 Nutrient Pump: OFF")
        logger.info("Nutrient Pump turned OFF")

# Simulated OLED Display
st.header("Simulated OLED Display")
oled_col1, oled_col2 = st.columns(2)

with oled_col1:
    st.text(f"Air Temp: {current_data['air_temperature']:.1f}°C")
    st.text(f"Water Temp: {current_data['water_temperature']:.1f}°C")
    st.text(f"Humidity: {current_data['humidity']}%")

with oled_col2:
    st.text(f"EC: {current_ec:.2f} mS/cm")
    st.text(f"pH: {current_data['ph']:.2f}")
    st.text(f"VPD: {current_vpd:.2f} kPa")

# Historical data charts
st.header("Historical Data")

# Get historical data for the last 24 hours
end_time = datetime.now()
start_time = end_time - timedelta(hours=24)
historical_data = data_manager.get_historical_data(start_time, end_time)

# Create temperature chart
temp_fig = go.Figure()
temp_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['air_temperature'], mode='lines', name='Air Temperature'))
temp_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['water_temperature'], mode='lines', name='Water Temperature'))
temp_fig.update_layout(title='Temperature (Last 24 Hours)', xaxis_title='Time', yaxis_title='Temperature (°C)')

# Create humidity chart
humidity_fig = go.Figure()
humidity_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['humidity'], mode='lines', name='Humidity'))
humidity_fig.update_layout(title='Humidity (Last 24 Hours)', xaxis_title='Time', yaxis_title='Humidity (%)')

# Create EC and pH chart
ec_ph_fig = go.Figure()
ec_ph_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['ec'], mode='lines', name='EC'))
ec_ph_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['ph'], mode='lines', name='pH', yaxis='y2'))
ec_ph_fig.update_layout(
    title='EC and pH (Last 24 Hours)',
    xaxis_title='Time',
    yaxis_title='EC (mS/cm)',
    yaxis2=dict(title='pH', overlaying='y', side='right')
)

# Create VPD chart
vpd_data = [calculate_vpd(temp, hum) for temp, hum in zip(historical_data['air_temperature'], historical_data['humidity'])]
vpd_fig = go.Figure()
vpd_fig.add_trace(go.Scatter(x=historical_data.index, y=vpd_data, mode='lines', name='VPD'))
vpd_fig.add_hline(y=0.4, line_dash="dash", line_color="red", annotation_text="Min VPD: 0.4 kPa")
vpd_fig.add_hline(y=1.6, line_dash="dash", line_color="red", annotation_text="Max VPD: 1.6 kPa")
vpd_fig.add_hrect(y0=0.4, y1=0.8, fillcolor="rgba(0,255,0,0.1)", line_width=0, annotation_text="Early Grow")
vpd_fig.add_hrect(y0=0.8, y1=1.2, fillcolor="rgba(255,255,0,0.1)", line_width=0, annotation_text="Late Grow/Early Flowering")
vpd_fig.add_hrect(y0=1.2, y1=1.6, fillcolor="rgba(255,165,0,0.1)", line_width=0, annotation_text="Late Flowering")
vpd_fig.update_layout(title='VPD (Last 24 Hours)', xaxis_title='Time', yaxis_title='VPD (kPa)')

# Display charts
st.plotly_chart(temp_fig, use_container_width=True)
st.plotly_chart(humidity_fig, use_container_width=True)
st.plotly_chart(ec_ph_fig, use_container_width=True)
st.plotly_chart(vpd_fig, use_container_width=True)

# Update historical data
if st.button("Update Historical Data"):
    data_manager.add_data_point(current_data)
    st.success("Historical data updated successfully!")
    logger.info("Historical data updated")

# Sensor Calibration
st.sidebar.header("Sensor Calibration")
if st.sidebar.button("Calibrate pH Sensor"):
    st.sidebar.info("pH sensor calibration initiated. Follow the instructions on the device.")
    # Add actual calibration logic here

if st.sidebar.button("Calibrate EC Sensor"):
    st.sidebar.info("EC sensor calibration initiated. Follow the instructions on the device.")
    # Add actual calibration logic here

# Footer
st.markdown("---")
st.markdown("© 2024 Growroom Control System. All rights reserved.")

# Log the completion of the dashboard update
logger.info("Growroom Control System dashboard updated successfully")

# Run the Telegram bot in a separate thread
async def run_telegram_bot_async():
    await run_bot()

def run_telegram_bot():
    asyncio.run(run_telegram_bot_async())

bot_thread = threading.Thread(target=run_telegram_bot)
bot_thread.start()
