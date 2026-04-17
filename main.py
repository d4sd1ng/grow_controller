from __future__ import annotations

import streamlit as st
from datetime import datetime, timedelta, date
import time
import logging
from typing import Any
import plotly.graph_objects as go
import hashlib

from env_config import load_dotenv_if_present
from iot_sensor import IoTSensor
from data_manager import DataManager
from vpd_calculator import calculate_vpd, is_vpd_in_range, get_vpd_range, calculate_target_humidity
# Telegram vorübergehend aus — Meldungen laufen über alerts_log / CYD (mobile/summary).
# from telegram_notify import send_telegram_message
from esp32_controller import ESP32Controller, simulate_wlan_communication
from gpio_simulator import GPIO, OutputDevice
from pid_controller import PIDController
from schedule_manager import EquipmentSchedule
from ml_controller import MLController
from multi_device_manager import get_multi_device_manager

load_dotenv_if_present()

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

_ALERT_COOLDOWN_SEC = 300.0
# Streamlit reruns often; avoid one SQLite row per refresh unless values move or a heartbeat interval passes.
_DASHBOARD_SQLITE_LOG_MIN_INTERVAL_SEC = 60.0
_DASHBOARD_LOG_FLOAT_EPS = (
    ("air_temperature", 0.05),
    ("water_temperature", 0.05),
    ("case_temperature", 0.05),
    ("humidity", 0.2),
    ("light_level", 1.0),
    ("ec", 0.02),
    ("ph", 0.03),
)
_DASHBOARD_LOG_BOOL_KEYS = ("circulation_vent", "exhaust_vent", "heater", "pump")

# User authentication
_DEFAULT_USERNAME = "admin"
_DEFAULT_PASSWORD = "growroom2024"  # Change in production!


def check_credentials(username: str, password: str) -> bool:
    """Verify username/password (simple hash comparison)."""
    # In production, use proper password hashing (bcrypt)
    return username == _DEFAULT_USERNAME and password == _DEFAULT_PASSWORD


def login_screen():
    """Display login form."""
    st.title("🔐 Growroom Control System - Login")
    
    with st.form("login_form"):
        username = st.text_input("Username", value="")
        password = st.text_input("Password", type="password", value="")
        submitted = st.form_submit_button("Login", use_container_width=True)
        
        if submitted:
            if check_credentials(username, password):
                st.session_state.authenticated = True
                st.session_state.username = username
                st.rerun()
            else:
                st.error("❌ Invalid credentials")
    
    st.info("Default: admin / growroom2024")


def check_authentication():
    """Check if user is authenticated, show login if not."""
    if 'authenticated' not in st.session_state:
        st.session_state.authenticated = False
    
    if not st.session_state.authenticated:
        login_screen()
        return False
    return True


def _dashboard_telemetry_changed(prev: dict[str, Any] | None, row: dict[str, Any]) -> bool:
    if prev is None:
        return True
    for key, eps in _DASHBOARD_LOG_FLOAT_EPS:
        try:
            a = float(row[key])
            b = float(prev[key])
        except (KeyError, TypeError, ValueError):
            return True
        if abs(a - b) > eps:
            return True
    for key in _DASHBOARD_LOG_BOOL_KEYS:
        if bool(row.get(key)) != bool(prev.get(key)):
            return True
    return False


def _maybe_log_dashboard_telemetry_row(log_row: dict[str, Any]) -> None:
    now = time.monotonic()
    if "dashboard_sqlite_last_log_mono" not in st.session_state:
        st.session_state.dashboard_sqlite_last_log_mono = 0.0
        st.session_state.dashboard_sqlite_last_logged_snap = None
    prev = st.session_state.dashboard_sqlite_last_logged_snap
    interval_elapsed = (now - float(st.session_state.dashboard_sqlite_last_log_mono)) >= _DASHBOARD_SQLITE_LOG_MIN_INTERVAL_SEC
    if not _dashboard_telemetry_changed(prev, log_row) and not interval_elapsed:
        logger.debug("Skipping dashboard SQLite write (unchanged, within heartbeat interval)")
        return
    data_manager.add_data_point(log_row)
    st.session_state.dashboard_sqlite_last_log_mono = now
    st.session_state.dashboard_sqlite_last_logged_snap = {k: log_row[k] for k, _ in _DASHBOARD_LOG_FLOAT_EPS}
    for k in _DASHBOARD_LOG_BOOL_KEYS:
        st.session_state.dashboard_sqlite_last_logged_snap[k] = bool(log_row.get(k))


