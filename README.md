# Smart Wine Fridge Web UI & Control System
A modern IoT wine management system combining a responsive web interface with intelligent hardware controllers for automated bottle tracking, climate monitoring, and LED-guided bottle placement.

## 🏗️ System Architecture
The Smart Wine Fridge consists of three main components:

- **Web UI (Frontend)**: Vite-based responsive interface for wine management
- **Raspberry Pi Controller (Backend)**: Central brain managing MQTT, web server, and databases  
- **ESP32 Modules (Hardware)**: Drawer controllers with sensors, LEDs, and actuators

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
- Node.js (v18 or higher)
- Python 3 (3.8+)
- Raspberry Pi OS (64-bit recommended)
- Git

### Package Managers
- npm for Node.js packages
- apt for system packages

### Hardware Requirements
- Raspberry Pi 5 (2GB+ RAM)
- ESP32 Development Boards (CP2102 recommended)
- Raspberry Pi Touch Screen (optional)
- M5Unit QRCode Scanner
- Load cells, LEDs, sensors (see hardware specs below)

## 🔧 Installation Guide

### Step 1: System Preparation (Raspberry Pi)
```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install required packages
sudo apt install -y nodejs npm python3-paho-mqtt python3-serial git mosquitto mosquitto-clients

# Install PM2 globally for process management
sudo npm install -g pm2
```

### Step 2: Clone Repository
```bash
# Navigate to installation directory
cd /home/plasticlab

# Clone the main repository
git clone https://github.com/bareaescobar/WineFridge.git

# Navigate to project directory
cd WineFridge

# Initialize and clone the frontend submodule
git submodule init
git submodule update

# Alternative: Clone submodule directly if not working
cd RPI/frontend
git clone https://github.com/bareaescobar/WineFridge_web.git

# Verify submodule is properly cloned
ls -la RPI/frontend/WineFridge_web/
```

### Step 3: Frontend Setup
```bash
# Navigate to frontend directory
cd RPI/frontend/WineFridge_web

# Install dependencies using npm
npm install
```

### Step 4: Backend Setup
The backend files are already included in the repository structure, no copying needed.

### Step 5: Configure Mosquitto MQTT Broker
```bash
# Copy the existing mosquitto configuration
sudo cp ESP32_DrawerX/SWF_mosquitto.conf /etc/mosquitto/conf.d/

# Create log directory
sudo mkdir -p /var/log/mosquitto
sudo chown mosquitto:mosquitto /var/log/mosquitto

# Enable and start mosquitto
sudo systemctl enable mosquitto
sudo systemctl start mosquitto

# Verify mosquitto is running
sudo systemctl status mosquitto
```

### Step 6: PM2 Process Management Setup
The system uses PM2 for automatic process management. All configuration is already included in the repository.

```bash
# Navigate to backend directory
cd RPI/backend

# Start all services using the existing PM2 configuration
pm2 start pm2.config.js

# Save PM2 configuration for auto-restart on boot
pm2 save
pm2 startup

# Verify all services are running
pm2 status
```

If services fail to start:
```bash
# Delete all PM2 processes and restart
pm2 delete all
pm2 start pm2.config.js
```

## 🚀 PM2 Process Management

### Essential PM2 Commands
```bash
# View all running processes
pm2 status

# View detailed process information
pm2 show <process-name>

# View logs for all processes
pm2 logs

# View logs for specific process
pm2 logs web-server
pm2 logs mqtt-handler
pm2 logs barcode-scanner

# Stop all processes
pm2 stop all

# Stop specific process
pm2 stop web-server

# Restart all processes
pm2 restart all

# Restart specific process
pm2 restart web-server

# Delete all processes
pm2 delete all

# Delete specific process
pm2 delete web-server

# Monitor real-time process metrics
pm2 monit

# Reload processes with zero downtime
pm2 reload all
```

### PM2 Configuration
The system runs three main processes:
- **web-server**: Node.js web server with WebSocket and MQTT bridge
- **mqtt-handler**: Python MQTT message processor and system logic
- **barcode-scanner**: Python barcode scanner interface

