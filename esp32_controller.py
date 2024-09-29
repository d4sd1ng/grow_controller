import random
from datetime import datetime

class ESP32Controller:
    def __init__(self):
        self.temperature = 25.0
        self.humidity = 60.0
        self.relay_states = {
            'heater': False,
            'circulation_vent': False,
            'exhaust_vent': False,
            'water_pump': False
        }
        self.oled_data = {}

    def read_sensor_data(self):
        # Simulate reading sensor data
        self.temperature += random.uniform(-0.5, 0.5)
        self.humidity += random.uniform(-1, 1)
        return {
            'esp32_temperature': round(self.temperature, 1),
            'esp32_humidity': round(self.humidity, 1)
        }

    def control_relay(self, relay_name, state):
        if relay_name in self.relay_states:
            self.relay_states[relay_name] = state

    def update_oled_display(self, data):
        self.oled_data = data

    def get_oled_display(self):
        return self.oled_data

    def send_data_to_main_controller(self):
        sensor_data = self.read_sensor_data()
        return {
            'timestamp': datetime.now().isoformat(),
            'sensor_data': sensor_data,
            'esp32_relay_states': self.relay_states
        }

    def receive_command_from_main_controller(self, command):
        for relay, state in command.items():
            if relay in self.relay_states:
                self.control_relay(relay, state)
        if 'oled_data' in command:
            self.update_oled_display(command['oled_data'])

def simulate_wlan_communication(esp32, receive_function):
    # Simulate sending data from ESP32 to main controller
    data = esp32.send_data_to_main_controller()
    return receive_function(data)

# Example usage
if __name__ == "__main__":
    esp32 = ESP32Controller()
    print("ESP32 initialized")
    print("Sensor data:", esp32.read_sensor_data())
    print("Relay states:", esp32.relay_states)
    esp32.update_oled_display({"line1": "Temp: 25.0C", "line2": "Humidity: 60%"})
    print("OLED display:", esp32.get_oled_display())
