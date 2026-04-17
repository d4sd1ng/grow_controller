#!/bin/bash
# Raspberry Pi Deployment Script
# Run this on your Raspberry Pi to set up Grow Controller

set -e  # Exit on error

echo ""
echo "============================================================"
echo "🌱 Grow Controller - Raspberry Pi Setup"
echo "============================================================"
echo ""
echo "This will install everything on your Raspberry Pi."
echo "The Pi will act as server for all ESP32 devices."
echo ""

# Check Python version
echo "🔍 Checking Python version..."
python3 --version || {
    echo "❌ Python3 not found. Installing..."
    sudo apt update
    sudo apt install -y python3 python3-pip python3-venv
}

# Check if git installed
git --version || {
    echo "❌ Git not found. Installing..."
    sudo apt install -y git
}

# Clone repo if not exists
if [ ! -d "grow_controller" ]; then
    echo ""
    echo "📥 Cloning repository..."
    git clone https://github.com/d4sd1ng/grow_controller.git
    cd grow_controller
else
    echo ""
    echo "📁 Using existing grow_controller directory"
    cd grow_controller
    git pull origin main
fi

# Create virtual environment
echo ""
echo "🐍 Setting up virtual environment..."
if [ ! -d ".venv" ]; then
    python3 -m venv .venv
    echo "✅ Virtual environment created"
else
    echo "⚠️  Virtual environment already exists"
fi

# Activate venv
source .venv/bin/activate

# Install dependencies
echo ""
echo "📦 Installing dependencies (this may take a few minutes)..."
pip install --upgrade pip
pip install -r requirements.txt
echo "✅ Dependencies installed"

# Create .env if not exists
if [ ! -f ".env" ]; then
    echo ""
    echo "📝 Creating .env file..."
    cp .env.example .env
    echo "✅ .env file created"
    echo ""
    echo "⚠️  IMPORTANT: Edit .env and set passwords!"
    echo "   nano .env"
else
    echo ""
    echo "⚠️  .env file already exists"
fi

# Create data directory
echo ""
echo "📁 Creating data directory..."
mkdir -p data
echo "✅ Data directory created"

# Run tests
echo ""
echo "🧪 Running tests..."
python -m unittest system_tests test_extended -v 2>&1 | tail -5
echo "✅ Tests completed"

# Configure network
echo ""
echo "============================================================"
echo "🌐 Network Configuration"
echo "============================================================"
echo ""

# Get Pi IP address
PI_IP=$(hostname -I | awk '{print $1}')
echo "📍 Your Raspberry Pi IP: $PI_IP"
echo ""
echo "📱 ESP32 Configuration:"
echo "   Set TELEMETRY_URL in your ESP32 firmware to:"
echo "   http://$PI_IP:8087/api/v1/telemetry"
echo ""

# Create systemd services
echo ""
echo "📝 Creating systemd service files..."

# Telemetry API service
cat > grow-telemetry-api.service << EOF
[Unit]
Description=Grow Controller Telemetry API
After=network.target

[Service]
Type=simple
User=$USER
WorkingDirectory=$(pwd)
Environment=PATH=$(pwd)/.venv/bin
ExecStart=$(pwd)/.venv/bin/python run_telemetry_api.py
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
echo "✅ Created grow-telemetry-api.service"

# Dashboard service
cat > grow-dashboard.service << EOF
[Unit]
Description=Grow Controller Streamlit Dashboard
After=network.target grow-telemetry-api.service

[Service]
Type=simple
User=$USER
WorkingDirectory=$(pwd)
Environment=PATH=$(pwd)/.venv/bin
ExecStart=$(pwd)/.venv/bin/streamlit run main.py --server.headless=true --server.port=8501 --server.address=0.0.0.0
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
echo "✅ Created grow-dashboard.service"

echo ""
echo "============================================================"
echo "✅ SETUP COMPLETE!"
echo "============================================================"
echo ""
echo "📋 Next Steps:"
echo ""
echo "1. Edit .env file with your passwords:"
echo "   nano .env"
echo ""
echo "2. Install systemd services (for auto-start):"
echo "   sudo cp grow-*.service /etc/systemd/system/"
echo "   sudo systemctl daemon-reload"
echo "   sudo systemctl enable grow-telemetry-api"
echo "   sudo systemctl enable grow-dashboard"
echo "   sudo systemctl start grow-telemetry-api"
echo "   sudo systemctl start grow-dashboard"
echo ""
echo "3. OR start manually:"
echo "   ./start_telemetry_api.sh"
echo "   ./start_dashboard.sh"
echo ""
echo "4. Access from any device on your network:"
echo "   Dashboard: http://$PI_IP:8501"
echo "   API: http://$PI_IP:8087"
echo "   API Docs: http://$PI_IP:8087/docs"
echo ""
echo "5. Configure ESP32 devices:"
echo "   Set TELEMETRY_URL to: http://$PI_IP:8087/api/v1/telemetry"
echo ""
echo "📚 Documentation: README.md"
echo ""

# Create start scripts
cat > start_telemetry_api.sh << 'EOF'
#!/bin/bash
cd $(dirname $0)
source .venv/bin/activate
echo "Starting Telemetry API on port 8087..."
python run_telemetry_api.py
EOF
chmod +x start_telemetry_api.sh

cat > start_dashboard.sh << 'EOF'
#!/bin/bash
cd $(dirname $0)
source .venv/bin/activate
echo "Starting Dashboard on port 8501..."
streamlit run main.py --server.headless=true --server.port=8501 --server.address=0.0.0.0
EOF
chmod +x start_dashboard.sh

cat > start_all.sh << EOF
#!/bin/bash
echo "Starting Grow Controller services..."
echo ""

# Start Telemetry API in background
./start_telemetry_api.sh &
sleep 2

# Start Dashboard
./start_dashboard.sh &

echo ""
echo "Services started:"
echo "  Dashboard: http://$PI_IP:8501"
echo "  API: http://$PI_IP:8087"
EOF
chmod +x start_all.sh

echo "✅ Startup scripts created:"
echo "   - start_telemetry_api.sh"
echo "   - start_dashboard.sh"
echo "   - start_all.sh"
echo ""
