"""Multi-device management for multiple ESP32 boards."""

from __future__ import annotations

import logging
from datetime import datetime
from typing import Any

from esp32_controller import ESP32Controller

logger = logging.getLogger(__name__)


class MultiDeviceManager:
    """Manage multiple ESP32 devices (sensors + relays)."""
    
    def __init__(self):
        self.devices: dict[str, ESP32Controller] = {}
        self.device_metadata: dict[str, dict[str, Any]] = {}
    
    def register_device(self, device_id: str, description: str = "", location: str = "") -> ESP32Controller:
        """Register a new ESP32 device."""
        if device_id in self.devices:
            logger.warning(f"Device {device_id} already registered, updating metadata")
        
        controller = ESP32Controller(device_id=device_id)
        self.devices[device_id] = controller
        self.device_metadata[device_id] = {
            "description": description,
            "location": location,
            "registered_at": datetime.now().isoformat(),
            "last_seen": None,
            "status": "registered",
        }
        logger.info(f"Registered device: {device_id} ({description})")
        return controller
    
    def unregister_device(self, device_id: str) -> bool:
        """Remove a device from management."""
        if device_id in self.devices:
            del self.devices[device_id]
            del self.device_metadata[device_id]
            logger.info(f"Unregistered device: {device_id}")
            return True
        return False
    
    def get_device(self, device_id: str) -> ESP32Controller | None:
        """Get controller for specific device."""
        return self.devices.get(device_id)
    
    def update_device_status(self, device_id: str, status: str = "online") -> None:
        """Update device last-seen timestamp."""
        if device_id in self.device_metadata:
            self.device_metadata[device_id]["last_seen"] = datetime.now().isoformat()
            self.device_metadata[device_id]["status"] = status
    
    def get_all_readings(self) -> dict[str, dict[str, Any]]:
        """Get latest readings from all devices."""
        readings = {}
        for device_id, controller in self.devices.items():
            try:
                data = controller.read_sensor_data()
                readings[device_id] = {
                    "sensor_data": data,
                    "relay_states": controller.relay_states,
                    "metadata": self.device_metadata.get(device_id, {}),
                }
                self.update_device_status(device_id, "online")
            except Exception as e:
                logger.error(f"Failed to read from {device_id}: {e}")
                self.update_device_status(device_id, "error")
        return readings
    
    def get_merged_readings(self) -> dict[str, Any]:
        """Merge readings from all devices into single dict.
        
        Priority: First device provides primary values (temp, humidity),
        additional devices add prefixed values.
        """
        if not self.devices:
            return {}
        
        # Get first device as primary
        primary_id = next(iter(self.devices))
        primary = self.devices[primary_id]
        merged = primary.read_sensor_data()
        merged["primary_device"] = primary_id
        
        # Add other devices with prefix
        for device_id, controller in list(self.devices.items())[1:]:
            try:
                data = controller.read_sensor_data()
                for key, value in data.items():
                    merged[f"{device_id}_{key}"] = value
            except Exception as e:
                logger.error(f"Failed to read from {device_id}: {e}")
        
        return merged
    
    def control_device_relay(self, device_id: str, relay_name: str, state: bool) -> bool:
        """Control relay on specific device."""
        device = self.devices.get(device_id)
        if not device:
            logger.error(f"Device {device_id} not found")
            return False
        
        try:
            device.control_relay(relay_name, state)
            logger.info(f"Set {device_id}/{relay_name} = {state}")
            return True
        except Exception as e:
            logger.error(f"Failed to control relay: {e}")
            return False
    
    def get_device_list(self) -> list[dict[str, Any]]:
        """Get list of all registered devices with metadata."""
        return [
            {
                "device_id": device_id,
                **metadata,
            }
            for device_id, metadata in self.device_metadata.items()
        ]
    
    def get_device_count(self) -> int:
        """Get number of registered devices."""
        return len(self.devices)
    
    def send_command_to_device(self, device_id: str, command: dict[str, Any]) -> bool:
        """Send command to specific device."""
        device = self.devices.get(device_id)
        if not device:
            return False
        
        try:
            device.receive_command_from_main_controller(command)
            self.update_device_status(device_id, "online")
            return True
        except Exception as e:
            logger.error(f"Failed to send command to {device_id}: {e}")
            self.update_device_status(device_id, "error")
            return False
    
    def broadcast_command(self, command: dict[str, Any]) -> dict[str, bool]:
        """Send command to all devices."""
        results = {}
        for device_id in self.devices:
            results[device_id] = self.send_command_to_device(device_id, command)
        return results


# Global multi-device manager
_multi_device_mgr: MultiDeviceManager | None = None


def get_multi_device_manager() -> MultiDeviceManager:
    """Get or create global multi-device manager."""
    global _multi_device_mgr
    if _multi_device_mgr is None:
        _multi_device_mgr = MultiDeviceManager()
        # Register default device (backward compatibility)
        _multi_device_mgr.register_device(
            device_id="grow",
            description="Primary Growroom ESP32",
            location="Grow Room"
        )
    return _multi_device_mgr
