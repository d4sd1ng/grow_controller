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
        heater.on()
        logger.info("Heater turned ON")
    elif current_data['air_temperature'] > target_air_temp + 1:
        heater.off()
        logger.info("Heater turned OFF")

    # Control circulation vent
    if current_data['humidity'] > target_humidity + 5:
        circulation_vent.on()
        logger.info("Circulation Vent turned ON")
    elif current_data['humidity'] < target_humidity - 5:
        circulation_vent.off()
        logger.info("Circulation Vent turned OFF")

    # Control exhaust vent
    if current_data['air_temperature'] > target_air_temp + 2:
        exhaust_vent.on()
        logger.info("Exhaust Vent turned ON")
    elif current_data['air_temperature'] < target_air_temp - 2:
        exhaust_vent.off()
        logger.info("Exhaust Vent turned OFF")

    # Control water pump
    if current_data['ec'] < target_ec - 0.2:
        water_pump.on()
        logger.info("Water Pump turned ON")
    elif current_data['ec'] > target_ec + 0.2:
        water_pump.off()
        logger.info("Water Pump turned OFF")

# Function to send command to ESP32 controller
def send_command_to_esp32(current_data):
    command = {
        'heater': GPIO.input(heater.pin),
        'circulation_vent': GPIO.input(circulation_vent.pin),
        'exhaust_vent': GPIO.input(exhaust_vent.pin),
        'water_pump': GPIO.input(water_pump.pin)
    }
    
    # Update OLED display data
    command['oled_data'] = {
        "line1": f"Temp: {current_data.get('esp32_temperature', current_data['air_temperature']):.1f}C",
        "line2": f"Humidity: {current_data.get('esp32_humidity', current_data['humidity']):.0f}%"
    }
    
    return command

# Function to check if a parameter is out of range and send an alert
async def check_and_alert(param_name, current_value, target_value, threshold):
    if abs(current_value - target_value) > threshold:
        alert_message = f"⚠️ Alert: {param_name} is out of range. Current value: {current_value}, Target value: {target_value}"
        await send_alert(alert_message)
        logger.warning(alert_message)

# Custom function to display metrics
def custom_metric(label, value, delta=None):
    delta_html = f"<span class='metric-delta'>{delta}</span>" if delta else ""
    return st.markdown(f"""
        <div class="metric">
            <span class="metric-label">{label}</span>
            <span class="metric-value">{value}</span>
            {delta_html}
        </div>
    """, unsafe_allow_html=True)

# Streamlit app
st.title("🌱 Growroom Control System")
st.markdown("Monitor and manage key environmental parameters for optimal plant growth.")

# Sidebar for setting target values and growth stage
st.sidebar.header("Settings")
growth_stage = st.sidebar.radio("Growth Stage", ("Early Grow", "Late Grow/Early Flowering", "Late Flowering"))

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

# Function to update and display real-time data
def update_real_time_data():
    # Get current sensor data
    current_data = iot_sensor.read_sensor_data()
    esp32_data = simulate_wlan_communication(esp32_controller, 'send')
    
    # Merge ESP32 data with current_data
    if esp32_data and 'sensor_data' in esp32_data:
        current_data.update(esp32_data['sensor_data'])
    if esp32_data and 'esp32_relay_states' in esp32_data:
        current_data['esp32_relay_states'] = esp32_data['esp32_relay_states']
    
    # Calculate VPD
    current_vpd = calculate_vpd(current_data['air_temperature'], current_data['humidity'])
    
    # Control grow room equipment
    control_grow_room_equipment(current_data, target_air_temp, target_humidity, target_ec)
    
    # Send command to ESP32
    esp32_command = send_command_to_esp32(current_data)
    simulate_wlan_communication(esp32_controller, 'receive', esp32_command)
    
    # Real-time data display
    st.header("Real-time Data")
    col1, col2, col3 = st.columns(3)
    col4, col5, col6 = st.columns(3)
    
    with col1:
        custom_metric(
            label="Air Temperature",
            value=f"{current_data['air_temperature']:.1f} °C",
            delta=f"{current_data['air_temperature'] - target_air_temp:.1f} °C"
        )
        asyncio.run(check_and_alert("Air Temperature", current_data['air_temperature'], target_air_temp, 1))
    
    with col2:
        custom_metric(
            label="Water Temperature",
            value=f"{current_data['water_temperature']:.1f} °C"
        )
    
    with col3:
        custom_metric(
            label="Humidity",
            value=f"{current_data['humidity']:.0f}%",
            delta=f"{current_data['humidity'] - target_humidity:.0f}%"
        )
        asyncio.run(check_and_alert("Humidity", current_data['humidity'], target_humidity, 5))
    
    with col4:
        custom_metric(
            label="EC",
            value=f"{current_data['ec']:.2f} mS/cm",
            delta=f"{current_data['ec'] - target_ec:.2f} mS/cm"
        )
        asyncio.run(check_and_alert("EC", current_data['ec'], target_ec, 0.3))
    
    with col5:
        custom_metric(
            label="pH",
            value=f"{current_data['ph']:.2f}",
            delta=f"{current_data['ph'] - target_ph:.2f}"
        )
        asyncio.run(check_and_alert("pH", current_data['ph'], target_ph, 0.5))
    
    with col6:
        custom_metric(
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
        custom_metric(
            label="ESP32 Temperature",
            value=f"{current_data.get('esp32_temperature', current_data['air_temperature']):.1f} °C",
            delta=f"{current_data.get('esp32_temperature', current_data['air_temperature']) - target_air_temp:.1f} °C"
        )
    
    with esp32_col2:
        custom_metric(
            label="ESP32 Humidity",
            value=f"{current_data.get('esp32_humidity', current_data['humidity']):.0f}%",
            delta=f"{current_data.get('esp32_humidity', current_data['humidity']) - target_humidity:.0f}%"
        )
    
    # Display ESP32 relay states
    st.subheader("ESP32 Relay States")
    esp32_relay_states = current_data.get('esp32_relay_states', {})
    for relay, state in esp32_relay_states.items():
        st.write(f"{relay.capitalize()}: {'ON' if state else 'OFF'}")
    
    # Add manual override controls for ESP32 relays
    st.subheader("Manual Override")
    for relay in esp32_relay_states:
        if st.button(f"Toggle {relay.capitalize()}"):
            command = {
                relay: not esp32_relay_states[relay]
            }
            simulate_wlan_communication(esp32_controller, 'receive', command)
    
    # Update historical data
    data_manager.add_data_point(current_data)
    
    # Log the completion of the dashboard update
    logger.info("Growroom Control System dashboard updated successfully")

# Function to display historical data charts
def display_historical_data():
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

# Sensor Calibration
st.sidebar.header("Sensor Calibration")
if st.sidebar.button("Calibrate pH Sensor"):
    st.sidebar.info("pH sensor calibration initiated. Follow the instructions on the device.")
    # Add actual calibration logic here

if st.sidebar.button("Calibrate EC Sensor"):
    st.sidebar.info("EC sensor calibration initiated. Follow the instructions on the device.")
    # Add actual calibration logic here

# Main app logic
if __name__ == "__main__":
    update_real_time_data()
    display_historical_data()
    
    # Footer
    st.markdown("---")
    st.markdown("© 2024 Growroom Control System. All rights reserved.")

    # Automatically refresh the app every 5 seconds
    st.empty()
    time.sleep(5)
    st.experimental_rerun()
