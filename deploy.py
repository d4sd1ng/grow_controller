"""
Deploy/Start Grow Controller on a server.
Sets up everything and starts services.
"""

import sys
import subprocess
import os
from pathlib import Path
import time

def run_command(cmd, cwd=None):
    """Run command and return output."""
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=cwd)
    return result.returncode, result.stdout, result.stderr

def check_requirements():
    """Check if all requirements are met."""
    print("\n" + "="*60)
    print("🔍 Checking Requirements")
    print("="*60)
    
    # Python version
    version = sys.version_info
    if version.major < 3 or version.minor < 11:
        print(f"❌ Python 3.11+ required (found {version.major}.{version.minor})")
        return False
    print(f"✅ Python {version.major}.{version.minor}.{version.micro}")
    
    # Check if .env exists
    if not Path(".env").exists():
        print("❌ .env file not found. Run 'python setup.py' first!")
        return False
    print("✅ .env file exists")
    
    # Check if virtual environment exists
    if not Path(".venv").exists():
        print("❌ Virtual environment not found. Run 'python setup.py' first!")
        return False
    print("✅ Virtual environment exists")
    
    # Check dependencies
    python_path = Path(".venv/Scripts/python.exe" if sys.platform == "win32" else ".venv/bin/python")
    if not python_path.exists():
        print("❌ Python not found in virtual environment")
        return False
    
    returncode, _, _ = run_command(f"{python_path} -c 'import streamlit'")
    if returncode != 0:
        print("❌ Dependencies not installed. Run 'python setup.py' first!")
        return False
    print("✅ Dependencies installed")
    
    return True

def create_windows_service_files():
    """Create batch files for running services on Windows."""
    print("\n📝 Creating startup scripts")
    
    # Batch file for Telemetry API
    api_bat = Path("start_telemetry_api.bat")
    api_bat.write_text("""@echo off
echo Starting Telemetry API Server...
call .venv\\Scripts\\activate
python run_telemetry_api.py
pause
""")
    print(f"✅ Created {api_bat}")
    
    # Batch file for Streamlit Dashboard
    dashboard_bat = Path("start_dashboard.bat")
    dashboard_bat.write_text("""@echo off
echo Starting Streamlit Dashboard...
call .venv\\Scripts\\activate
streamlit run main.py --server.headless=true --server.port=8501
pause
""")
    print(f"✅ Created {dashboard_bat}")
    
    # Batch file for both services
    both_bat = Path("start_all_services.bat")
    both_bat.write_text("""@echo off
echo Starting Grow Controller Services...
echo.

:: Start Telemetry API in background
start "Telemetry API" cmd /k "call .venv\\Scripts\\activate && python run_telemetry_api.py"
timeout /t 3 /nobreak >nul

:: Start Dashboard
start "Dashboard" cmd /k "call .venv\\Scripts\\activate && streamlit run main.py --server.headless=true --server.port=8501"

echo.
echo Both services started!
echo - Telemetry API: http://localhost:8087
echo - Dashboard: http://localhost:8501
echo.
pause
""")
    print(f"✅ Created {both_bat}")

def create_linux_service_files():
    """Create systemd service files for Linux."""
    print("\n📝 Creating systemd service files")
    
    install_path = Path.cwd().absolute()
    
    # Telemetry API service
    api_service = f"""[Unit]
Description=Grow Controller Telemetry API
After=network.target

[Service]
Type=simple
User={os.getenv('USER', 'pi')}
WorkingDirectory={install_path}
Environment=PATH={install_path}/.venv/bin
ExecStart={install_path}/.venv/bin/python run_telemetry_api.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
"""
    Path("grow-telemetry-api.service").write_text(api_service)
    print("✅ Created grow-telemetry-api.service")
    
    # Dashboard service
    dashboard_service = f"""[Unit]
Description=Grow Controller Streamlit Dashboard
After=network.target grow-telemetry-api.service

[Service]
Type=simple
User={os.getenv('USER', 'pi')}
WorkingDirectory={install_path}
Environment=PATH={install_path}/.venv/bin
ExecStart={install_path}/.venv/bin/streamlit run main.py --server.headless=true --server.port=8501 --server.address=0.0.0.0
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
"""
    Path("grow-dashboard.service").write_text(dashboard_service)
    print("✅ Created grow-dashboard.service")
    
    print("\n📋 To install as system services:")
    print("   sudo cp grow-*.service /etc/systemd/system/")
    print("   sudo systemctl daemon-reload")
    print("   sudo systemctl enable grow-telemetry-api")
    print("   sudo systemctl enable grow-dashboard")
    print("   sudo systemctl start grow-telemetry-api")
    print("   sudo systemctl start grow-dashboard")

def test_connection():
    """Test if services are running."""
    print("\n🔗 Testing Connections")
    print("-" * 60)
    
    # Test Telemetry API
    try:
        import httpx
        response = httpx.get("http://localhost:8087/api/v1/health", timeout=2)
        if response.status_code == 200:
            print("✅ Telemetry API: http://localhost:8087")
        else:
            print("⚠️  Telemetry API running but not healthy")
    except:
        print("❌ Telemetry API not reachable")
    
    # Test Dashboard
    try:
        import httpx
        response = httpx.get("http://localhost:8501", timeout=2)
        if response.status_code == 200:
            print("✅ Dashboard: http://localhost:8501")
        else:
            print("⚠️  Dashboard running but not responding")
    except:
        print("❌ Dashboard not reachable")

def main():
    print("\n" + "="*60)
    print("🚀 Grow Controller - Server Deployment")
    print("="*60)
    
    if not check_requirements():
        print("\n❌ Requirements not met!")
        print("   Run 'python setup.py' first to prepare the system.")
        sys.exit(1)
    
    print("\n✅ All requirements met!")
    
    # Create startup scripts
    if sys.platform == "win32":
        create_windows_service_files()
    else:
        create_linux_service_files()
    
    # Test connections
    test_connection()
    
    # Show next steps
    print("\n" + "="*60)
    print("✅ DEPLOYMENT READY!")
    print("="*60)
    
    print("\n📋 Quick Start:")
    if sys.platform == "win32":
        print("   Double-click: start_all_services.bat")
        print("   OR run separately:")
        print("   - start_telemetry_api.bat")
        print("   - start_dashboard.bat")
    else:
        print("   Start services:")
        print("   - ./start_telemetry_api.sh")
        print("   - ./start_dashboard.sh")
    
    print("\n🌐 Access:")
    print("   Dashboard: http://localhost:8501")
    print("   Telemetry API: http://localhost:8087")
    print("   API Docs: http://localhost:8087/docs")
    
    print("\n📱 ESP32 Configuration:")
    print("   Set TELEMETRY_URL in ESP firmware to:")
    print("   http://<server-ip>:8087/api/v1/telemetry")
    
    print("\n🔧 Auto-start on boot (Linux):")
    print("   Install systemd services (see above)")
    
    print()

if __name__ == "__main__":
    main()
