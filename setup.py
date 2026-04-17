"""
Automated setup script for Grow Controller.
Run this after cloning the repo to get started quickly.
"""

import sys
import subprocess
import os
from pathlib import Path

def print_step(step: str, message: str):
    print(f"\n{'='*60}")
    print(f"Step {step}: {message}")
    print('='*60)

def check_python_version():
    """Ensure Python 3.11+ is installed."""
    print_step("1/6", "Checking Python version")
    
    version = sys.version_info
    if version.major < 3 or (version.major == 3 and version.minor < 11):
        print(f"❌ Python 3.11+ required, found {version.major}.{version.minor}")
        print("   Download: https://www.python.org/downloads/")
        return False
    
    print(f"✅ Python {version.major}.{version.minor}.{version.micro} detected")
    return True

def create_venv():
    """Create virtual environment."""
    print_step("2/6", "Creating virtual environment")
    
    venv_path = Path(".venv")
    if venv_path.exists():
        print("⚠️  Virtual environment already exists, skipping")
        return True
    
    try:
        subprocess.run([sys.executable, "-m", "venv", ".venv"], check=True)
        print("✅ Virtual environment created")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Failed to create venv: {e}")
        return False

def install_dependencies():
    """Install Python packages from requirements.txt."""
    print_step("3/6", "Installing dependencies")
    
    pip_path = Path(".venv/Scripts/pip.exe" if sys.platform == "win32" else ".venv/bin/pip")
    if not pip_path.exists():
        print("❌ Virtual environment not activated properly")
        return False
    
    try:
        subprocess.run([str(pip_path), "install", "-r", "requirements.txt"], check=True)
        print("✅ Dependencies installed")
        return True
    except subprocess.CalledProcessError as e:
        print(f"❌ Failed to install dependencies: {e}")
        return False

def create_env_file():
    """Create .env file from example if not exists."""
    print_step("4/6", "Setting up environment configuration")
    
    env_file = Path(".env")
    env_example = Path(".env.example")
    
    if env_file.exists():
        print("⚠️  .env file already exists, skipping")
        return True
    
    if not env_example.exists():
        print("❌ .env.example not found")
        return False
    
    # Copy example
    env_file.write_text(env_example.read_text())
    print("✅ .env file created from template")
    print("\n⚠️  IMPORTANT: Edit .env and set:")
    print("   - DASHBOARD_PASSWORD (change from default!)")
    print("   - TELEMETRY_API_KEY (generate random string)")
    print("   - TELEGRAM_BOT_TOKEN (optional, for notifications)")
    return True

def create_data_directory():
    """Create data directory for SQLite database."""
    print_step("5/6", "Creating data directory")
    
    data_dir = Path("data")
    data_dir.mkdir(exist_ok=True)
    print(f"✅ Data directory created: {data_dir.absolute()}")
    return True

def run_tests():
    """Run test suite to verify installation."""
    print_step("6/6", "Running tests")
    
    python_path = Path(".venv/Scripts/python.exe" if sys.platform == "win32" else ".venv/bin/python")
    if not python_path.exists():
        print("❌ Python not found in venv")
        return False
    
    try:
        result = subprocess.run(
            [str(python_path), "-m", "unittest", "system_tests", "test_extended", "-v"],
            capture_output=True,
            text=True
        )
        
        if result.returncode == 0:
            # Count passed tests
            passed = result.stdout.count(" ... ok")
            print(f"✅ All {passed} tests passed")
            return True
        else:
            print("❌ Some tests failed:")
            print(result.stdout[-500:])  # Last 500 chars
            return False
    except Exception as e:
        print(f"❌ Failed to run tests: {e}")
        return False

def print_next_steps():
    """Show next steps."""
    print(f"\n{'='*60}")
    print("✅ SETUP COMPLETE!")
    print('='*60)
    print("\nNext steps:")
    print("1. Edit .env file:")
    print("   - Change DASHBOARD_PASSWORD")
    print("   - Set TELEMETRY_API_KEY (random string)")
    print("\n2. Start Streamlit Dashboard:")
    if sys.platform == "win32":
        print("   .venv\\Scripts\\activate")
    else:
        print("   source .venv/bin/activate")
    print("   streamlit run main.py")
    print("\n3. Start Telemetry API (optional, separate terminal):")
    print("   python run_telemetry_api.py")
    print("\n4. Login to Dashboard:")
    print("   - Username: admin")
    print("   - Password: (from .env, default: growroom2024)")
    print("\n📚 Documentation: README.md")
    print("🐛 Issues: https://github.com/d4sd1ng/grow_controller/issues")
    print()

def main():
    print("\n" + "="*60)
    print("🌱 Grow Controller - Automated Setup")
    print("="*60)
    
    steps = [
        check_python_version,
        create_venv,
        install_dependencies,
        create_env_file,
        create_data_directory,
        run_tests,
    ]
    
    for step_func in steps:
        if not step_func():
            print("\n❌ Setup failed! Please fix the issue above and retry.")
            sys.exit(1)
    
    print_next_steps()

if __name__ == "__main__":
    main()
