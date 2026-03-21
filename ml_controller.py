import numpy as np
import pandas as pd
from sklearn.linear_model import LinearRegression
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
import joblib

class MLController:
    def __init__(self):
        self.model = LinearRegression()
        self.scaler = StandardScaler()
        self.is_trained = False

    def train(self, X, y):
        X_scaled = self.scaler.fit_transform(X)
        X_train, X_test, y_train, y_test = train_test_split(X_scaled, y, test_size=0.2, random_state=42)
        self.model.fit(X_train, y_train)
        self.is_trained = True
        score = self.model.score(X_test, y_test)
        print(f"Model R-squared score: {score}")

    def predict(self, X):
        if not self.is_trained:
            raise ValueError("Model is not trained yet.")
        X_scaled = self.scaler.transform(X)
        return self.model.predict(X_scaled)

    def save_model(self, filename):
        joblib.dump((self.model, self.scaler, self.is_trained), filename)

    def load_model(self, filename):
        self.model, self.scaler, self.is_trained = joblib.load(filename)

def prepare_data(historical_data):
    X = historical_data[['air_temperature', 'humidity', 'water_temperature', 'ec', 'ph']]
    
    relay_columns = ['heater', 'circulation_vent', 'exhaust_vent', 'water_pump']
    available_relay_columns = [col for col in relay_columns if col in historical_data.columns]
    
    if available_relay_columns:
        y = historical_data[available_relay_columns]
    else:
        # If no relay states are available, create dummy target variables
        y = pd.DataFrame(0, index=historical_data.index, columns=relay_columns)
    
    return X, y

# Example usage:
# ml_controller = MLController()
# X, y = prepare_data(historical_data)
# ml_controller.train(X, y)
# new_data = np.array([[25.0, 60.0, 22.0, 1.5, 6.5]])
# predictions = ml_controller.predict(new_data)
# ml_controller.save_model('ml_model.joblib')