def _maybe_notify_alert(alert_key: str, message: str, severity: str = "warn") -> None:
    if "telegram_alert_at" not in st.session_state:
        st.session_state.telegram_alert_at = {}
    now = time.monotonic()
    last = st.session_state.telegram_alert_at.get(alert_key, 0.0)
    if now - last < _ALERT_COOLDOWN_SEC:
        return
    st.session_state.telegram_alert_at[alert_key] = now
    # send_telegram_message(message)
    data_manager.add_alert(severity, message, "dashboard")


def _check_and_alert(param_name: str, current_value: float, target_value: float, threshold: float) -> None:
    if abs(current_value - target_value) <= threshold:
        return
    alert_message = (
        f"⚠️ Alert: {param_name} is out of range. "
        f"Current value: {current_value}, Target value: {target_value}"
    )
    logger.warning(alert_message)
    key = param_name.lower().replace(" ", "_")
    _maybe_notify_alert(key, alert_message)

st.set_page_config(page_title="Growroom Control System", page_icon="🌱", layout="wide")

# Add these variables at the beginning of the script
if 'grow_start_date' not in st.session_state:
    st.session_state.grow_start_date = date.today()
if 'current_growth_stage' not in st.session_state:
    st.session_state.current_growth_stage = "Vegetation"
if 'vegetation_days' not in st.session_state:
    st.session_state.vegetation_days = 0

