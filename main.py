import streamlit as st
import pandas as pd
import plotly.graph_objects as go
from datetime import datetime, timedelta
from iot_sensor import IoTSensor
from data_manager import DataManager
from vpd_calculator import calculate_vpd, get_vpd_range, is_vpd_in_range, calculate_target_humidity
from telegram_bot import create_application, send_alert, Application
import asyncio
import logging
from gpio_simulator import GPIO, OutputDevice
import nest_asyncio
from esp32_controller import ESP32Controller, simulate_wlan_communication
import time

nest_asyncio.apply()

# Initialize Telegram bot
loop = asyncio.get_event_loop()
application = loop.run_until_complete(create_application())

# Initialize GPIO cleanup
if 'initialized' not in st.session_state:
    st.session_state.initialized = True
    GPIO.cleanup()

# Set up logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

# Initialize IoT sensor, data manager, and ESP32 controller
iot_sensor = IoTSensor()
data_manager = DataManager()
esp32_controller = ESP32Controller()

# Initialize GPIO devices
heater = OutputDevice(17)
circulation_vent = OutputDevice(18)
exhaust_vent = OutputDevice(22)
water_pump = OutputDevice(23)

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

# Grow/Flowering Cycle Tracking
if 'cycle_start_date' not in st.session_state:
    st.session_state.cycle_start_date = datetime.now().date()
    st.session_state.growing_start_date = st.session_state.cycle_start_date
    st.session_state.flowering_start_date = None

def display_cycle_info():
    current_date = datetime.now().date()
    total_days = (current_date - st.session_state.cycle_start_date).days + 1
    
    if st.session_state.flowering_start_date is None:
        growing_days = total_days
        flowering_days = 0
    else:
        growing_days = (st.session_state.flowering_start_date - st.session_state.growing_start_date).days + 1
        flowering_days = (current_date - st.session_state.flowering_start_date).days + 1
    
    st.sidebar.markdown(f"**Current Cycle:**")
    st.sidebar.markdown(f"Total Days: {total_days}")
    st.sidebar.markdown(f"Days Growing: {growing_days}")
    st.sidebar.markdown(f"Days Flowering: {flowering_days}")

def set_cycle_start():
    st.session_state.cycle_start_date = st.session_state.new_start_date
    st.session_state.growing_start_date = st.session_state.cycle_start_date
    st.session_state.flowering_start_date = None

def switch_to_flowering():
    if st.session_state.flowering_start_date is None:
        st.session_state.flowering_start_date = datetime.now().date()
        st.success("Switched to flowering stage!")
    else:
        st.warning("Already in flowering stage!")

# Display cycle information
display_cycle_info()

# Set cycle start date
st.sidebar.date_input("Set Cycle Start Date", st.session_state.cycle_start_date, key="new_start_date")
if st.sidebar.button("Update Cycle Start"):
    set_cycle_start()

# Button to switch to flowering stage
if st.sidebar.button("Switch to Flowering Stage"):
    switch_to_flowering()

# Log the selected growth stage
logger.info(f"Selected growth stage: {growth_stage}")

# Convert growth stage to lowercase with underscores for internal use
growth_stage_key = growth_stage.lower().replace(" ", "_").replace("/", "_")

# Get VPD and humidity ranges for the current growth stage
vpd_min, vpd_max, humidity_min, humidity_max = get_vpd_range(growth_stage_key)

# Set target values based on growth stage
target_air_temp = st.sidebar.slider("Target Air Temperature (°C)", 20.0, 35.0, 27.5, 0.1)
target_humidity = st.sidebar.slider("Target Humidity (%)", humidity_min, humidity_max, calculate_target_humidity(target_air_temp, growth_stage_key), 1)
target_ec = st.sidebar.slider("Target EC (mS/cm)", 0.5, 3.0, 1.5, 0.1)
target_ph = st.sidebar.slider("Target pH", 5.5, 7.5, 6.5, 0.1)

# Log the target values
logger.info(f"Target values - Air Temp: {target_air_temp}, Humidity: {target_humidity}, EC: {target_ec}, pH: {target_ph}")

