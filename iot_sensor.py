import random
from datetime import datetime


class IoTSensor:
    """Simulated grow-room sensor readings (replace with hardware/HTTP in production)."""

    def __init__(self) -> None:
        self.air_temperature = 27.0
        self.water_temperature = 22.0
        self.case_temperature = 23.0
        self.humidity = 60
        self.ec = 1.5
        self.ph = 6.5
        self.light_level = 1000

    def read_sensor_data(self) -> dict:
        self.air_temperature += random.uniform(-0.2, 0.2)
        self.water_temperature += random.uniform(-0.1, 0.1)
        self.case_temperature += random.uniform(-0.1, 0.1)
        self.humidity += random.uniform(-1, 1)
        self.ec += random.uniform(-0.05, 0.05)
        self.ph += random.uniform(-0.05, 0.05)
        self.light_level += random.uniform(-20, 20)

        self.air_temperature = max(20, min(35, self.air_temperature))
        self.water_temperature = max(18, min(30, self.water_temperature))
        self.case_temperature = max(18, min(35, self.case_temperature))
        self.humidity = max(30, min(80, self.humidity))
        self.ec = max(0.5, min(3.0, self.ec))
        self.ph = max(5.5, min(7.5, self.ph))
        self.light_level = max(0, min(2000, self.light_level))

        return {
            "timestamp": datetime.now(),
            "air_temperature": round(self.air_temperature, 1),
            "water_temperature": round(self.water_temperature, 1),
            "case_temperature": round(self.case_temperature, 1),
            "humidity": round(self.humidity),
            "ec": round(self.ec, 2),
            "ph": round(self.ph, 2),
            "light_level": round(self.light_level),
        }

    def get_sensor_data(self) -> dict:
        """Alias for Telegram bot / other callers that expect the old SensorSimulator API."""
        return self.read_sensor_data()
