import random
from datetime import datetime

class IoTSensor:
    def __init__(self):
        self.air_temperature = 27.0
        self.water_temperature = 22.0
        self.humidity = 60
        self.ec = 1.5  # mS/cm
        self.ph = 6.5
        self.light_level = 1000

    def read_sensor_data(self):
        # Simulate small changes in sensor readings
        self.air_temperature += random.uniform(-0.2, 0.2)
        self.water_temperature += random.uniform(-0.1, 0.1)
        self.humidity += random.uniform(-1, 1)
        self.ec += random.uniform(-0.05, 0.05)
        self.ph += random.uniform(-0.05, 0.05)
        self.light_level += random.uniform(-20, 20)

        # Ensure values stay within realistic ranges
        self.air_temperature = max(20, min(35, self.air_temperature))
        self.water_temperature = max(18, min(30, self.water_temperature))
        self.humidity = max(30, min(80, self.humidity))
        self.ec = max(0.5, min(3.0, self.ec))
        self.ph = max(5.5, min(7.5, self.ph))
        self.light_level = max(0, min(2000, self.light_level))

        return {
            'timestamp': datetime.now(),
            'air_temperature': round(self.air_temperature, 1),
            'water_temperature': round(self.water_temperature, 1),
            'humidity': round(self.humidity),
            'ec': round(self.ec, 2),
            'ph': round(self.ph, 2),
            'light_level': round(self.light_level)
        }
