"""Schedule-based automation for grow room equipment."""

from __future__ import annotations

import json
from datetime import datetime, time
from pathlib import Path
from typing import Any

from env_config import load_dotenv_if_present

load_dotenv_if_present()


class EquipmentSchedule:
    """Time-based schedule for controllable equipment (lights and ventilation)."""
    
    def __init__(self, config_path: str | Path | None = None):
        self.config_path = config_path or Path(__file__).parent / "schedules.json"
        self.schedules: dict[str, list[dict[str, Any]]] = {}
        # For testing: in-memory mode doesn't persist to file
        self._persist = not (isinstance(self.config_path, str) and self.config_path == ":memory:")
        self._load_config()
    
    def _load_config(self) -> None:
        """Load schedules from JSON config file."""
        if not self._persist:
            self.schedules = self._get_default_schedules()
            return
        
        if Path(self.config_path).exists():
            with open(self.config_path, 'r') as f:
                self.schedules = json.load(f)
            if "water_pump" in self.schedules:
                self.schedules.pop("water_pump", None)
                self._save_config()
        else:
            self.schedules = self._get_default_schedules()
            self._save_config()
    
    def _save_config(self) -> None:
        """Save current schedules to JSON file."""
        if not self._persist:
            return
        Path(self.config_path).parent.mkdir(parents=True, exist_ok=True)
        with open(self.config_path, 'w') as f:
            json.dump(self.schedules, f, indent=2)
    
    def _get_default_schedules(self) -> dict[str, list[dict[str, Any]]]:
        """Return default grow room schedules."""
        return {
            "lights": [
                {
                    "name": "Day Cycle",
                    "start": "06:00",
                    "end": "20:00",
                    "enabled": True,
                    "days": [0, 1, 2, 3, 4, 5, 6]  # All days
                }
            ],
            "exhaust_vent": [
                {
                    "name": "Day Ventilation",
                    "start": "08:00",
                    "end": "18:00",
                    "enabled": False,
                    "days": [0, 1, 2, 3, 4, 5, 6]
                }
            ]
        }
    
    def is_equipment_active(self, equipment: str, current_time: datetime | None = None) -> bool:
        """Check if equipment should be active based on schedule."""
        if current_time is None:
            current_time = datetime.now()
        
        if equipment not in self.schedules:
            return False
        
        current_day = current_time.weekday()  # 0=Monday
        current_time_only = current_time.time()
        
        for schedule in self.schedules[equipment]:
            if not schedule.get("enabled", False):
                continue
            
            if current_day not in schedule.get("days", []):
                continue
            
            start_time = self._parse_time(schedule["start"])
            
            if "end" in schedule:
                end_time = self._parse_time(schedule["end"])
                if start_time <= current_time_only <= end_time:
                    return True
            elif "duration_minutes" in schedule:
                from datetime import timedelta
                start_dt = datetime.combine(current_time.date(), start_time)
                end_dt = start_dt + timedelta(minutes=schedule["duration_minutes"])
                end_time = end_dt.time()
                
                if start_time <= current_time_only <= end_time:
                    return True
        
        return False
    
    def get_active_schedules(self, equipment: str, current_time: datetime | None = None) -> list[dict[str, Any]]:
        """Get all active schedules for equipment at current time."""
        if current_time is None:
            current_time = datetime.now()
        
        active = []
        current_day = current_time.weekday()
        current_time_only = current_time.time()
        
        for schedule in self.schedules.get(equipment, []):
            if not schedule.get("enabled", False):
                continue
            
            if current_day not in schedule.get("days", []):
                continue
            
            start_time = self._parse_time(schedule["start"])
            
            if "end" in schedule:
                end_time = self._parse_time(schedule["end"])
                if start_time <= current_time_only <= end_time:
                    active.append(schedule)
            elif "duration_minutes" in schedule:
                from datetime import timedelta
                start_dt = datetime.combine(current_time.date(), start_time)
                end_dt = start_dt + timedelta(minutes=schedule["duration_minutes"])
                end_time = end_dt.time()
                
                if start_time <= current_time_only <= end_time:
                    active.append(schedule)
        
        return active
    
    def set_schedule(self, equipment: str, schedule: dict[str, Any]) -> None:
        """Add or update a schedule for equipment."""
        if equipment not in self.schedules:
            self.schedules[equipment] = []
        
        self.schedules[equipment].append(schedule)
        self._save_config()
    
    def disable_schedule(self, equipment: str, schedule_name: str) -> None:
        """Disable a specific schedule by name."""
        if equipment in self.schedules:
            for sched in self.schedules[equipment]:
                if sched.get("name") == schedule_name:
                    sched["enabled"] = False
            self._save_config()
    
    def enable_schedule(self, equipment: str, schedule_name: str) -> None:
        """Enable a specific schedule by name."""
        if equipment in self.schedules:
            for sched in self.schedules[equipment]:
                if sched.get("name") == schedule_name:
                    sched["enabled"] = True
            self._save_config()
    
    def get_all_schedules(self) -> dict[str, list[dict[str, Any]]]:
        """Get all configured schedules."""
        return self.schedules
    
    @staticmethod
    def _parse_time(time_str: str) -> time:
        """Parse time string (HH:MM) to time object."""
        parts = time_str.split(":")
        return time(int(parts[0]), int(parts[1]))


if __name__ == "__main__":
    scheduler = EquipmentSchedule()
    
    print("📅 Current Schedules:")
    for equipment, schedules in scheduler.get_all_schedules().items():
        print(f"\n{equipment}:")
        for schedule in schedules:
            status = "✅" if schedule.get("enabled") else "❌"
            print(f"  {status} {schedule['name']}: {schedule['start']} - {schedule.get('end', 'N/A')}")
    
    now = datetime.now()
    print(f"\n🔍 Active right now:")
    for equipment in ["lights", "exhaust_vent"]:
        is_active = scheduler.is_equipment_active(equipment, now)
        print(f"  {equipment}: {'ON' if is_active else 'OFF'}")
