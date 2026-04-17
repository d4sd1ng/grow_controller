"""Backup and restore for grow telemetry SQLite database."""

from __future__ import annotations

import shutil
import sys
from datetime import datetime
from pathlib import Path

from env_config import get_grow_data_db_path, load_dotenv_if_present

load_dotenv_if_present()


def backup_db(backup_dir: Path | None = None) -> Path:
    """Create timestamped backup of telemetry database."""
    db_path = get_grow_data_db_path()
    if db_path == ":memory:":
        raise RuntimeError("Cannot backup in-memory database")
    
    db_file = Path(db_path)
    if not db_file.exists():
        raise FileNotFoundError(f"Database not found: {db_file}")
    
    if backup_dir is None:
        backup_dir = db_file.parent / "backups"
    
    backup_dir.mkdir(parents=True, exist_ok=True)
    
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    backup_name = f"grow_telemetry_{timestamp}.db"
    backup_path = backup_dir / backup_name
    
    shutil.copy2(db_file, backup_path)
    print(f"✅ Backup created: {backup_path}")
    return backup_path


def restore_db(backup_path: str | Path) -> None:
    """Restore database from backup file."""
    db_path = get_grow_data_db_path()
    if db_path == ":memory:":
        raise RuntimeError("Cannot restore to in-memory database")
    
    backup_file = Path(backup_path)
    if not backup_file.exists():
        raise FileNotFoundError(f"Backup file not found: {backup_file}")
    
    db_file = Path(db_path)
    db_file.parent.mkdir(parents=True, exist_ok=True)
    
    # Create backup of current DB before restore
    if db_file.exists():
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        pre_restore_backup = db_file.parent / f"pre_restore_{timestamp}.db"
        shutil.copy2(db_file, pre_restore_backup)
        print(f"📦 Current DB backed up to: {pre_restore_backup}")
    
    shutil.copy2(backup_file, db_file)
    print(f"✅ Database restored from: {backup_path}")


def list_backups(backup_dir: Path | None = None) -> list[Path]:
    """List all available backups."""
    db_path = get_grow_data_db_path()
    if db_path == ":memory:":
        print("No backups for in-memory database")
        return []
    
    if backup_dir is None:
        backup_dir = Path(db_path).parent / "backups"
    
    if not backup_dir.exists():
        print("No backups directory found")
        return []
    
    backups = sorted(backup_dir.glob("grow_telemetry_*.db"), reverse=True)
    if not backups:
        print("No backups found")
        return backups
    
    print(f"📦 Available backups ({len(backups)}):")
    for i, backup in enumerate(backups, 1):
        size_kb = backup.stat().st_size / 1024
        print(f"  {i}. {backup.name} ({size_kb:.1f} KB)")
    
    return backups


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  python backup_restore.py backup [directory]")
        print("  python backup_restore.py restore <backup_file>")
        print("  python backup_restore.py list [directory]")
        sys.exit(1)
    
    command = sys.argv[1].lower()
    
    try:
        if command == "backup":
            backup_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else None
            backup_db(backup_dir)
        elif command == "restore":
            if len(sys.argv) < 3:
                print("Error: restore requires backup file path")
                sys.exit(1)
            restore_db(sys.argv[2])
        elif command == "list":
            backup_dir = Path(sys.argv[2]) if len(sys.argv) > 2 else None
            list_backups(backup_dir)
        else:
            print(f"Unknown command: {command}")
            sys.exit(1)
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)