### Auto-startup Configuration
```bash
# Save current process list
pm2 save

# Generate startup script (run once)
pm2 startup

# Follow the instructions displayed to configure auto-startup
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

#### Bottle Loading:
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
WineFridge/
├── .gitignore
├── .gitmodules
├── README.md
├── filestructure.txt
│
├── ESP32_DrawerX/                    # ESP32 drawer controller code
│   ├── ESP32_DrawerX.ino
│   ├── PubSubClient_mod.h
│   └── SWF_mosquitto.conf           # MQTT broker configuration
│
├── ESP32_Lighting/                   # ESP32 lighting controller code
│   └── ESP32_Lighting.ino
│
├── RPI/                             # Raspberry Pi backend system
│   ├── backend/                     # Server and control logic
│   │   ├── barcode_scanner.py       # UART barcode reader
│   │   ├── mqtt_handler.py          # Main MQTT control logic  
│   │   ├── pm2.config.js            # PM2 process configuration
│   │   └── database/
│   │       ├── inventory.json       # Current wine inventory
│   │       └── wine-catalog.json    # Wine database
│   │
│   └── frontend/                    # Web interface
│       └── WineFridge_web/          # Frontend submodule
│           ├── package.json
│           ├── vite.config.js
│           ├── server.mjs           # Web server
│           ├── content/
│           │   └── main.json        # Configuration data
│           └── src/
│               ├── *.html           # Page templates
│               ├── js/              # JavaScript modules
│               │   ├── main.js
│               │   ├── home.js
│               │   ├── load-bottle.js
│               │   ├── drawer.js
│               │   └── helpers/
│               │       └── helpers.js
│               ├── styles/          # SCSS stylesheets
│               │   ├── main.scss
│               │   ├── variables.scss
│               │   └── pages/
│               ├── img/             # Images and icons
│               ├── fonts/           # Typography
│               └── partials/        # Reusable components
│
└── logs/                           # System logs
    └── mqtt.log
```

## 🎨 Frontend Features

### Page Structure
| Page | Description |
|------|-------------|
| auth | User authentication |
| forgot-password | Password recovery |
| connect-fridge | Initial setup |
| setup-fridge | Configuration |
| home | Main dashboard |
| account | User profile |
| notifications | Alert center |
| load-bottle | Add wines |
| unload-bottle | Remove wines |
| swap-bottle | Move wines |
| drawer | Drawer management |
| drawer-red-wines | Red wine inventory |
| drawer-white-wines | White wine inventory |
| drawer-rose-wines | Rosé wine inventory |

### Technology Stack
- **Build Tool**: Vite
- **Styling**: SCSS with modern CSS features
- **UI Components**: Swiper for carousels
- **Code Formatting**: Prettier with pre-commit hooks

### Development Commands
```bash
# Start development server
npm run dev

# Build for production  
npm run build

# Preview production build
npm run preview

# Format code
npm run format

# Check formatting
npm run format:check
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

#### MQTT Connection Failed:
```bash
# Check mosquitto status
sudo systemctl status mosquitto

# Test MQTT connectivity
mosquitto_sub -h localhost -p 1883 -t "#" -v

# Check configuration
cat /etc/mosquitto/conf.d/SWF_mosquitto.conf
```

#### Web Server Not Starting:
```bash
# Check PM2 status
pm2 status

# Check specific service logs
pm2 logs web-server

# Restart web server
pm2 restart web-server

# Test manual start
cd /home/plasticlab/WineFridge/RPI/frontend/WineFridge_web
node server.mjs
```

#### Frontend Not Building:
```bash
# Clear node modules and reinstall
cd RPI/frontend/WineFridge_web
rm -rf node_modules package-lock.json
npm install

# Check for missing dependencies
npm audit
```

#### PM2 Processes Not Starting:
```bash
# Check PM2 logs
pm2 logs

# Delete all and restart
pm2 delete all
pm2 start RPI/backend/pm2.config.js

# Check Node.js and Python paths
which node
which python3
```

### Log Locations
- **PM2 Processes**: `pm2 logs`
- **Mosquitto**: `/var/log/mosquitto/mosquitto.log`
- **System**: `/home/plasticlab/WineFridge/logs/`
- **Web Server**: `pm2 logs web-server`
- **MQTT Handler**: `pm2 logs mqtt-handler`

##
Note: This system requires careful hardware setup and configuration. Ensure all electrical connections are safe and follow proper IoT security practices.
