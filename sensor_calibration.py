import time

class SensorCalibration:
    @staticmethod
    def calibrate_ph_sensor():
        print("Starting pH sensor calibration...")
        print("1. Rinse the pH probe with distilled water and dry it.")
        print("2. Place the probe in pH 7.0 buffer solution.")
        time.sleep(5)  # Simulating calibration time
        print("3. Adjust the calibration screw until the reading shows 7.0")
        time.sleep(5)  # Simulating calibration time
        print("4. Rinse the probe and place it in pH 4.0 buffer solution.")
        time.sleep(5)  # Simulating calibration time
        print("5. Adjust the slope screw until the reading shows 4.0")
        time.sleep(5)  # Simulating calibration time
        print("pH sensor calibration complete.")

    @staticmethod
    def calibrate_ec_sensor():
        print("Starting EC sensor calibration...")
        print("1. Rinse the EC probe with distilled water and dry it.")
        print("2. Place the probe in 1413 µS/cm calibration solution.")
        time.sleep(5)  # Simulating calibration time
        print("3. Adjust the calibration screw until the reading shows 1.413 mS/cm")
        time.sleep(5)  # Simulating calibration time
        print("4. Rinse the probe and place it in 2.76 mS/cm calibration solution.")
        time.sleep(5)  # Simulating calibration time
        print("5. Verify that the reading shows 2.76 mS/cm (±0.05 mS/cm)")
        time.sleep(5)  # Simulating calibration time
        print("EC sensor calibration complete.")

# Example usage:
# SensorCalibration.calibrate_ph_sensor()
# SensorCalibration.calibrate_ec_sensor()
