# Smart Wine Fridge Web UI & Control System

A modern IoT wine management system combining a responsive web interface with intelligent hardware controllers for automated bottle tracking, climate monitoring, and LED-guided bottle placement.

## 🏗️ System Architecture

The Smart Wine Fridge consists of three main components:

- **Web UI** (Frontend): Vite-based responsive interface for wine management
- **Raspberry Pi Controller** (Backend): Central brain managing MQTT, web server, and databases
- **ESP32 Modules** (Hardware): Drawer controllers with sensors, LEDs, and actuators

```
┌─────────────────┐    ┌─────────────────┐    ┌─────────────────┐
│   Web Browser   │◄──►│  Raspberry Pi   │◄──►│ ESP32 Drawers   │
│   (Frontend)    │    │   (Backend)     │    │   (Hardware)    │
│                 │    │                 │    │                 │
│ • User Interface│    │ • MQTT Broker   │    │ • Weight Sensors│
│ • Wine Catalog  │    │ • Web Server    │    │ • LED Strips    │
│ • Notifications │    │ • Database      │    │ • Temp/Humidity │
│ • Controls      │    │ • Barcode Reader│    │ • Position Detect│
└─────────────────┘    └─────────────────┘    └─────────────────┘
```

## 📋 Prerequisites

### Software Requirements
- **Node.js** (v18 or higher)
- **Python 3** (3.8+)
- **Raspberry Pi OS** (64-bit recommended)
- **Git**

### Package Managers
- **pnpm** (recommended) or **npm**
- **pip3** for Python packages

### Hardware Requirements
- **Raspberry Pi 5** (2GB+ RAM)
- **ESP32 Development Boards** (CP2102 recommended)
- **Raspberry Pi Touch Screen** (optional)
- **M5Unit QRCode Scanner**
- **Load cells, LEDs, sensors** (see hardware specs below)

## 🔧 Installation Guide

### Step 1: System Preparation (Raspberry Pi)

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install required packages
sudo apt install -y nodejs npm python3-pip git mosquitto mosquitto-clients

# Install PM2 globally for process management
sudo npm install -g pm2

# Install Python dependencies
pip3 install paho-mqtt pyserial
```

### Step 2: Clone Repository

```bash
# Navigate to installation directory
cd /home/plasticlab

# Clone the main repository
git clone https://github.com/bareaescobar/WineFridge.git winefridge
cd winefridge

# Clone frontend submodule (if separate)
git submodule update --init --recursive
```

### Step 3: Frontend Setup

```bash
# Navigate to frontend directory
cd frontend  # or wherever your web UI is located

# Install dependencies (using pnpm - recommended)
pnpm install
# OR using npm
npm install
```

### Step 4: Backend Setup

```bash
# Create backend directory structure
mkdir -p /home/plasticlab/winefridge/backend
mkdir -p /home/plasticlab/winefridge/backend/database
mkdir -p /home/plasticlab/winefridge/logs

