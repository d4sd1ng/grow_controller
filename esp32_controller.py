"""
Ein simulierter ESP32 für Growroom: Sensoren (AM2302×2, EC/pH, Licht), Relais + optional OLED.
Ersetzt die früheren separaten sensor-/relay-/kompakt-Controller-Dateien.
"""

from __future__ import annotations

import random
from datetime import datetime, timezone
from typing import Any

# Relais-Keys wie vom Growroom-Sketch / Hauptrechner erwartet
_DEFAULT_RELAYS = ("light", "heater", "circulation_vent", "exhaust_vent")


class ESP32Controller:
    """Eine Einheit: Messwerte, Relais, OLED-Daten — wie ein einzelnes Board vor Ort."""

    def __init__(self, device_id: str = "grow") -> None:
        self.device_id = device_id
        # Primäre Raumwerte (für kompakte Keys esp32_temperature / esp32_humidity wie bisher in main.py)
        self.am2302_1_temperature = 25.0
        self.am2302_1_humidity = 60.0
        self.am2302_2_temperature = 25.0
        self.am2302_2_humidity = 60.0
        self.ec = 1.5
        self.ph = 6.0
        self.light_intensity = 0.0

        self.relay_states = {k: False for k in _DEFAULT_RELAYS}
        self.oled_data: dict[str, Any] = {}

    def read_sensor_data(self) -> dict[str, Any]:
        self.am2302_1_temperature = max(
            10.0, min(40.0, self.am2302_1_temperature + random.uniform(-0.5, 0.5))
        )
        self.am2302_1_humidity = max(
            30.0, min(90.0, self.am2302_1_humidity + random.uniform(-1.0, 1.0))
        )
        self.am2302_2_temperature = max(
            10.0, min(40.0, self.am2302_2_temperature + random.uniform(-0.5, 0.5))
        )
        self.am2302_2_humidity = max(
            30.0, min(90.0, self.am2302_2_humidity + random.uniform(-1.0, 1.0))
        )
        self.ec = max(0.5, min(3.0, self.ec + random.uniform(-0.05, 0.05)))
        self.ph = max(5.0, min(8.0, self.ph + random.uniform(-0.1, 0.1)))
        # BH1750 o.ä. — starke Schwankung wie im alten Relay-Simulator
        self.light_intensity = random.uniform(0.0, 100_000.0)

        did = self.device_id
        return {
            # Kompakt (main.py esp32_data)
            "esp32_temperature": round(self.am2302_1_temperature, 1),
            "esp32_humidity": round(self.am2302_1_humidity, 1),
            # Detailliert pro Gerät
            f"esp32_{did}_am2302_1_temperature": round(self.am2302_1_temperature, 1),
            f"esp32_{did}_am2302_1_humidity": round(self.am2302_1_humidity, 1),
            f"esp32_{did}_am2302_2_temperature": round(self.am2302_2_temperature, 1),
            f"esp32_{did}_am2302_2_humidity": round(self.am2302_2_humidity, 1),
            f"esp32_{did}_ec": round(self.ec, 2),
            f"esp32_{did}_ph": round(self.ph, 2),
            f"esp32_{did}_light_intensity": round(self.light_intensity, 1),
        }

    def control_relay(self, relay_name: str, state: bool) -> None:
        if relay_name in self.relay_states:
            self.relay_states[relay_name] = bool(state)
        else:
            raise ValueError(f"Invalid relay name: {relay_name}")

    def update_oled_display(self, data: dict[str, Any]) -> None:
        self.oled_data = dict(data)

    def get_oled_display(self) -> dict[str, Any]:
        return self.oled_data

    def send_data_to_main_controller(self) -> dict[str, Any]:
        sensor_data = self.read_sensor_data()
        return {
            "timestamp": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
            "sensor_data": sensor_data,
            "esp32_relay_states": dict(self.relay_states),
            "device_info": {
                "device_id": self.device_id,
                "firmware_version": "2.1.0-sim",
                "i2c_device_count": 2,
                "has_oled": True,
                "has_bh1750": True,
                "bh1750_address": "0x23",
                "i2c_devices": [
                    {"address": "0x3C", "name": "OLED_SSD1306"},
                    {"address": "0x23", "name": "BH1750_ADDR_LOW"},
                ],
            },
        }

    def receive_command_from_main_controller(self, command: dict[str, Any] | None) -> None:
        if not command:
            return
        nested = command.get("relay_states")
        if isinstance(nested, dict):
            for relay, state in nested.items():
                if relay in self.relay_states:
                    self.control_relay(relay, bool(state))
        else:
            for relay in self.relay_states:
                if relay in command:
                    self.control_relay(relay, bool(command[relay]))
        if "oled_data" in command:
            self.update_oled_display(command["oled_data"])


def simulate_wlan_communication(
    controller: ESP32Controller,
    action: str,
    data: dict[str, Any] | None = None,
) -> dict[str, Any] | None:
    """Kompatibel mit main.py (send/receive) und älteren Tests (receive + relay_states)."""
    if action == "send":
        return controller.send_data_to_main_controller()
    if action == "receive":
        controller.receive_command_from_main_controller(data)
        return {"status": "success"}
    return None


if __name__ == "__main__":
    esp = ESP32Controller()
    print("sensor sample:", esp.read_sensor_data())
    print("wlan send:", simulate_wlan_communication(esp, "send"))
