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

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

st.set_page_config(page_title="Growroom Control System", page_icon="🌱", layout="wide")

def set_custom_font_size(small, medium, large):
    st.markdown(f'''
    <style>
    .small-font {{
        font-size: {small}px !important;
    }}
    .medium-font {{
        font-size: {medium}px !important;
    }}
    .large-font {{
        font-size: {large}px !important;
    }}
    .metric-label {{
        font-size: 15px;
    }}
    .metric-value {{
        font-size: 20px;
    }}
    .metric-delta {{
        font-size: 10px;
    }}
    </style>
    ''', unsafe_allow_html=True)

set_custom_font_size(12, 16, 20)

st.markdown("<h1 class='large-font'>🌱 Growroom Control System</h1>", unsafe_allow_html=True)
st.markdown("<p class='medium-font'>Monitor and manage key environmental parameters for optimal plant growth.</p>", unsafe_allow_html=True)

heater = OutputDevice(17)
circulation_vent = OutputDevice(18)
exhaust_vent = OutputDevice(22)
water_pump = OutputDevice(23)

iot_sensor = IoTSensor()
data_manager = DataManager()
esp32_controller = ESP32Controller()

def control_grow_room_equipment(current_data, target_air_temp, target_humidity, target_ec):
    if current_data['air_temperature'] < target_air_temp - 1:
        GPIO.output(heater.pin, True)
        logger.info("Heater turned ON")
    elif current_data['air_temperature'] > target_air_temp + 1:
        GPIO.output(heater.pin, False)
        logger.info("Heater turned OFF")

    if current_data['humidity'] > target_humidity + 5:
        GPIO.output(circulation_vent.pin, True)
        logger.info("Circulation Vent turned ON")
    elif current_data['humidity'] < target_humidity - 5:
        GPIO.output(circulation_vent.pin, False)
        logger.info("Circulation Vent turned OFF")

    if current_data['air_temperature'] > target_air_temp + 2:
        GPIO.output(exhaust_vent.pin, True)
        logger.info("Exhaust Vent turned ON")
    elif current_data['air_temperature'] < target_air_temp - 2:
        GPIO.output(exhaust_vent.pin, False)
        logger.info("Exhaust Vent turned OFF")

    if current_data['ec'] < target_ec - 0.2:
        GPIO.output(water_pump.pin, True)
        logger.info("Water Pump turned ON")
    elif current_data['ec'] > target_ec + 0.2:
        GPIO.output(water_pump.pin, False)
        logger.info("Water Pump turned OFF")

def send_command_to_esp32(current_data):
    command = {
        'heater': GPIO.input(heater.pin),
        'circulation_vent': GPIO.input(circulation_vent.pin),
        'exhaust_vent': GPIO.input(exhaust_vent.pin),
        'water_pump': GPIO.input(water_pump.pin)
    }
    
    command['oled_data'] = {
        "line1": f"Temp: {current_data.get('esp32_temperature', current_data['air_temperature']):.1f}C",
        "line2": f"Humidity: {current_data.get('esp32_humidity', current_data['humidity']):.0f}%"
    }
    
    return command

async def check_and_alert(param_name, current_value, target_value, threshold):
    if abs(current_value - target_value) > threshold:
        alert_message = f"⚠️ Alert: {param_name} is out of range. Current value: {current_value}, Target value: {target_value}"
        await send_alert(alert_message)
        logger.warning(alert_message)

def custom_metric(label, value, delta=None):
    delta_html = f"<span class='metric-delta'>{delta}</span>" if delta else ""
    return st.markdown(f"""
        <div class="metric">
            <span class="metric-label">{label}</span>
            <span class="metric-value">{value}</span>
            {delta_html}
        </div>
    """, unsafe_allow_html=True)

st.sidebar.markdown("<h3 class='medium-font'>Settings</h3>", unsafe_allow_html=True)
growth_stage = st.sidebar.radio("Growth Stage", ("Early Grow", "Late Grow/Early Flowering", "Late Flowering"))

growth_stage_key = growth_stage.lower().replace(" ", "_").replace("/", "_")

vpd_min, vpd_max, humidity_min, humidity_max = get_vpd_range(growth_stage_key)

target_air_temp = st.sidebar.slider("Target Air Temperature (°C)", 20.0, 35.0, 27.5, 0.1)
target_humidity = st.sidebar.slider("Target Humidity (%)", humidity_min, humidity_max, calculate_target_humidity(target_air_temp, growth_stage_key), 1)
target_ec = st.sidebar.slider("Target EC (mS/cm)", 0.5, 3.0, 1.5, 0.1)
target_ph = st.sidebar.slider("Target pH", 5.5, 7.5, 6.5, 0.1)

logger.info(f"Target values - Air Temp: {target_air_temp}, Humidity: {target_humidity}, EC: {target_ec}, pH: {target_ph}")

