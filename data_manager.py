import pandas as pd
import random
from datetime import datetime, timedelta

class DataManager:
    def __init__(self):
        self.data = pd.DataFrame(columns=['timestamp', 'air_temperature', 'water_temperature', 'case_temperature', 
                                          'humidity', 'light_level', 'ec', 'ph', 
                                          'circulation_vent', 'exhaust_vent', 'heater', 'pump'])
        self.data.set_index('timestamp', inplace=True)

    def add_data_point(self, data_point):
        new_data = pd.DataFrame([data_point])
        new_data.set_index('timestamp', inplace=True)
        self.data = pd.concat([self.data, new_data])
        
        # Keep only the last 7 days of data
        self.data = self.data[self.data.index > (datetime.now() - timedelta(days=7))]

    def get_historical_data(self, start_time, end_time):
        return self.data.loc[start_time:end_time]

    def generate_mock_data(self):
        # Generate mock data for the last 7 days
        end_time = datetime.now()
        start_time = end_time - timedelta(days=7)
        date_range = pd.date_range(start=start_time, end=end_time, freq='5T')
        
        mock_data = pd.DataFrame({
            'timestamp': date_range,
            'air_temperature': [round(random.uniform(20, 30), 1) for _ in range(len(date_range))],
            'water_temperature': [round(random.uniform(18, 28), 1) for _ in range(len(date_range))],
            'case_temperature': [round(random.uniform(22, 32), 1) for _ in range(len(date_range))],
            'humidity': [round(random.uniform(40, 70)) for _ in range(len(date_range))],
            'light_level': [round(random.uniform(0, 2000)) for _ in range(len(date_range))],
            'ec': [round(random.uniform(0.5, 3.0), 2) for _ in range(len(date_range))],
            'ph': [round(random.uniform(5.5, 7.5), 2) for _ in range(len(date_range))],
            'circulation_vent': [random.choice([True, False]) for _ in range(len(date_range))],
            'exhaust_vent': [random.choice([True, False]) for _ in range(len(date_range))],
            'heater': [random.choice([True, False]) for _ in range(len(date_range))],
            'pump': [random.choice([True, False]) for _ in range(len(date_range))]
        })
        mock_data.set_index('timestamp', inplace=True)
        self.data = mock_data