# Function to check if a parameter is out of range and send an alert
async def check_and_alert(param_name, current_value, target_value, threshold):
    if abs(current_value - target_value) > threshold:
        alert_message = f"⚠️ Alert: {param_name} is out of range. Current value: {current_value}, Target value: {target_value}"
        await send_alert(alert_message)
        logger.warning(alert_message)

# Function to receive data from ESP32 controller
def receive_data_from_esp32(data):
    return data

# Function to control grow room equipment
def control_grow_room_equipment(current_data):
    if current_data['air_temperature'] < target_air_temp - 0.5:
        heater.on()
        logger.info("Heater turned ON")
    else:
        heater.off()
        logger.info("Heater turned OFF")
    
    if current_data['humidity'] < target_humidity - 5:
        circulation_vent.on()
        logger.info("Circulation Vent turned ON")
    else:
        circulation_vent.off()
        logger.info("Circulation Vent turned OFF")
    
    if current_data['humidity'] > target_humidity + 5:
        exhaust_vent.on()
        logger.info("Exhaust Vent turned ON")
    else:
        exhaust_vent.off()
        logger.info("Exhaust Vent turned OFF")
    
    if current_data['ec'] < target_ec - 0.2 or current_data['ph'] < target_ph - 0.5:
        water_pump.on()
        logger.info("Water Pump turned ON")
    else:
        water_pump.off()
        logger.info("Water Pump turned OFF")

# Function to send command to ESP32 controller
def send_command_to_esp32(current_data):
    command = {
        'heater': heater.input(heater.pin),
        'circulation_vent': circulation_vent.input(circulation_vent.pin),
        'exhaust_vent': exhaust_vent.input(exhaust_vent.pin),
        'water_pump': water_pump.input(water_pump.pin)
    }
    
    # Update OLED display data
    command['oled_data'] = {
        "line1": f"Temp: {current_data['esp32_temperature']:.1f}C",
        "line2": f"Humidity: {current_data['esp32_humidity']:.0f}%"
    }
    
    return command

# Main loop
while True:
    # Simulate WLAN communication with ESP32
    esp32_data = simulate_wlan_communication(esp32_controller, receive_data_from_esp32)
    
    # Get current sensor data
    current_data = iot_sensor.read_sensor_data()
    current_data.update(esp32_data)
    
    # Calculate VPD
    current_vpd = calculate_vpd(current_data['air_temperature'], current_data['humidity'])
    
    # Control grow room equipment
    control_grow_room_equipment(current_data)
    
    # Send command to ESP32
    esp32_command = send_command_to_esp32(current_data)
    simulate_wlan_communication(esp32_controller, lambda x: x.receive_command_from_main_controller(esp32_command))
    
    # Real-time data display
    st.header("Real-time Data")
    col1, col2, col3 = st.columns(3)
    col4, col5, col6 = st.columns(3)
    
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
    
    # ESP32 Controller Status
    st.header("ESP32 Controller Status")
    esp32_col1, esp32_col2 = st.columns(2)
    
    with esp32_col1:
        st.metric(
            label="ESP32 Temperature",
            value=f"{current_data['esp32_temperature']:.1f} °C",
            delta=f"{current_data['esp32_temperature'] - target_air_temp:.1f} °C"
        )
    
    with esp32_col2:
        st.metric(
            label="ESP32 Humidity",
            value=f"{current_data['esp32_humidity']:.0f}%",
            delta=f"{current_data['esp32_humidity'] - target_humidity:.0f}%"
        )
    
    # Display ESP32 relay states
    st.subheader("ESP32 Relay States")
    for relay, state in current_data['esp32_relay_states'].items():
        st.write(f"{relay.capitalize()}: {'ON' if state else 'OFF'}")
    
    # Add manual override controls for ESP32 relays
    st.subheader("Manual Override")
    for relay in current_data['esp32_relay_states']:
        if st.button(f"Toggle {relay.capitalize()}"):
            command = {
                relay: not current_data['esp32_relay_states'][relay]
            }
            simulate_wlan_communication(esp32_controller, lambda x: x.receive_command_from_main_controller(command))
    
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
    data_manager.add_data_point(current_data)
    
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
    
    # Sleep for a short interval before the next update
    time.sleep(5)