def update_real_time_data():
    current_data = iot_sensor.read_sensor_data()
    esp32_data = simulate_wlan_communication(esp32_controller, 'send')
    
    if esp32_data and 'sensor_data' in esp32_data:
        current_data.update(esp32_data['sensor_data'])
    if esp32_data and 'esp32_relay_states' in esp32_data:
        current_data['esp32_relay_states'] = esp32_data['esp32_relay_states']
    
    current_vpd = calculate_vpd(current_data['air_temperature'], current_data['humidity'])
    
    control_grow_room_equipment(current_data, target_air_temp, target_humidity, target_ec)
    
    esp32_command = send_command_to_esp32(current_data)
    simulate_wlan_communication(esp32_controller, 'receive', esp32_command)
    
    st.markdown("<h2 class='medium-font'>Real-time Data</h2>", unsafe_allow_html=True)
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
    
    st.markdown("<h2 class='medium-font'>Equipment Status</h2>", unsafe_allow_html=True)
    equipment_col1, equipment_col2 = st.columns(2)
    
    with equipment_col1:
        st.markdown(f"<p class='small-font'>Heater: {'ON' if GPIO.input(heater.pin) else 'OFF'}</p>", unsafe_allow_html=True)
        st.markdown(f"<p class='small-font'>Circulation Vent: {'ON' if GPIO.input(circulation_vent.pin) else 'OFF'}</p>", unsafe_allow_html=True)
    
    with equipment_col2:
        st.markdown(f"<p class='small-font'>Exhaust Vent: {'ON' if GPIO.input(exhaust_vent.pin) else 'OFF'}</p>", unsafe_allow_html=True)
        st.markdown(f"<p class='small-font'>Water Pump: {'ON' if GPIO.input(water_pump.pin) else 'OFF'}</p>", unsafe_allow_html=True)
    
    data_manager.add_data_point(current_data)
    
    logger.info("Growroom Control System dashboard updated successfully")

def display_historical_data():
    st.markdown("<h2 class='medium-font'>Historical Data</h2>", unsafe_allow_html=True)
    
    end_time = datetime.now()
    start_time = end_time - timedelta(hours=24)
    historical_data = data_manager.get_historical_data(start_time, end_time)
    
    temp_fig = go.Figure()
    temp_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['air_temperature'], mode='lines', name='Air Temperature'))
    temp_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['water_temperature'], mode='lines', name='Water Temperature'))
    temp_fig.update_layout(title='Temperature (Last 24 Hours)', xaxis_title='Time', yaxis_title='Temperature (°C)')
    
    humidity_fig = go.Figure()
    humidity_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['humidity'], mode='lines', name='Humidity'))
    humidity_fig.update_layout(title='Humidity (Last 24 Hours)', xaxis_title='Time', yaxis_title='Humidity (%)')
    
    ec_ph_fig = go.Figure()
    ec_ph_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['ec'], mode='lines', name='EC'))
    ec_ph_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['ph'], mode='lines', name='pH', yaxis='y2'))
    ec_ph_fig.update_layout(
        title='EC and pH (Last 24 Hours)',
        xaxis_title='Time',
        yaxis_title='EC (mS/cm)',
        yaxis2=dict(title='pH', overlaying='y', side='right')
    )
    
    vpd_data = [calculate_vpd(temp, hum) for temp, hum in zip(historical_data['air_temperature'], historical_data['humidity'])]
    vpd_fig = go.Figure()
    vpd_fig.add_trace(go.Scatter(x=historical_data.index, y=vpd_data, mode='lines', name='VPD'))
    vpd_fig.add_hline(y=0.4, line_dash="dash", line_color="red", annotation_text="Min VPD: 0.4 kPa")
    vpd_fig.add_hline(y=1.6, line_dash="dash", line_color="red", annotation_text="Max VPD: 1.6 kPa")
    vpd_fig.add_hrect(y0=0.4, y1=0.8, fillcolor="rgba(0,255,0,0.1)", line_width=0, annotation_text="Early Grow")
    vpd_fig.add_hrect(y0=0.8, y1=1.2, fillcolor="rgba(255,255,0,0.1)", line_width=0, annotation_text="Late Grow/Early Flowering")
    vpd_fig.add_hrect(y0=1.2, y1=1.6, fillcolor="rgba(255,165,0,0.1)", line_width=0, annotation_text="Late Flowering")
    vpd_fig.update_layout(title='VPD (Last 24 Hours)', xaxis_title='Time', yaxis_title='VPD (kPa)')
    
    st.plotly_chart(temp_fig, use_container_width=True)
    st.plotly_chart(humidity_fig, use_container_width=True)
    st.plotly_chart(ec_ph_fig, use_container_width=True)
    st.plotly_chart(vpd_fig, use_container_width=True)

def toggle_heater():
    current_state = GPIO.input(heater.pin)
    GPIO.output(heater.pin, not current_state)
    logger.info(f"Heater toggled to {'ON' if not current_state else 'OFF'}")

st.sidebar.markdown("<h3 class='medium-font'>Sensor Calibration</h3>", unsafe_allow_html=True)
if st.sidebar.button("Calibrate pH Sensor"):
    st.sidebar.info("pH sensor calibration initiated. Follow the instructions on the device.")

if st.sidebar.button("Calibrate EC Sensor"):
    st.sidebar.info("EC sensor calibration initiated. Follow the instructions on the device.")

if __name__ == "__main__":
    update_real_time_data()
    display_historical_data()
    
    if st.button("Toggle Heater"):
        toggle_heater()
        st.rerun()
    
    st.markdown("---")
    st.markdown("<p class='small-font'>© 2024 Growroom Control System. All rights reserved.</p>", unsafe_allow_html=True)