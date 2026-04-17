import numpy as np
import pandas as pd
from sklearn.linear_model import LinearRegression
from sklearn.model_selection import train_test_split
from sklearn.preprocessing import StandardScaler
from datetime import datetime, timedelta
import joblib
import logging
from pathlib import Path

logger = logging.getLogger(__name__)


class MLController:
    """ML-based prediction and anomaly detection for grow room automation."""
    
    def __init__(self, model_path: str | Path | None = None):
        self.model = LinearRegression()
        self.scaler = StandardScaler()
        self.is_trained = False
        self.model_path = model_path or Path(__file__).parent / "ml_model.joblib"
        
        # Try to load existing model
        if Path(self.model_path).exists():
            try:
                self.load_model(self.model_path)
                logger.info(f"Loaded ML model from {self.model_path}")
            except Exception as e:
                logger.warning(f"Failed to load ML model: {e}")

    def train(self, X, y):
        """Train model on historical data."""
        if len(X) < 10:
            raise ValueError("Need at least 10 samples to train")
        
        X_scaled = self.scaler.fit_transform(X)
        X_train, X_test, y_train, y_test = train_test_split(
            X_scaled, y, test_size=0.2, random_state=42
        )
        self.model.fit(X_train, y_train)
        self.is_trained = True
        score = self.model.score(X_test, y_test)
        logger.info(f"Model R-squared score: {score:.4f}")
        return score

    def predict(self, X):
        """Make predictions with trained model."""
        if not self.is_trained:
            raise ValueError("Model is not trained yet.")
        X_scaled = self.scaler.transform(X)
        return self.model.predict(X_scaled)

    def predict_optimal_vpd_conditions(self, current_readings: dict, growth_stage: str = "early_grow") -> dict:
        """Predict optimal temperature/humidity for target VPD based on growth stage.
        
        ML learns from historical data when VPD was in optimal range and plants thrived.
        Returns recommended temperature/humidity setpoints.
        """
        if not self.is_trained:
            return self._get_manual_vpd_targets(growth_stage)
        
        try:
            from vpd_calculator import get_vpd_range
            
            # Get target VPD range for growth stage
            vpd_min, vpd_max, hum_min, hum_max = get_vpd_range(growth_stage)
            target_vpd = (vpd_min + vpd_max) / 2
            
            # Current conditions
            current_temp = current_readings.get('air_temperature', 25.0)
            current_humidity = current_readings.get('humidity', 60.0)
            
            # ML suggests adjustments based on learned patterns
            X = pd.DataFrame([{
                'air_temperature': current_temp,
                'humidity': current_humidity,
                'water_temperature': current_readings.get('water_temperature', 22.0),
                'ec': current_readings.get('ec', 1.5),
                'ph': current_readings.get('ph', 6.5),
            }])
            
            # Predict optimal temperature setpoint
            predictions = self.predict(X)
            
            # Interpret predictions as temperature/humidity recommendations
            suggested_temp = predictions[0][0] if predictions.shape[1] > 0 else current_temp
            suggested_humidity = predictions[0][1] if predictions.shape[1] > 1 else current_humidity
            
            # Clamp to reasonable ranges
            suggested_temp = max(18.0, min(35.0, suggested_temp))
            suggested_humidity = max(40.0, min(80.0, suggested_humidity))
            
            # Calculate resulting VPD
            from vpd_calculator import calculate_vpd
            predicted_vpd = calculate_vpd(suggested_temp, suggested_humidity)
            
            return {
                'suggested_temperature': round(suggested_temp, 1),
                'suggested_humidity': round(suggested_humidity, 1),
                'predicted_vpd': round(predicted_vpd, 2),
                'target_vpd_range': (vpd_min, vpd_max),
                'vpd_optimal': target_vpd,
                'growth_stage': growth_stage,
            }
        except Exception as e:
            logger.error(f"ML VPD prediction failed: {e}")
            return self._get_manual_vpd_targets(growth_stage)
    
    def predict_optimal_grow_conditions(self, current_readings: dict, growth_stage: str = "early_grow") -> dict:
        """Comprehensive prediction: VPD + EC + pH optimization.
        
        Returns complete grow environment recommendations including:
        - Temperature & Humidity (for VPD)
        - EC targets (nutrient levels)
        - pH targets (nutrient availability)
        - Timing suggestions for adjustments
        """
        if not self.is_trained:
            return self._get_manual_grow_targets(growth_stage)
        
        try:
            from vpd_calculator import get_vpd_range, calculate_vpd
            
            # Current conditions
            current_ec = current_readings.get('ec', 1.5)
            current_ph = current_readings.get('ph', 6.5)
            current_temp = current_readings.get('air_temperature', 25.0)
            current_humidity = current_readings.get('humidity', 60.0)
            
            # Prepare input for ML
            X = pd.DataFrame([{
                'air_temperature': current_temp,
                'humidity': current_humidity,
                'water_temperature': current_readings.get('water_temperature', 22.0),
                'ec': current_ec,
                'ph': current_ph,
            }])
            
            predictions = self.predict(X)
            
            # Extract predictions (columns: temp, humidity, ec, ph)
            suggested_ec = predictions[0][2] if predictions.shape[1] > 2 else current_ec
            suggested_ph = predictions[0][3] if predictions.shape[1] > 3 else current_ph
            
            # Clamp to safe ranges
            suggested_ec = max(0.8, min(2.5, suggested_ec))
            suggested_ph = max(5.8, min(6.8, suggested_ph))
            
            # Calculate VPD with suggested values
            suggested_temp = predictions[0][0] if predictions.shape[1] > 0 else current_temp
            suggested_humidity = predictions[0][1] if predictions.shape[1] > 1 else current_humidity
            suggested_temp = max(18.0, min(35.0, suggested_temp))
            suggested_humidity = max(40.0, min(80.0, suggested_humidity))
            predicted_vpd = calculate_vpd(suggested_temp, suggested_humidity)
            
            vpd_min, vpd_max, _, _ = get_vpd_range(growth_stage)
            
            # Determine if adjustments are needed
            ec_adjustment = abs(suggested_ec - current_ec) > 0.1
            ph_adjustment = abs(suggested_ph - current_ph) > 0.1
            
            return {
                # VPD optimization
                'suggested_temperature': round(suggested_temp, 1),
                'suggested_humidity': round(suggested_humidity, 1),
                'predicted_vpd': round(predicted_vpd, 2),
                'target_vpd_range': (vpd_min, vpd_max),
                
                # EC optimization
                'current_ec': round(current_ec, 2),
                'suggested_ec': round(suggested_ec, 2),
                'ec_adjustment_needed': ec_adjustment,
                'ec_action': 'increase' if suggested_ec > current_ec + 0.1 else ('decrease' if suggested_ec < current_ec - 0.1 else 'maintain'),
                
                # pH optimization
                'current_ph': round(current_ph, 2),
                'suggested_ph': round(suggested_ph, 2),
                'ph_adjustment_needed': ph_adjustment,
                'ph_action': 'increase' if suggested_ph > current_ph + 0.1 else ('decrease' if suggested_ph < current_ph - 0.1 else 'maintain'),
                
                # Timing
                'optimal_adjustment_time': self._get_optimal_adjustment_time(),
                'growth_stage': growth_stage,
            }
        except Exception as e:
            logger.error(f"ML grow optimization failed: {e}")
            return self._get_manual_grow_targets(growth_stage)
    
    def _get_manual_grow_targets(self, growth_stage: str) -> dict:
        """Fallback: Manual grow targets from standard ranges."""
        from vpd_calculator import get_vpd_range, calculate_vpd, calculate_target_humidity
        
        vpd_min, vpd_max, hum_min, hum_max = get_vpd_range(growth_stage)
        baseline_temp = 25.0
        target_humidity = calculate_target_humidity(baseline_temp, growth_stage)
        predicted_vpd = calculate_vpd(baseline_temp, target_humidity)
        
        # Standard EC/pH ranges for grow
        ec_ranges = {
            'early_grow': (1.0, 1.4),
            'late_grow_early_flowering': (1.4, 1.8),
            'late_flowering': (1.2, 1.6)
        }
        ph_optimal = 6.2  # Sweet spot for most stages
        
        ec_min, ec_max = ec_ranges.get(growth_stage, (1.2, 1.6))
        suggested_ec = (ec_min + ec_max) / 2
        
        return {
            'suggested_temperature': baseline_temp,
            'suggested_humidity': target_humidity,
            'predicted_vpd': round(predicted_vpd, 2),
            'target_vpd_range': (vpd_min, vpd_max),
            'current_ec': suggested_ec,
            'suggested_ec': round(suggested_ec, 2),
            'ec_adjustment_needed': False,
            'ec_action': 'maintain',
            'current_ph': ph_optimal,
            'suggested_ph': ph_optimal,
            'ph_adjustment_needed': False,
            'ph_action': 'maintain',
            'optimal_adjustment_time': '08:00',
            'growth_stage': growth_stage,
        }
    
    def _get_optimal_adjustment_time(self) -> str:
        """Suggest best time for EC/pH adjustments (typically morning)."""
        # Best practice: Early morning when lights are on
        return "08:00"
    
    def _get_manual_vpd_targets(self, growth_stage: str) -> dict:
        """Fallback: Manual VPD targets from calculator."""
        from vpd_calculator import get_vpd_range, calculate_target_humidity
        
        vpd_min, vpd_max, hum_min, hum_max = get_vpd_range(growth_stage)
        target_vpd = (vpd_min + vpd_max) / 2
        
        # Assume 25°C as baseline
        baseline_temp = 25.0
        target_humidity = calculate_target_humidity(baseline_temp, growth_stage)
        
        from vpd_calculator import calculate_vpd
        predicted_vpd = calculate_vpd(baseline_temp, target_humidity)
        
        return {
            'suggested_temperature': baseline_temp,
            'suggested_humidity': target_humidity,
            'predicted_vpd': round(predicted_vpd, 2),
            'target_vpd_range': (vpd_min, vpd_max),
            'vpd_optimal': target_vpd,
            'growth_stage': growth_stage,
        }

    def detect_anomalies(self, current_readings: dict, threshold: float = 2.0) -> list[str]:
        """Detect anomalous sensor readings."""
        anomalies = []
        
        # Simple statistical anomaly detection
        ranges = {
            'air_temperature': (18.0, 35.0),
            'humidity': (40.0, 80.0),
            'water_temperature': (15.0, 30.0),
            'ec': (0.5, 3.5),
            'ph': (5.5, 7.5),
        }
        
        for sensor, (low, high) in ranges.items():
            value = current_readings.get(sensor)
            if value is not None:
                if value < low or value > high:
                    anomalies.append(f"{sensor}: {value} out of range [{low}-{high}]")
        
        return anomalies

    def save_model(self, filename=None):
        """Save trained model to disk."""
        path = filename or self.model_path
        joblib.dump((self.model, self.scaler, self.is_trained), path)
        logger.info(f"Model saved to {path}")

    def load_model(self, filename):
        """Load model from disk."""
        self.model, self.scaler, self.is_trained = joblib.load(filename)
        self.is_trained = True


def prepare_data(historical_data):
    """Prepare DataFrame for ML training."""
    X = historical_data[['air_temperature', 'humidity', 'water_temperature', 'ec', 'ph']]
    
    relay_columns = ['heater', 'circulation_vent', 'exhaust_vent', 'water_pump']
    available_relay_columns = [col for col in relay_columns if col in historical_data.columns]
    
    if available_relay_columns:
        y = historical_data[available_relay_columns].astype(int)
    else:
        # If no relay states are available, create dummy target variables
        y = pd.DataFrame(0, index=historical_data.index, columns=relay_columns)
    
    return X, y
