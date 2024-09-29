import random
from datetime import datetime

class SensorSimulator:
    def __init__(self):
        self.air_temperature = 27.0
        self.water_temperature = 22.0
        self.case_temperature = 27.0
        self.humidity = 60
        self.light_level = 1000
        self.ec = 1.5  # mS/cm
        self.ph = 6.5
        self.circulation_vent = False
        self.exhaust_vent = False
        self.heater = False
        self.pump = False
        self.growth_stage = 'early_grow'

    def get_sensor_data(self):
        # Simulate small changes in sensor readings
        self.air_temperature += random.uniform(-0.2, 0.2)
        self.water_temperature += random.uniform(-0.1, 0.1)
        self.case_temperature += random.uniform(-0.2, 0.2)
        self.humidity += random.uniform(-1, 1)
        self.light_level += random.uniform(-20, 20)
        self.ec += random.uniform(-0.05, 0.05)
        self.ph += random.uniform(-0.05, 0.05)

        # Ensure values stay within realistic ranges
        self.air_temperature = max(26, min(29, self.air_temperature))
        self.water_temperature = max(18, min(30, self.water_temperature))
        self.case_temperature = max(26, min(29, self.case_temperature))
        self.humidity = max(30, min(80, self.humidity))
        self.light_level = max(0, min(2000, self.light_level))
        self.ec = max(0.5, min(3.0, self.ec))
        self.ph = max(5.5, min(7.5, self.ph))

        return {
            'timestamp': datetime.now(),
            'air_temperature': round(self.air_temperature, 1),
            'water_temperature': round(self.water_temperature, 1),
            'case_temperature': round(self.case_temperature, 1),
            'humidity': round(self.humidity),
            'light_level': round(self.light_level),
            'ec': round(self.ec, 2),
            'tds': round(self.ec * 500),  # Convert EC to TDS (ppm)
            'ph': round(self.ph, 2),
            'circulation_vent': self.circulation_vent,
            'exhaust_vent': self.exhaust_vent,
            'heater': self.heater,
            'pump': self.pump
        }

    def set_equipment_state(self, equipment, state):
        if equipment in ['circulation_vent', 'exhaust_vent', 'heater', 'pump']:
            setattr(self, equipment, state)

    def set_growth_stage(self, stage):
        if stage in ['early_grow', 'late_grow_early_flowering', 'late_flowering']:
            self.growth_stage = stage

class WLANSimulator:
    @staticmethod
    def simulate_communication(sender, receiver, data=None):
        if data is None:
            data = sender.send_data_to_main_controller()
        
        # Simulate network delay
        import time
        time.sleep(0.1)
        
        # Simulate packet loss (1% chance)
        if random.random() < 0.01:
            return None
        
        return receiver.receive_data_from_esp32(data)

# Example usage
if __name__ == "__main__":
    sensor_sim = SensorSimulator()
    print("Sensor Simulator initialized")
    print("Sensor data:", sensor_sim.get_sensor_data())
    
    from esp32_controller import ESP32Controller
    esp32 = ESP32Controller()
    main_controller = type('MainController', (), {
        'receive_data_from_esp32': lambda self, data: print(f"Received data: {data}")
    })()
    
    wlan_sim = WLANSimulator()
    result = wlan_sim.simulate_communication(esp32, main_controller)
    print("WLAN communication result:", result)