# Copy backend files (adjust paths as needed)
cp RPI/backend/* /home/plasticlab/winefridge/backend/
```

### Step 5: Configure Mosquitto MQTT Broker

Create MQTT configuration file:

```bash
# Create mosquitto config directory
sudo mkdir -p /etc/mosquitto/conf.d

# Create configuration file
sudo tee /etc/mosquitto/conf.d/winefridge.conf << EOF
# TCP listener for ESP32 devices
listener 1883
protocol mqtt

# WebSocket listener for web interface
listener 9001
protocol websockets

# Allow anonymous connections (adjust for production)
allow_anonymous true

# Logging
log_dest file /var/log/mosquitto/mosquitto.log
log_type all
EOF

# Create log directory
sudo mkdir -p /var/log/mosquitto
sudo chown mosquitto:mosquitto /var/log/mosquitto

# Enable and start mosquitto
sudo systemctl enable mosquitto
sudo systemctl start mosquitto
```

### Step 6: System Services Setup

Create systemd services for automatic startup:

```bash
# Web server service
sudo tee /etc/systemd/system/winefridge-web.service << EOF
[Unit]
Description=Wine Fridge Web Server
After=network.target mosquitto.service

[Service]
Type=simple
User=plasticlab
WorkingDirectory=/home/plasticlab/winefridge/backend
ExecStart=/usr/bin/node server.js
Restart=always
RestartSec=10
Environment=NODE_ENV=production

[Install]
WantedBy=multi-user.target
EOF

# MQTT Handler service
sudo tee /etc/systemd/system/winefridge-mqtt.service << EOF
[Unit]
Description=Wine Fridge MQTT Handler
After=network.target mosquitto.service

[Service]
Type=simple
User=plasticlab
WorkingDirectory=/home/plasticlab/winefridge/backend
ExecStart=/usr/bin/python3 mqtt_handler.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Barcode scanner service
sudo tee /etc/systemd/system/winefridge-scanner.service << EOF
[Unit]
Description=Wine Fridge Barcode Scanner
After=network.target

[Service]
Type=simple
User=plasticlab
WorkingDirectory=/home/plasticlab/winefridge/backend
ExecStart=/usr/bin/python3 barcode_scanner.py
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# Reload systemd and enable services
sudo systemctl daemon-reload
sudo systemctl enable winefridge-web
sudo systemctl enable winefridge-mqtt
sudo systemctl enable winefridge-scanner
```

## 🚀 Usage Instructions

### Development Mode

For development with hot-reload:

```bash
# Start frontend development server
cd frontend
pnpm dev
# Access at http://localhost:5173

# In another terminal, start backend services
cd backend
pm2 start pm2.config.js
```

### Production Mode

```bash
# Build frontend for production
cd frontend
pnpm build

# Copy built files to backend
cp -r dist/* ../backend/public/

# Start all services
sudo systemctl start mosquitto
sudo systemctl start winefridge-web
sudo systemctl start winefridge-mqtt
sudo systemctl start winefridge-scanner

# Access at http://localhost:3000 or Pi's IP address
```

### After System Reboot

All services should start automatically. To check status:

```bash
# Check service status
sudo systemctl status winefridge-web
sudo systemctl status winefridge-mqtt
sudo systemctl status winefridge-scanner
sudo systemctl status mosquitto

# View logs
journalctl -u winefridge-web -f
journalctl -u winefridge-mqtt -f
```

## 📡 MQTT Protocol

### Topic Structure

Each component uses two topics:
- `winefridge/{component}/command` → Component receives commands
- `winefridge/{component}/status` → Component sends status updates

### Components
- `drawer_1`, `drawer_2`, `drawer_3` (functional drawers)
- `lighting` (display lighting controller)
- `system` (RPI/Web communication)

### Message Format

All messages follow this structure:
```json
{
  "action": "what_to_do",
  "source": "who_sent_this", 
  "data": {
    // Action-specific data
  },
  "timestamp": "2025-06-18T10:30:00Z"
}
```

### Example Commands

**Bottle Loading:**
```bash
# Test bottle scanning
mosquitto_pub -h localhost -p 1883 -t 'winefridge/system/status' -m '{
  "action": "barcode_scanned",
  "source": "barcode_scanner",
  "data": {"barcode": "8410415520628"},
  "timestamp": "2025-06-18T12:00:00Z"
}'

# Test bottle placement expectation
mosquitto_pub -h localhost -p 1883 -t 'winefridge/system/status' -m '{
  "action": "expect_bottle",
  "position": 3,
  "barcode": "8410415520628",
  "bottle_info": {
    "name": "Señorío de los Llanos",
    "type": "Reserva",
    "region": "Valdepeñas",
    "vintage": "2018"
  },
  "timeout_ms": 30000,
  "timestamp": "2025-06-18T12:00:05Z"
}'
```

## 📁 Project Structure

```
smart-wine-fridge/
├── frontend/                    # Web UI (Vite + Vanilla JS)
│   ├── src/
│   │   ├── js/                 # JavaScript modules
│   │   ├── styles/             # SCSS stylesheets
│   │   ├── pages/              # HTML page templates
│   │   └── assets/             # Images, fonts, icons
│   ├── content/
│   │   └── main.json           # Configuration data
│   ├── package.json
│   ├── vite.config.js
│   └── .gitignore
│
├── backend/                     # Raspberry Pi backend
│   ├── server.js               # Web server (Node.js)
│   ├── mqtt_handler.py         # Main control logic
│   ├── barcode_scanner.py      # UART barcode reader
│   ├── pm2.config.js           # Process management
│   └── database/
│       ├── inventory.json      # Current state
│       └── wine-catalog.json   # Wine database
│
├── RPI/                        # Raspberry Pi specific files
└── logs/                       # System logs
```

## 🎨 Frontend Features

### Page Structure

| Page | Description |
|------|-------------|
| `auth` | User authentication |
| `forgot-password` | Password recovery |
| `connect-fridge` | Initial setup |
| `setup-fridge` | Configuration |
| `home` | Main dashboard |
| `account` | User profile |
| `notifications` | Alert center |
| `load-bottle` | Add wines |
| `unload-bottle` | Remove wines |
| `swap-bottle` | Move wines |
| `drawer` | Drawer management |
| `red-wines` | Red wine inventory |
| `white-wines` | White wine inventory |
| `rose-wines` | Rosé wine inventory |

### Technology Stack

- **Build Tool**: Vite
- **Styling**: SCSS with modern CSS features
- **UI Components**: Swiper for carousels
- **Templating**: Handlebars
- **Code Formatting**: Prettier with pre-commit hooks

### Development Commands

```bash
# Start development server
pnpm dev

# Build for production
pnpm build

# Preview production build
pnpm preview

# Format code
pnpm format

# Check formatting
pnpm format:check
```

## 🔧 Hardware Configuration

### Power Requirements
- **Total System**: ~120W peak (150W PSU recommended)
- **Raspberry Pi**: 5V 3A
- **ESP32 Modules**: 5V 2A each
- **LED Strips**: 5V shared supply

### Functional Drawers (1-3)
Each drawer includes:
- Weight sensors (4x load cells + HX711)
- Position detection switches (9x)
- Front LED strips (WS2812B)
- Temperature/humidity sensor (SHT30-D)
- Dual-color general lighting (COB LEDs)

### Display Drawers (4-9)
- LED strips for visual display
- General lighting only
- No sensors (controlled by lighting ESP32)

## 🛠️ Troubleshooting

### Common Issues

**MQTT Connection Failed:**
```bash
# Check mosquitto status
sudo systemctl status mosquitto

# Test MQTT connectivity
mosquitto_sub -h localhost -p 1883 -t "#" -v

# Check configuration
cat /etc/mosquitto/conf.d/winefridge.conf
```

**Web Server Not Starting:**
```bash
# Check Node.js installation
node --version

# Check service logs
journalctl -u winefridge-web -f

# Test manual start
cd /home/plasticlab/winefridge/backend
node server.js
```

**Frontend Not Building:**
```bash
# Clear node modules and reinstall
rm -rf node_modules package-lock.json
pnpm install

# Check for missing dependencies
pnpm audit
```

### Log Locations
- **Web Server**: `journalctl -u winefridge-web`
- **MQTT Handler**: `journalctl -u winefridge-mqtt`
- **Mosquitto**: `/var/log/mosquitto/mosquitto.log`
- **System**: `/home/plasticlab/winefridge/logs/`

## 🔒 Security Notes

**Production Deployment:**
- Change MQTT `allow_anonymous` to `false`
- Set up MQTT user authentication
- Configure firewall rules
- Use HTTPS for web interface
- Regular security updates

## 📝 License

This project is proprietary. See LICENSE file for details.

## 📞 Support

For issues and questions:
- Create GitHub issue
- Check troubleshooting section
- Review system logs

---

**Note**: This system requires careful hardware setup and configuration. Ensure all electrical connections are safe and follow proper IoT security practices.