# Check authentication first
if not check_authentication():
    st.stop()

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
    .stButton > button {{
        font-size: 0.6rem;
        padding: 0.1rem 0.2rem;
        height: auto;
        min-height: 0;
        width: 100%;
    }}
    </style>
    ''', unsafe_allow_html=True)

set_custom_font_size(12, 16, 20)

st.markdown("<h1 class='large-font'>🌱 Growroom Control System</h1>", unsafe_allow_html=True)

# Initialize GPIO devices once per session (avoid recreation on every rerun)
if 'gpio_devices' not in st.session_state:
    st.session_state.gpio_devices = {
        'heater': OutputDevice(17),
        'circulation_vent': OutputDevice(18),
        'exhaust_vent': OutputDevice(22),
        'water_pump': OutputDevice(23),
    }

# Initialize PID controllers once per session
if 'pid_controllers' not in st.session_state:
    st.session_state.pid_controllers = {
        'temperature': PIDController(kp=2.0, ki=0.1, kd=0.5, setpoint=27.5),
        'humidity': PIDController(kp=1.5, ki=0.05, kd=0.3, setpoint=60.0),
        'ec': PIDController(kp=1.0, ki=0.02, kd=0.2, setpoint=1.5),
    }

# Initialize schedule manager once per session
if 'schedule_manager' not in st.session_state:
    st.session_state.schedule_manager = EquipmentSchedule()

# Initialize ML controller once per session
if 'ml_controller' not in st.session_state:
    st.session_state.ml_controller = MLController()

# Initialize multi-device manager
if 'device_manager' not in st.session_state:
    st.session_state.device_manager = get_multi_device_manager()

heater = st.session_state.gpio_devices['heater']
circulation_vent = st.session_state.gpio_devices['circulation_vent']
exhaust_vent = st.session_state.gpio_devices['exhaust_vent']
water_pump = st.session_state.gpio_devices['water_pump']

iot_sensor = IoTSensor()
data_manager = DataManager()
esp32_controller = ESP32Controller()

def control_grow_room_equipment(current_data, target_air_temp, target_humidity, target_ec):
    """PID-based equipment control with hysteresis fallback."""
    pid_temp = st.session_state.pid_controllers['temperature']
    pid_humidity = st.session_state.pid_controllers['humidity']
    pid_ec = st.session_state.pid_controllers['ec']

    # Update PID setpoints
    pid_temp.set_setpoint(target_air_temp)
    pid_humidity.set_setpoint(target_humidity)
    pid_ec.set_setpoint(target_ec)

    # Compute PID outputs
    temp_error = pid_temp.compute(current_data['air_temperature'])
    humidity_error = pid_humidity.compute(current_data['humidity'])
    ec_error = pid_ec.compute(current_data['ec'])

    # Heater control (PID output > 0 means heat needed)
    if temp_error > 0.5:
        if not GPIO.input(heater.pin):
            heater.on()
            logger.info("Heater turned ON (auto)")
            data_manager.add_alert("info", "Heater activated (air temp below target)", "automation")
    elif temp_error < -0.5:
        if GPIO.input(heater.pin):
            heater.off()
            logger.info("Heater turned OFF (auto)")

    # Circulation vent control (humidity management)
    if humidity_error > 2.0:
        if not GPIO.input(circulation_vent.pin):
            circulation_vent.on()
            logger.info("Circulation Vent turned ON (auto)")
            data_manager.add_alert("info", "Circulation vent activated (humidity above target)", "automation")
    elif humidity_error < -2.0:
        if GPIO.input(circulation_vent.pin):
            circulation_vent.off()
            logger.info("Circulation Vent turned OFF (auto)")

    # Exhaust vent control (temperature emergency)
    if current_data['air_temperature'] > target_air_temp + 2:
        if not GPIO.input(exhaust_vent.pin):
            exhaust_vent.on()
            logger.info("Exhaust Vent turned ON (auto)")
            data_manager.add_alert("info", "Exhaust vent activated (air temp above target)", "automation")
    elif current_data['air_temperature'] < target_air_temp - 2:
        if GPIO.input(exhaust_vent.pin):
            exhaust_vent.off()
            logger.info("Exhaust Vent turned OFF (auto)")

    # Water pump control (EC management)
    if ec_error > 0.1:
        if not GPIO.input(water_pump.pin):
            water_pump.on()
            logger.info("Water Pump turned ON (auto)")
    elif ec_error < -0.1:
        if GPIO.input(water_pump.pin):
            water_pump.off()
            logger.info("Water Pump turned OFF (auto)")

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

def custom_metric(label, value, delta=None):
    delta_html = ""
    if delta is not None:
        delta_value = float(delta.split()[0].rstrip('%'))
        if delta_value > 0:
            delta_html = f"<span class='metric-delta' style='color: #FF5722;'>▲ {delta}</span>"
        elif delta_value < 0:
            delta_html = f"<span class='metric-delta' style='color: #009688;'>▼ {delta}</span>"
        else:
            delta_html = f"<span class='metric-delta' style='color: #9E9E9E;'>● {delta}</span>"

    return st.markdown(f"""
        <div class="metric">
            <span class="metric-label">{label}</span>
            <span class="metric-value">{value}</span>
            {delta_html}
        </div>
    """, unsafe_allow_html=True)

st.sidebar.markdown("<h3 class='medium-font'>Settings</h3>", unsafe_allow_html=True)
growth_stage = st.sidebar.radio("Growth Stage", ("Early Grow", "Late Grow/Early Flowering", "Late Flowering"))

# Show user info and logout button
st.sidebar.markdown(f"👤 Logged in as: **{st.session_state.username}**")
if st.sidebar.button("Logout", use_container_width=True):
    st.session_state.authenticated = False
    st.session_state.username = None
    st.rerun()

growth_stage_key = growth_stage.lower().replace(" ", "_").replace("/", "_")

vpd_min, vpd_max, humidity_min, humidity_max = get_vpd_range(growth_stage_key)

target_air_temp = st.sidebar.slider("Target Air Temperature (°C)", 20.0, 35.0, 27.5, 0.1)
target_humidity = st.sidebar.slider("Target Humidity (%)", humidity_min, humidity_max, calculate_target_humidity(target_air_temp, growth_stage_key), 1)
target_ec = st.sidebar.slider("Target EC (mS/cm)", 0.5, 3.0, 1.5, 0.1)
target_ph = st.sidebar.slider("Target pH", 5.5, 7.5, 6.5, 0.1)

logger.info(f"Target values - Air Temp: {target_air_temp}, Humidity: {target_humidity}, EC: {target_ec}, pH: {target_ph}")

# Add this in the sidebar
st.sidebar.markdown("<h3 class='medium-font'>Grow Information</h3>", unsafe_allow_html=True)
st.session_state.grow_start_date = st.sidebar.date_input("Grow Start Date", st.session_state.grow_start_date)
st.session_state.current_growth_stage = st.sidebar.radio("Current Growth Stage", ["Vegetation", "Flowering"])

_ctrl_sidebar = data_manager.get_control_state()
_mode_index = 0 if _ctrl_sidebar["control_mode"] == "auto" else 1
_relay_mode = st.sidebar.radio(
    "Relay-Steuerung",
    ["auto", "manual"],
    index=_mode_index,
    format_func=lambda m: "Automatik (Zielwerte)" if m == "auto" else "Manuell (CYD / Taster hier)",
    help="Manuell: Relais folgen Server-Zielzustand (portables CYD, Taster). Automatik: Regeln im Dashboard setzen die Ausgänge.",
)
if _relay_mode != _ctrl_sidebar["control_mode"]:
    data_manager.set_control_mode(_relay_mode)

# Function to calculate days since start
def days_since_start():
    return (date.today() - st.session_state.grow_start_date).days

def _equipment_for_log():
    return {
        "circulation_vent": bool(GPIO.input(circulation_vent.pin)),
        "exhaust_vent": bool(GPIO.input(exhaust_vent.pin)),
        "heater": bool(GPIO.input(heater.pin)),
        "pump": bool(GPIO.input(water_pump.pin)),
    }


def _apply_control_targets_to_gpio(targets: dict) -> None:
    if targets.get("heater"):
        heater.on()
    else:
        heater.off()
    if targets.get("circulation_vent"):
        circulation_vent.on()
    else:
        circulation_vent.off()
    if targets.get("exhaust_vent"):
        exhaust_vent.on()
    else:
        exhaust_vent.off()
    pump_on = targets.get("pump") or targets.get("water_pump")
    if pump_on:
        water_pump.on()
    else:
        water_pump.off()


def update_real_time_data():
    current_data = iot_sensor.read_sensor_data()
    esp32_data = simulate_wlan_communication(esp32_controller, 'send')

    if esp32_data and "sensor_data" in esp32_data:
        current_data.update(esp32_data["sensor_data"])
    if esp32_data and "esp32_relay_states" in esp32_data:
        current_data["esp32_relay_states"] = esp32_data["esp32_relay_states"]

    current_vpd = calculate_vpd(current_data["air_temperature"], current_data["humidity"])

    # ML anomaly detection
    ml_ctrl = st.session_state.ml_controller
    anomalies = ml_ctrl.detect_anomalies(current_data)
    if anomalies:
        for anomaly in anomalies:
            _maybe_notify_alert(f"anomaly_{anomaly.split(':')[0]}", f"⚠️ {anomaly}", "warn")

    ctrl = data_manager.get_control_state()
    if ctrl["control_mode"] == "auto":
        # Apply schedule-based overrides before PID control
        schedule_mgr = st.session_state.schedule_manager
        if schedule_mgr.is_equipment_active("lights", datetime.now()):
            # Lights schedule active - could control light relay here
            pass
        
        control_grow_room_equipment(current_data, target_air_temp, target_humidity, target_ec)
        data_manager.sync_relay_targets_from_automation(_equipment_for_log())
    else:
        _apply_control_targets_to_gpio(ctrl["targets"])

    esp32_command = send_command_to_esp32(current_data)
    simulate_wlan_communication(esp32_controller, 'receive', esp32_command)

    # Display grow information
    st.markdown("<h2 class='medium-font'>Grow Information</h2>", unsafe_allow_html=True)
    st.markdown(f"<p class='small-font'>Days since start: {days_since_start()}</p>", unsafe_allow_html=True)
    st.markdown(f"<p class='small-font'>Current stage: {st.session_state.current_growth_stage}</p>", unsafe_allow_html=True)
    if st.session_state.current_growth_stage == "Flowering":
        if st.session_state.vegetation_days == 0:
            st.session_state.vegetation_days = days_since_start()
        flowering_days = days_since_start() - st.session_state.vegetation_days
        st.markdown(f"<p class='small-font'>Days in flowering: {flowering_days}</p>", unsafe_allow_html=True)

    st.markdown("<h2 class='medium-font'>Real-time Data</h2>", unsafe_allow_html=True)
    col1, col2, col3 = st.columns(3)
    col4, col5, col6 = st.columns(3)

    with col1:
        custom_metric(
            label="Air Temperature",
            value=f"{current_data['air_temperature']:.1f} °C",
            delta=f"{current_data['air_temperature'] - target_air_temp:.1f} °C"
        )
        _check_and_alert("Air Temperature", current_data["air_temperature"], target_air_temp, 1)

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
        _check_and_alert("Humidity", float(current_data["humidity"]), target_humidity, 5)

    with col4:
        custom_metric(
            label="EC",
            value=f"{current_data['ec']:.2f} mS/cm",
            delta=f"{current_data['ec'] - target_ec:.2f} mS/cm"
        )
        _check_and_alert("EC", current_data["ec"], target_ec, 0.3)

    with col5:
        custom_metric(
            label="pH",
            value=f"{current_data['ph']:.2f}",
            delta=f"{current_data['ph'] - target_ph:.2f}"
        )
        _check_and_alert("pH", current_data["ph"], target_ph, 0.5)

    with col6:
        custom_metric(
            label="VPD",
            value=f"{current_vpd:.2f} kPa",
            delta=f"{current_vpd - (vpd_min + vpd_max) / 2:.2f} kPa"
        )
        if not is_vpd_in_range(current_vpd, growth_stage_key):
            alert_message = f"⚠️ Alert: VPD is out of range for current stage. Current value: {current_vpd:.2f} kPa"
            logger.warning(alert_message)
            _maybe_notify_alert("vpd", alert_message)

    st.markdown("<h2 class='medium-font'>Equipment Status</h2>", unsafe_allow_html=True)
    col1, col2 = st.columns(2)

    with col1:
        st.markdown(f"<p class='small-font'>Heater: {'ON' if GPIO.input(heater.pin) else 'OFF'}</p>", unsafe_allow_html=True)
        if st.button("Toggle Heater", key="toggle_heater", use_container_width=True):
            toggle_heater()
            st.rerun()

        st.markdown(f"<p class='small-font'>Circulation Vent: {'ON' if GPIO.input(circulation_vent.pin) else 'OFF'}</p>", unsafe_allow_html=True)
        if st.button("Toggle Circulation Vent", key="toggle_circulation_vent", use_container_width=True):
            toggle_circulation_vent()
            st.rerun()

    with col2:
        st.markdown(f"<p class='small-font'>Exhaust Vent: {'ON' if GPIO.input(exhaust_vent.pin) else 'OFF'}</p>", unsafe_allow_html=True)
        if st.button("Toggle Exhaust Vent", key="toggle_exhaust_vent", use_container_width=True):
            toggle_exhaust_vent()
            st.rerun()

        st.markdown(f"<p class='small-font'>Water Pump: {'ON' if GPIO.input(water_pump.pin) else 'OFF'}</p>", unsafe_allow_html=True)
        if st.button("Toggle Water Pump", key="toggle_water_pump", use_container_width=True):
            toggle_water_pump()
            st.rerun()

    # Schedule Status
    st.markdown("<h2 class='medium-font'>Active Schedules</h2>", unsafe_allow_html=True)
    schedule_mgr = st.session_state.schedule_manager
    sched_col1, sched_col2, sched_col3 = st.columns(3)
    with sched_col1:
        lights_active = schedule_mgr.is_equipment_active("lights")
        st.markdown(f"<p class='small-font'>💡 Lights: {'Active' if lights_active else 'Inactive'}</p>", unsafe_allow_html=True)
    with sched_col2:
        pump_active = schedule_mgr.is_equipment_active("water_pump")
        st.markdown(f"<p class='small-font'>💧 Water Pump: {'Active' if pump_active else 'Inactive'}</p>", unsafe_allow_html=True)
    with sched_col3:
        vent_active = schedule_mgr.is_equipment_active("exhaust_vent")
        st.markdown(f"<p class='small-font'>💨 Exhaust: {'Active' if vent_active else 'Inactive'}</p>", unsafe_allow_html=True)

    # ML Grow Optimization (VPD + EC + pH)
    st.markdown("<h2 class='medium-font'>🎯 ML Grow Optimization</h2>", unsafe_allow_html=True)
    if ml_ctrl.is_trained:
        st.markdown("<p class='small-font'>✅ ML model trained - Complete grow optimization active</p>", unsafe_allow_html=True)
        
        # Get comprehensive ML recommendations
        ml_grow = ml_ctrl.predict_optimal_grow_conditions(current_data, growth_stage_key)
        
        if ml_grow:
            # Row 1: VPD
            st.markdown("<p class='small-font'><b>VPD Optimization</b></p>", unsafe_allow_html=True)
            vpd_col1, vpd_col2, vpd_col3 = st.columns(3)
            
            with vpd_col1:
                current_temp = current_data['air_temperature']
                suggested_temp = ml_grow.get('suggested_temperature', current_temp)
                st.markdown(f"<p class='small-font'>🌡️ Temp: {suggested_temp}°C</p>", unsafe_allow_html=True)
                if abs(suggested_temp - current_temp) > 0.5:
                    st.markdown(f"<p class='small-font' style='color:orange;'>Current: {current_temp}°C</p>", unsafe_allow_html=True)
            
            with vpd_col2:
                current_hum = current_data['humidity']
                suggested_hum = ml_grow.get('suggested_humidity', current_hum)
                st.markdown(f"<p class='small-font'>💧 Humidity: {suggested_hum}%</p>", unsafe_allow_html=True)
                if abs(suggested_hum - current_hum) > 2:
                    st.markdown(f"<p class='small-font' style='color:orange;'>Current: {current_hum}%</p>", unsafe_allow_html=True)
            
            with vpd_col3:
                predicted_vpd = ml_grow.get('predicted_vpd', current_vpd)
                target_vpd = ml_grow.get('vpd_optimal', 1.0)
                st.markdown(f"<p class='small-font'>📊 VPD: {predicted_vpd} kPa (Target: {target_vpd})</p>", unsafe_allow_html=True)
            
            # Row 2: EC & pH with alerts
            st.markdown("<p class='small-font'><b>Nutrient Optimization</b></p>", unsafe_allow_html=True)
            nutrient_col1, nutrient_col2, nutrient_col3 = st.columns(3)
            
            with nutrient_col1:
                current_ec = current_data['ec']
                suggested_ec = ml_grow.get('suggested_ec', current_ec)
                ec_action = ml_grow.get('ec_action', 'maintain')
                ec_icon = "✅" if ec_action == "maintain" else ("⬆️" if ec_action == "increase" else "⬇️")
                st.markdown(f"<p class='small-font'>{ec_icon} EC: {suggested_ec} mS/cm</p>", unsafe_allow_html=True)
                if abs(suggested_ec - current_ec) > 0.1:
                    st.markdown(f"<p class='small-font' style='color:orange;'>Current: {current_ec} mS/cm</p>", unsafe_allow_html=True)
                    st.markdown(f"<p class='small-font' style='color:red;'>Action: {ec_action}</p>", unsafe_allow_html=True)
            
            with nutrient_col2:
                current_ph = current_data['ph']
                suggested_ph = ml_grow.get('suggested_ph', current_ph)
                ph_action = ml_grow.get('ph_action', 'maintain')
                ph_icon = "✅" if ph_action == "maintain" else ("⬆️" if ph_action == "increase" else "⬇️")
                st.markdown(f"<p class='small-font'>{ph_icon} pH: {suggested_ph}</p>", unsafe_allow_html=True)
                if abs(suggested_ph - current_ph) > 0.1:
                    st.markdown(f"<p class='small-font' style='color:orange;'>Current: {current_ph}</p>", unsafe_allow_html=True)
                    st.markdown(f"<p class='small-font' style='color:red;'>Action: {ph_action}</p>", unsafe_allow_html=True)
            
            with nutrient_col3:
                adj_time = ml_grow.get('optimal_adjustment_time', '08:00')
                st.markdown(f"<p class='small-font'>⏰ Best time: {adj_time}</p>", unsafe_allow_html=True)
                
                # Show adjustment alerts
                if ml_grow.get('ec_adjustment_needed') or ml_grow.get('ph_adjustment_needed'):
                    alerts = []
                    if ml_grow.get('ec_adjustment_needed'):
                        alerts.append(f"EC {ml_grow.get('ec_action')}")
                    if ml_grow.get('ph_adjustment_needed'):
                        alerts.append(f"pH {ml_grow.get('ph_action')}")
                    st.markdown(f"<p class='small-font' style='color:orange;'>⚠️ {', '.join(alerts)}</p>", unsafe_allow_html=True)
    else:
        st.markdown("<p class='small-font'>⏳ ML model not trained yet (needs 10+ data points)</p>", unsafe_allow_html=True)
        st.markdown("<p class='small-font'>💡 Train on historical data to get VPD + EC + pH optimization</p>", unsafe_allow_html=True)
        if st.button("Train ML Model on Grow Data"):
            train_ml_model()

    log_row = {**current_data, **_equipment_for_log()}
    _maybe_log_dashboard_telemetry_row(log_row)

    logger.info("Growroom Control System dashboard updated successfully")

def display_historical_data():
    end_time = datetime.now()
    start_time = end_time - timedelta(hours=24)
    historical_data = data_manager.get_historical_data(start_time, end_time)

    if historical_data.empty:
        st.info("No historical data in the last 24 hours yet. Data accumulates as the dashboard refreshes.")
        return

    temp_fig = go.Figure()
    temp_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['air_temperature'], mode='lines', name='Air Temperature'))
    temp_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['water_temperature'], mode='lines', name='Water Temperature'))
    temp_fig.update_layout(title='Temperature (Last 24 Hours)', xaxis_title='Time', yaxis_title='Temperature (°C)', height=300)

    humidity_fig = go.Figure()
    humidity_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['humidity'], mode='lines', name='Humidity'))
    humidity_fig.update_layout(title='Humidity (Last 24 Hours)', xaxis_title='Time', yaxis_title='Humidity (%)', height=300)

    ec_ph_fig = go.Figure()
    ec_ph_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['ec'], mode='lines', name='EC'))
    ec_ph_fig.add_trace(go.Scatter(x=historical_data.index, y=historical_data['ph'], mode='lines', name='pH', yaxis='y2'))
    ec_ph_fig.update_layout(
        title='EC and pH (Last 24 Hours)',
        xaxis_title='Time',
        yaxis_title='EC (mS/cm)',
        yaxis2=dict(title='pH', overlaying='y', side='right'),
        height=300
    )

    vpd_data = [calculate_vpd(temp, hum) for temp, hum in zip(historical_data['air_temperature'], historical_data['humidity'])]
    vpd_fig = go.Figure()
    vpd_fig.add_trace(go.Scatter(x=historical_data.index, y=vpd_data, mode='lines', name='VPD'))
    vpd_fig.add_hline(y=0.4, line_dash="dash", line_color="#FF5722", annotation_text="Min VPD: 0.4 kPa")
    vpd_fig.add_hline(y=1.6, line_dash="dash", line_color="#FF5722", annotation_text="Max VPD: 1.6 kPa")
    vpd_fig.add_hrect(y0=0.4, y1=0.8, fillcolor="rgba(0,150,136,0.1)", line_width=0, annotation_text="Early Grow")
    vpd_fig.add_hrect(y0=0.8, y1=1.2, fillcolor="rgba(0,150,136,0.2)", line_width=0, annotation_text="Late Grow/Early Flowering")
    vpd_fig.add_hrect(y0=1.2, y1=1.6, fillcolor="rgba(0,150,136,0.3)", line_width=0, annotation_text="Late Flowering")
    vpd_fig.update_layout(title='VPD (Last 24 Hours)', xaxis_title='Time', yaxis_title='VPD (kPa)', height=300)

    st.plotly_chart(temp_fig, use_container_width=True)
    st.plotly_chart(humidity_fig, use_container_width=True)
    st.plotly_chart(ec_ph_fig, use_container_width=True)
    st.plotly_chart(vpd_fig, use_container_width=True)

def toggle_heater():
    new_on = not GPIO.input(heater.pin)
    if new_on:
        heater.on()
    else:
        heater.off()
    data_manager.set_control_targets_partial({"heater": new_on}, switch_to_manual=True)
    logger.info(f"Heater set to {'ON' if new_on else 'OFF'} (manual)")

def toggle_circulation_vent():
    new_on = not GPIO.input(circulation_vent.pin)
    if new_on:
        circulation_vent.on()
    else:
        circulation_vent.off()
    data_manager.set_control_targets_partial({"circulation_vent": new_on}, switch_to_manual=True)
    logger.info(f"Circulation Vent set to {'ON' if new_on else 'OFF'} (manual)")

def toggle_exhaust_vent():
    new_on = not GPIO.input(exhaust_vent.pin)
    if new_on:
        exhaust_vent.on()
    else:
        exhaust_vent.off()
    data_manager.set_control_targets_partial({"exhaust_vent": new_on}, switch_to_manual=True)
    logger.info(f"Exhaust Vent set to {'ON' if new_on else 'OFF'} (manual)")

def toggle_water_pump():
    new_on = not GPIO.input(water_pump.pin)
    if new_on:
        water_pump.on()
    else:
        water_pump.off()
    data_manager.set_control_targets_partial({"water_pump": new_on}, switch_to_manual=True)
    logger.info(f"Water Pump set to {'ON' if new_on else 'OFF'} (manual)")

def train_ml_model():
    """Train ML model on historical grow data (VPD + EC + pH)."""
    ml_ctrl = st.session_state.ml_controller
    try:
        end_time = datetime.now()
        start_time = end_time - timedelta(days=7)
        historical_data = data_manager.get_historical_data(start_time, end_time)
        
        if historical_data.empty or len(historical_data) < 10:
            st.error(f"Not enough data for training (need 10+, have {len(historical_data)})")
            return
        
        # Features: all sensor values
        X = historical_data[['air_temperature', 'humidity', 'water_temperature', 'ec', 'ph']]
        
        # Targets: optimal conditions
        from vpd_calculator import calculate_target_humidity
        
        y_data = []
        for _, row in historical_data.iterrows():
            current_temp = row['air_temperature']
            current_ec = row.get('ec', 1.5)
            current_ph = row.get('ph', 6.5)
            
            y_data.append({
                'optimal_temp': current_temp,  # Learn stable temp
                'optimal_humidity': calculate_target_humidity(current_temp, growth_stage_key),
                'optimal_ec': current_ec,  # Learn EC patterns
                'optimal_ph': current_ph,  # Learn pH patterns
            })
        y = pd.DataFrame(y_data)
        
        score = ml_ctrl.train(X, y)
        ml_ctrl.save_model()
        st.success(f"✅ ML model trained on complete grow data! R-squared: {score:.4f}")
        st.info("💡 Model now suggests optimal VPD + EC + pH adjustments")
        st.rerun()
    except Exception as e:
        st.error(f"Training failed: {e}")
        logger.exception("ML training failed")

st.sidebar.markdown("<h3 class='medium-font'>Sensor Calibration</h3>", unsafe_allow_html=True)
if st.sidebar.button("Calibrate pH Sensor"):
    st.sidebar.info("pH sensor calibration initiated. Follow the instructions on the device.")

if st.sidebar.button("Calibrate EC Sensor"):
    st.sidebar.info("EC sensor calibration initiated. Follow the instructions on the device.")

if __name__ == "__main__":
    update_real_time_data()
    display_historical_data()

    st.markdown("---")
    st.markdown("<p class='small-font'>© 2024 Growroom Control System. All rights reserved.</p>", unsafe_allow_html=True)
