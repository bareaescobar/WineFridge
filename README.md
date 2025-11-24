# Smart Wine Fridge v8 - Complete System

A professional IoT wine management system combining intelligent hardware control, automated inventory tracking, and a responsive web interface for seamless wine storage management.

## 📋 Table of Contents

1. [System Overview](#system-overview)
2. [Hardware Architecture](#hardware-architecture)
3. [Prerequisites](#prerequisites)
4. [Installation Guide](#installation-guide)
5. [User Operation Guides](#user-operation-guides)
6. [System Control](#system-control)
7. [MQTT Protocol Reference](#mqtt-protocol-reference)
8. [Troubleshooting](#troubleshooting)
9. [Maintenance](#maintenance)

---

## 🏗️ System Overview

The Smart Wine Fridge manages wine storage across **9 drawers** organized in **3 temperature zones**, featuring automated bottle detection, weight sensing, intelligent LED guidance, and real-time inventory management.

### System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      WEB INTERFACE                          │
│              (Vite + Vanilla JavaScript)                    │
│         WebSocket + MQTT for Real-time Updates             │
└─────────────────┬───────────────────────────────────────────┘
                  │
┌─────────────────▼───────────────────────────────────────────┐
│              RASPBERRY PI 5 CONTROLLER                      │
│  ┌──────────────┬────────────────┬─────────────────┐       │
│  │ MQTT Broker  │  Web Server    │  Database       │       │
│  │ (Mosquitto)  │  (Node.js)     │  (JSON Files)   │       │
│  └──────────────┴────────────────┴─────────────────┘       │
│  ┌──────────────────────────────────────────────────┐      │
│  │  Barcode Scanner (Python) + MQTT Handler         │      │
│  └──────────────────────────────────────────────────┘      │
└─────────────────┬───────────────────────────────────────────┘
                  │ MQTT @ 192.168.1.84
┌─────────────────▼───────────────────────────────────────────┐
│                    ESP32 CONTROLLERS                        │
│  ┌───────────────┐  ┌────────────────┐  ┌────────────────┐ │
│  │  Functional   │  │   Lighting     │  │   Display      │ │
│  │  Drawers      │  │   Controllers  │  │   Drawers      │ │
│  │  (3, 5, 7)    │  │   (2, 6, 8)    │  │   (1, 4, 9)    │ │
│  │               │  │                │  │                │ │
│  │ • Weight      │  │ • Zone LEDs    │  │ • LED Strips   │ │
│  │ • Switches    │  │ • Drawer LEDs  │  │ • General      │ │
│  │ • LEDs        │  │ • COB Lighting │  │   Lighting     │ │
│  │ • Temp/Humid  │  └────────────────┘  └────────────────┘ │
│  │ • COB Lights  │                                          │
│  └───────────────┘                                          │
└─────────────────────────────────────────────────────────────┘
```

### Key Features

- **Automated Wine Routing**: Rosé → Drawer 3, White → Drawer 5, Red → Drawer 7
- **Real-time Inventory**: Weight-based bottle detection with position tracking
- **LED Guidance**: Visual feedback for load/unload/swap operations
- **Environmental Monitoring**: Temperature and humidity tracking per functional drawer
- **Multi-zone Lighting**: Independent control of 3 climate zones
- **Barcode Integration**: M5Unit QRCode scanner for wine identification
- **Web Dashboard**: Responsive interface with real-time updates

---

## 🔧 Hardware Architecture

### Drawer Layout

```
┌─────────────────────────────────────┐
│  Drawer 1  │ Display Only           │
├────────────┴────────────────────────┤
│  Drawer 2  │ Zone 1 Controller      │ ← Controls Zone 1 + Drawers 1-2
├─────────────────────────────────────┤
│  Drawer 3  │ FUNCTIONAL (Rosé)      │ ← Full sensors, 9 positions
├─────────────────────────────────────┤
│           ZONE 1 (Upper)            │ ← COB Lighting
├─────────────────────────────────────┤
│  Drawer 4  │ Display Only           │
├────────────┴────────────────────────┤
│  Drawer 5  │ FUNCTIONAL (White)     │ ← Full sensors, 9 positions
├────────────┬────────────────────────┤
│  Drawer 6  │ Zone 2 Controller      │ ← Controls Zone 2 + Drawers 4-6
├─────────────────────────────────────┤
│           ZONE 2 (Middle)           │ ← COB Lighting
├─────────────────────────────────────┤
│  Drawer 7  │ FUNCTIONAL (Red)       │ ← Full sensors, 9 positions
├────────────┴────────────────────────┤
│  Drawer 8  │ Zone 3 Controller      │ ← Controls Zone 3 + Drawers 7-9
├─────────────────────────────────────┤
│  Drawer 9  │ Display Only           │
├────────────┴────────────────────────┤
│           ZONE 3 (Lower)            │ ← COB Lighting
└─────────────────────────────────────┘
```

### Bottle Position Layout (All Drawers)

Each drawer supports 9 bottle positions in two rows:

```
Front View (looking at drawer):
┌─────┬─────┬─────┬─────┬─────┐
│  5  │  6  │  7  │  8  │  9  │  Front row (5 positions)
└─────┴─────┴─────┴─────┴─────┘
  ┌─────┬─────┬─────┬─────┐
  │  1  │  2  │  3  │  4  │      Rear row (4 positions)
  └─────┴─────┴─────┴─────┘
```

### Component Specifications

#### Functional Drawers (3, 5, 7)
- **ESP32 WROOM** (30-pin): Main controller
- **HX711 + 4x Load Cells**: Weight sensing (calibrated to ±10g)
- **9x Mechanical Switches**: Bottle position detection
- **WS2812B LED Strip**: 17 LEDs (9 positions + ambient)
- **SHT31 Sensor**: Temperature/humidity monitoring
- **COB LED Strips**: Dual-color (2700K warm + 6500K cool)
- **Firmware**: v6.3.0

#### Lighting Controllers (2, 6, 8)
- **ESP32 WROOM** (30-pin): Controller for 2 drawers + 1 zone
- **WS2812B LED Strips**: 2x 17 LEDs for display drawers
- **COB LED Strips**: 6 channels (2 drawers × 2 colors + zone × 2 colors)
- **TIP120 Transistors**: PWM control for COB lighting

#### Raspberry Pi 5 Backend
- **2GB RAM** minimum (4GB recommended)
- **microSD Card**: 32GB+ (Class 10)
- **USB**: M5Unit QRCode scanner
- **Power**: 5V from main 200W power supply

### Power Requirements
- **Main 5V Power Supply**: 200W (powers all components)
  - Raspberry Pi 5: ~15W
  - ESP32 Controllers: 6x @ 5W each = 30W
  - LED Strips: ~20W
  - COB Lighting: ~135W (18 channels @ 7.5W each)

---

## 📋 Prerequisites

### Software Requirements

#### Raspberry Pi OS
```bash
# Check current OS version
cat /etc/os-release
# Recommended: Raspberry Pi OS (64-bit) Bookworm or later
```

#### Node.js
```bash
# Check Node.js version
node --version
# Required: v18.0.0 or higher

# Check npm version
npm --version
# Required: v9.0.0 or higher

# Install/Update if needed:
curl -fsSL https://deb.nodesource.com/setup_18.x | sudo -E bash -
sudo apt install -y nodejs
```

#### Python
```bash
# Check Python version
python3 --version
# Required: Python 3.8 or higher

# Check pip version
python3 -m pip --version
# Required: pip 20.0 or higher
```

#### MQTT Broker
```bash
# Check if Mosquitto is installed
mosquitto -h
# Required: Mosquitto v2.0 or higher

# Check service status
sudo systemctl status mosquitto
```

#### PM2 Process Manager
```bash
# Check PM2 installation
pm2 --version
# Required: PM2 v5.0 or higher

# Install globally if needed
sudo npm install -g pm2
```

### Required Python Packages
```bash
# Packages are installed via apt (system packages)
# Check installed packages
dpkg -l | grep -E "python3-paho-mqtt|python3-serial"

# Install if missing (included in Step 1)
sudo apt install -y python3-paho-mqtt python3-serial
```

### Network Requirements
- **Static IP for Raspberry Pi**: Recommended (default: 192.168.1.84)
- **WiFi Access**: For ESP32 controllers and Raspberry Pi
- **Port Access**: 
  - 1883 (MQTT)
  - 9001 (WebSocket)
  - 3000 (Web Server)

---

## 🚀 Installation Guide

### Step 1: Prepare Raspberry Pi

```bash
# Update system
sudo apt update && sudo apt upgrade -y

# Install system dependencies including Python packages
sudo apt install -y git nodejs npm mosquitto mosquitto-clients python3-paho-mqtt python3-serial

# Install PM2 globally
sudo npm install -g pm2

# Verify installations
node --version    # Should be v18+
npm --version     # Should be v9+
python3 --version # Should be 3.8+
pm2 --version     # Should be 5.0+
```

### Step 2: Clone Repository

```bash
# Navigate to home directory
cd /home/plasticlab

# Clone main repository
git clone https://github.com/bareaescobar/WineFridge.git
cd WineFridge

# Verify structure
ls -la
# Should see: ESP32_Drawer3/, ESP32_Drawer5/, ESP32_Drawer7/, 
#             ESP32_Lighting/, RPI/, logs/
```

### Step 3: Configure MQTT Broker

```bash
# Create Mosquitto configuration
sudo mkdir -p /etc/mosquitto/conf.d

# Copy configuration file
sudo cp ESP32_Drawer3/SWF_mosquitto.conf /etc/mosquitto/conf.d/

# Create log directory
sudo mkdir -p /var/log/mosquitto
sudo chown mosquitto:mosquitto /var/log/mosquitto

# Enable and start Mosquitto
sudo systemctl enable mosquitto
sudo systemctl restart mosquitto

# Verify Mosquitto is running
sudo systemctl status mosquitto

# Test MQTT connectivity
mosquitto_sub -h localhost -t "test" &
mosquitto_pub -h localhost -t "test" -m "Hello MQTT"
# You should see "Hello MQTT" printed
killall mosquitto_sub
```

### Step 4: Setup Frontend

```bash
# Navigate to frontend directory
cd /home/plasticlab/WineFridge/RPI/frontend

# Install dependencies
npm install

# Build for production
npm run build

# Verify build
ls -la dist/
# Should see compiled JavaScript, CSS, and HTML files
```

### Step 5: Configure PM2 Services

```bash
# Start all services using PM2 configuration
cd /home/plasticlab/WineFridge/RPI
pm2 start pm2.config.js

# Check status
pm2 status
# Should see 3 processes: web-server, mqtt-handler, barcode-scanner

# View logs
pm2 logs

# Save PM2 configuration
pm2 save

# Setup PM2 to start on boot
pm2 startup
# Follow the instructions displayed (copy-paste the command shown)

# Verify auto-startup
sudo reboot
# After reboot, check:
pm2 status
```

### Step 6: Flash ESP32 Controllers

#### For Functional Drawers (3, 5, 7):

1. Open Arduino IDE
2. Install libraries:
   - WiFi (built-in)
   - PubSubClient
   - Adafruit_NeoPixel
   - HX711
   - Adafruit_SHT31
   - ArduinoJson
3. Open `ESP32_Drawer{X}/ESP32_Drawer{X}.ino` (where X = 3, 5, or 7)
4. Verify WiFi credentials in code:
   ```cpp
   #define WIFI_SSID_1 "Your_SSID"
   #define WIFI_PASSWORD_1 "Your_Password"
   ```
5. Verify MQTT broker IP:
   ```cpp
   #define MQTT_BROKER_IP "192.168.1.84"
   ```
6. Select board: "ESP32 Dev Module"
7. Upload firmware
8. Monitor serial output (115200 baud) for connection confirmation

#### For Lighting Controllers (2, 6, 8):

1. Open `ESP32_Lighting/ESP32_Lighting.ino`
2. Update controller-specific defines for each unit:
   ```cpp
   // For Drawer 2 (Zone 1 Controller):
   #define CONTROLLER_ID "lighting_1"
   #define DRAWER_START 1
   #define ZONE_ID 1
   
   // For Drawer 6 (Zone 2 Controller):
   #define CONTROLLER_ID "lighting_2"
   #define DRAWER_START 4
   #define ZONE_ID 2
   
   // For Drawer 8 (Zone 3 Controller):
   #define CONTROLLER_ID "lighting_3"
   #define DRAWER_START 7
   #define ZONE_ID 3
   ```
3. Verify WiFi and MQTT settings
4. Upload to each ESP32

### Step 7: Verify System

```bash
# Monitor MQTT messages
mosquitto_sub -h 192.168.1.84 -t "winefridge/#" -v

# You should see heartbeat messages every 60-90 seconds from:
# - winefridge/drawer_3/status
# - winefridge/drawer_5/status
# - winefridge/drawer_7/status
# - winefridge/lighting_1/status
# - winefridge/lighting_2/status
# - winefridge/lighting_3/status
```

### Step 8: Access Web Interface

```bash
# Find Raspberry Pi IP address
hostname -I

# Access from browser:
# http://192.168.1.84:3000
# or
# http://[YOUR_RPI_IP]:3000
```

---

## 👤 User Operation Guides

### 🍷 Loading a Bottle (Correct Process)

This is the primary workflow for adding new wines to the fridge.

#### Steps:

1. **Scan Barcode**
   - Place wine bottle barcode under the M5Unit scanner
   - Wait for confirmation light
   - System automatically identifies wine type (red/white/rosé)

2. **System Assigns Position**
   - Backend determines appropriate drawer based on wine type:
     - **Rosé wines** → Drawer 3
     - **White wines** → Drawer 5
     - **Red wines** → Drawer 7
   - Finds first available position in assigned drawer

3. **LED Guidance - Correct Position**
   - **GREEN BLINKING LED** illuminates at the correct position
   - Web interface shows drawer and position number
   - Audio feedback (if enabled)

4. **Place Bottle**
   - Open the indicated drawer
   - Place bottle at the **GREEN BLINKING position**
   - Switch detects bottle presence
   - Weight sensor confirms bottle placement

5. **Confirmation**
   - **GREEN LED** turns **SOLID** briefly
   - LED fades to **GRAY** (occupied position)
   - Web interface updates inventory
   - Success message displayed

**Total Time**: 5-10 seconds

#### Visual Feedback Summary:
- 🟢 **Green Blinking** = Correct position, place bottle here
- 🟢 **Green Solid** = Success! Bottle registered
- ⚫ **Gray** = Position occupied

---

### ❌ Loading a Bottle (Incorrect Position)

What happens when a bottle is placed in the wrong position during loading.

#### Scenario:

1. **Scan Barcode** (as above)
2. **System Shows Correct Position**
   - GREEN BLINKING at position 5 (example)

3. **User Places Bottle at Wrong Position**
   - User places bottle at position 3 instead of 5
   - Weight sensor detects unexpected weight change
   - System identifies incorrect placement

4. **Error Feedback**
   - **RED SOLID LED** appears at position 3 (wrong position)
   - **GREEN BLINKING LED** remains at position 5 (correct position)
   - **Web Interface**: Modal displays error message
     - "Incorrect position detected"
     - Shows correct position number
     - Visual diagram of drawer layout

5. **Correction Process**
   - User removes bottle from position 3
   - **RED LED** turns off when bottle removed
   - **GREEN BLINKING LED** continues at position 5
   - User places bottle at correct position 5
   - System confirms and completes registration

6. **Multiple Wrong Positions**
   - If bottle is placed at multiple wrong positions:
   - Each wrong position gets **RED SOLID LED**
   - Correct position maintains **GREEN BLINKING**
   - All wrong LEDs clear when bottles are removed

**Recovery Time**: 10-15 seconds

#### Visual Feedback Summary:
- 🔴 **Red Solid** = Wrong position, remove bottle
- 🟢 **Green Blinking** = Correct position, place here
- Multiple red LEDs possible until all are cleared

---

### 🔄 Unloading a Bottle

Process for removing wines from the fridge.

#### Method 1: From Drawer View (Recommended)

1. **Navigate to Drawer**
   - Open web interface
   - Select drawer from home screen
   - View current inventory with wine details

2. **Select Bottle**
   - Tap/click on bottle image or position number
   - Bottle details displayed (name, type, vintage)
   - "Remove Bottle" button appears

3. **Initiate Unload**
   - Press "Remove Bottle" button
   - **GREEN BLINKING LED** illuminates at bottle position

4. **Remove Bottle**
   - Open drawer
   - Remove bottle from **GREEN BLINKING position**
   - Switch detects removal
   - Weight sensor confirms bottle gone

5. **Confirmation**
   - **GREEN LED** turns **SOLID** briefly then off
   - Web interface updates (position now empty)
   - Success message displayed
   - Inventory count decremented

**Total Time**: 5-8 seconds

#### Method 2: Manual Selection

1. **Access Unload Menu**
   - Click "Unload Bottle" from main menu
   - Select drawer number
   - Select position number manually

2. **LED Guidance**
   - **GREEN BLINKING LED** at selected position

3. **Remove & Confirm** (same as Method 1)

---

### ❌ Unloading Incorrect Position

What happens when removing a bottle from the wrong position.

#### Scenario:

1. **User Initiates Unload**
   - Selects bottle at position 5 in Drawer 3
   - **GREEN BLINKING LED** at position 5

2. **User Removes Wrong Bottle**
   - User accidentally removes bottle from position 3
   - Weight sensor detects unexpected weight change at position 3
   - System identifies incorrect removal

3. **Error Feedback**
   - **RED SOLID LED** at position 3 (wrong removal)
   - **GREEN BLINKING LED** continues at position 5 (correct position)
   - **Web Interface Modal**:
     - "Wrong bottle removed"
     - Shows correct position (5)
     - Warning message

4. **Correction Process Option A: Replace First**
   - User places bottle back at position 3
   - **RED LED** turns off
   - **GREEN BLINKING** remains at position 5
   - User removes correct bottle from position 5
   - Operation completes successfully

5. **Correction Process Option B: Continue Anyway**
   - User removes correct bottle from position 5 while position 3 shows red
   - Both operations register:
     - Position 3: Marked as unexpectedly empty (inventory mismatch flag)
     - Position 5: Successfully removed
   - **RED LED** remains at position 3 until bottle replaced
   - System logs discrepancy for inventory audit

**Recovery Time**: 15-30 seconds

#### Visual Feedback Summary:
- 🟢 **Green Blinking** = Correct bottle to remove
- 🔴 **Red Solid** = Wrong bottle removed, replace to continue
- Both can be active simultaneously

---

### 🔀 Swapping Bottles

Process for exchanging positions of two bottles.

#### Steps:

1. **Initiate Swap Mode**
   - Access "Swap Bottle" from main menu
   - No need to select drawer or position
   - System enters swap mode

2. **Remove First Bottle**
   - Remove any bottle from any position
   - Position illuminates with **YELLOW SOLID LED**
   - Bottle information displayed on screen

3. **Remove Second Bottle**
   - Remove another bottle from any position
   - Second bottle information displayed on screen
   - Second position begins **GREEN BLINKING**

4. **Place First Bottle at Second Position**
   - Place the first bottle at the **GREEN BLINKING position**
   - System weighs bottle and confirms
   - **GREEN LED** turns **SOLID**

5. **Complete the Swap**
   - First position (yellow) now begins **GREEN BLINKING**
   - Place the second bottle at this **GREEN BLINKING position**
   - System weighs and confirms
   - **GREEN LED** turns **SOLID**

6. **Confirmation**
   - All LEDs turn off
   - Success message displayed on screen
   - Inventory updated with swapped locations

**Total Time**: 15-20 seconds

#### Use Cases:
- **Consolidation**: Group bottles by type/vintage
- **Organization**: Rearrange bottles within drawer
- **Temperature adjustment**: Move between zones
- **Access improvement**: Move bottles to front positions

---

### 💡 General Light Controls

Control zone and drawer lighting independently.

#### Zone Lighting Control

Each zone has dual-color COB lighting (warm + cool) controlled through the web interface.

**Access Zone Controls**:
1. Navigate to "Lighting" from main menu
2. Select zone: Upper (Zone 1), Middle (Zone 2), or Lower (Zone 3)

**Brightness Control**:
- Slider: 0-100%
- Real-time adjustment
- Changes apply immediately to COB strips

**Color Temperature Control**:
- Range: 2700K (warm) to 6500K (cool)
- Slider adjusts mix of warm/cool LEDs
- **2700K**: Full warm (orange/yellow tone)
- **4600K**: Balanced (neutral white)
- **6500K**: Full cool (blue/white tone)

#### Drawer Lighting Control

Each drawer has independent COB lighting plus position LEDs.

**Drawer General Lighting**:
- Same controls as zones (brightness + color temp)
- Access via individual drawer view
- Synchronized with position LEDs

**Position LED Control** (Functional Drawers Only):
- Automatically controlled during operations
- Manual control available in advanced settings:
  - Set individual position colors
  - Adjust brightness per position
  - Create custom patterns

**Display Drawer Lighting** (Drawers 1, 4, 9):
- Controlled by lighting controller
- Synchronized with adjacent functional drawer
- Or set to independent mode

#### All Lights Control:

**Master On/Off**:
- Toggle switch in lighting menu
- Turns all zone + drawer lights on/off simultaneously
- Preserves individual settings

---

### 🔌 System Shutdown Procedure

Safe shutdown procedure to preserve data and hardware.

#### Standard Shutdown (Recommended):

**From Web Interface**:
1. Click "LOG OUT" button in the interface
2. Confirmation dialog appears - click "Confirm Logout"
3. After logout, click "LOGIN WITH GOOGLE ACCOUNT" button
4. System shutdown confirmation window appears
5. Wait 30 seconds for safe shutdown
6. Once Raspberry Pi LED stops blinking, turn off the rear panel switch

**What Happens**:
- Web interface sends shutdown command via MQTT
- Backend saves all data to disk:
  - Inventory state (inventory.json)
  - Operation logs
  - System configuration
- PM2 gracefully stops all services:
  - web-server (closes connections)
  - mqtt-handler (completes pending operations)
  - barcode-scanner (closes serial port)
- MQTT broker stops
- Raspberry Pi shuts down

**Timeline**:
- 0s: Logout confirmed, shutdown initiated
- 2s: Services begin stopping
- 5s: All data saved
- 8s: PM2 services stopped
- 30s: System powered off - **safe to turn off rear panel switch**

#### Emergency Shutdown:

**Method: SSH Manual Shutdown**
```bash
# SSH into Raspberry Pi
ssh plasticlab@192.168.1.84

# Stop all services
pm2 stop all

# Verify stopped
pm2 status

# Shutdown system
sudo shutdown now

# After Raspberry Pi shuts down, turn off rear panel switch
```

#### Restart After Shutdown:

**Cold Start**:
1. Turn on the fridge with the rear panel switch
2. Wait for boot (30-45 seconds)
3. PM2 auto-starts configured services
4. ESP32 controllers reconnect automatically (may take 30s)
5. Web interface accessible after full boot

**Verification**:
```bash
# Check all services running
pm2 status

# Check MQTT connectivity
mosquitto_sub -h 192.168.1.84 -t "winefridge/#" -v

# Should see heartbeat messages from all ESP32s
```

#### Maintenance Shutdown:

**For Hardware Maintenance**:
1. Perform standard shutdown
2. Wait for complete power-off
3. Unplug power supply
4. Perform maintenance
5. Reconnect power
6. System auto-starts

**For Software Updates**:
1. Stop PM2 services only (don't shutdown):
   ```bash
   pm2 stop all
   ```
2. Perform updates
3. Restart services:
   ```bash
   pm2 restart all
   ```

---

## 📡 MQTT Protocol Reference

### Topic Structure

All MQTT communication uses two topics per component:

```
winefridge/{component}/command  → Component receives commands
winefridge/{component}/status   → Component publishes status
```

**Components**:
- `drawer_3`, `drawer_5`, `drawer_7` (functional drawers)
- `lighting_1`, `lighting_2`, `lighting_3` (lighting controllers)
- `system` (Raspberry Pi backend communication)

### Message Format

All messages use standardized JSON format:

```json
{
  "action": "action_name",
  "source": "sender_id",
  "data": {
    // Action-specific data
  },
  "timestamp": "2025-11-21T10:30:00Z"
}
```

### Common Commands

#### LED Control
```bash
# Set specific position LEDs
mosquitto_pub -h 192.168.1.84 -t 'winefridge/drawer_3/command' -m '{
  "action": "set_leds",
  "source": "backend",
  "data": {
    "positions": [
      {"position": 1, "color": "#FF0000", "brightness": 100, "blink": false},
      {"position": 5, "color": "#00FF00", "brightness": 80, "blink": true}
    ]
  },
  "timestamp": "2025-11-21T10:00:00Z"
}'
```

#### COB Lighting Control
```bash
# Set drawer general lighting
mosquitto_pub -h 192.168.1.84 -t 'winefridge/drawer_3/command' -m '{
  "action": "set_general_light",
  "source": "backend",
  "data": {
    "temperature": 4000,
    "brightness": 75
  },
  "timestamp": "2025-11-21T10:00:00Z"
}'
```

#### Sensor Reading Request
```bash
# Request current sensor data
mosquitto_pub -h 192.168.1.84 -t 'winefridge/drawer_3/command' -m '{
  "action": "read_sensors",
  "source": "backend",
  "timestamp": "2025-11-21T10:00:00Z"
}'
```

### Status Messages

#### Heartbeat (Every 60-90 seconds)
```json
{
  "action": "heartbeat",
  "source": "drawer_3",
  "data": {
    "firmware_version": "6.3.0",
    "uptime": 123456,
    "wifi_rssi": -65,
    "free_heap": 234567
  },
  "timestamp": "2025-11-21T10:00:00Z"
}
```

#### Bottle Events
```json
{
  "action": "bottle_placed",
  "source": "drawer_3",
  "data": {
    "position": 5,
    "weight": 1250.5
  },
  "timestamp": "2025-11-21T10:00:00Z"
}

{
  "action": "bottle_removed",
  "source": "drawer_3",
  "data": {
    "position": 3,
    "weight": 0.0
  },
  "timestamp": "2025-11-21T10:00:00Z"
}
```

#### Sensor Data
```json
{
  "action": "sensor_data",
  "source": "drawer_3",
  "data": {
    "weight": 10250.3,
    "temperature": 16.5,
    "humidity": 65.2,
    "positions": [1, 3, 5, 7, 9]
  },
  "timestamp": "2025-11-21T10:00:00Z"
}
```

### Monitoring MQTT Traffic

```bash
# Monitor all winefridge messages
mosquitto_sub -h 192.168.1.84 -t "winefridge/#" -v

# Monitor specific drawer
mosquitto_sub -h 192.168.1.84 -t "winefridge/drawer_3/#" -v

# Monitor system messages
mosquitto_sub -h 192.168.1.84 -t "winefridge/system/#" -v

# Save to log file with timestamps
mosquitto_sub -h 192.168.1.84 -t "winefridge/#" -v | while read line; do 
  echo "$(date '+%Y-%m-%d %H:%M:%S') $line" >> mqtt_monitor.log
done
```

---

## 🛠️ Troubleshooting

### Common Issues

#### MQTT Connection Failed

**Symptoms**: ESP32 connects to WiFi but no MQTT messages

**Solutions**:
```bash
# 1. Verify MQTT broker is running
sudo systemctl status mosquitto

# 2. Check broker is accessible
mosquitto_sub -h 192.168.1.84 -t "test" -v

# 3. Verify broker IP in ESP32 code
# Should match Raspberry Pi IP

# 4. Check firewall rules
sudo ufw status
sudo ufw allow 1883/tcp
sudo ufw allow 9001/tcp

# 5. Restart Mosquitto
sudo systemctl restart mosquitto
```

#### PM2 Services Not Running

**Symptoms**: Web interface not accessible, no backend responses

**Solutions**:
```bash
# 1. Check PM2 status
pm2 status

# 2. View error logs
pm2 logs --err

# 3. Restart all services
pm2 restart all

# 4. If services won't start, delete and recreate
pm2 delete all
cd /home/plasticlab/WineFridge/RPI/backend
pm2 start pm2.config.js
pm2 save

# 5. Check Node.js and Python paths
which node
which python3
# Update pm2.config.js if paths differ
```

#### Weight Sensor Inaccurate

**Symptoms**: Wrong weights reported, drift over time

**Solutions**:
```bash
# 1. Tare the scale (via MQTT command or power cycle)
mosquitto_pub -h 192.168.1.84 -t 'winefridge/drawer_3/command' -m '{
  "action": "tare_scale",
  "source": "maintenance"
}'

# 2. Test with known weight
# Place 1.5kg bottle, verify reading is within ±20g
```

#### LED Strip Not Working

**Symptoms**: No LEDs light up, incorrect colors

**Solutions**:
```bash
# 1. Test with simple command
mosquitto_pub -h 192.168.1.84 -t 'winefridge/drawer_3/command' -m '{
  "action": "set_leds",
  "data": {
    "positions": [{"position": 1, "color": "#FF0000", "brightness": 255}]
  }
}'

# 2. Check power supply
# Each LED needs ~60mA @ 5V
# 17 LEDs = ~1A required

# 3. Verify data pin connection
# Functional drawers: GPIO13
# Check for loose wire or broken solder joint

# 4. Test with different LED position
# If one position works, strip is functional

# 5. Check for reversed polarity
# LED strips have directional data flow
```

#### Web Interface Not Loading

**Symptoms**: Browser shows "Connection refused" or blank page

**Solutions**:
```bash
# 1. Check web server is running
pm2 status web-server

# 2. Test manually
cd /home/plasticlab/WineFridge/RPI/frontend
node server.mjs
# Should show "Server running on port 3000"

# 3. Check port is accessible
sudo netstat -tulpn | grep 3000

# 4. Verify firewall
sudo ufw allow 3000/tcp

# 5. Check frontend build
cd /home/plasticlab/WineFridge/RPI/frontend/WineFridge_web
npm run build
# Should complete without errors
```

#### Barcode Scanner Not Responding

**Symptoms**: No barcode_scanned messages when scanning

**Solutions**:
```bash
# 1. Check scanner service
pm2 status barcode-scanner

# 2. View scanner logs
pm2 logs barcode-scanner

# 3. Verify USB connection
lsusb
# Should show M5Unit device

# 4. Check serial port permissions
ls -l /dev/ttyUSB*
sudo usermod -a -G dialout plasticlab
# Logout and login for group change to take effect

# 5. Test serial communication
python3 -c "import serial; s=serial.Serial('/dev/ttyUSB0', 115200); print(s.read(100))"
# Scan barcode, should see data
```

### System Health Checks

```bash
# Complete system status check
echo "=== PM2 Services ==="
pm2 status

echo -e "\n=== MQTT Broker ==="
sudo systemctl status mosquitto | grep "Active:"

echo -e "\n=== ESP32 Controllers ==="
mosquitto_sub -h 192.168.1.84 -t "winefridge/+/status" -W 5 | grep "heartbeat"

echo -e "\n=== Disk Space ==="
df -h | grep -E "Filesystem|/dev/root"

echo -e "\n=== Memory Usage ==="
free -h

echo -e "\n=== Network ==="
hostname -I
```

### Log Locations

```bash
# PM2 logs
~/.pm2/logs/web-server-out.log
~/.pm2/logs/web-server-error.log
~/.pm2/logs/mqtt-handler-out.log
~/.pm2/logs/mqtt-handler-error.log
~/.pm2/logs/barcode-scanner-out.log
~/.pm2/logs/barcode-scanner-error.log

# Mosquitto logs
/var/log/mosquitto/mosquitto.log

# System logs
journalctl -u mosquitto
journalctl -xe
```

---

## 🔧 Maintenance

### Regular Maintenance Tasks

#### Daily
- Monitor PM2 services status
- Check for any error messages in logs
- Verify all ESP32s sending heartbeats

#### Weekly
- Backup database files:
  ```bash
  cp -r /home/plasticlab/WineFridge/RPI/backend/database ~/backups/db_$(date +%Y%m%d)
  ```
- Review system logs for anomalies
- Test barcode scanner functionality

#### Monthly
- Update system packages:
  ```bash
  sudo apt update && sudo apt upgrade -y
  ```
- Clean PM2 logs:
  ```bash
  pm2 flush
  ```
- Verify weight sensor calibration with known weight

#### Quarterly
- Full system backup
- Review and optimize inventory database
- Check all wire connections and solder joints
- Update ESP32 firmware if new version available

### Database Backup

```bash
# Manual backup
cd /home/plasticlab/WineFridge/RPI/backend
tar -czf ~/backups/winefridge_db_$(date +%Y%m%d_%H%M%S).tar.gz database/

# Automated daily backup (add to crontab)
crontab -e
# Add line:
0 2 * * * tar -czf ~/backups/winefridge_db_$(date +\%Y\%m\%d).tar.gz /home/plasticlab/WineFridge/RPI/backend/database/
```

### Firmware Updates

```bash
# 1. Backup current configuration
cd /home/plasticlab/WineFridge
git stash  # Save any local changes

# 2. Pull latest code
git pull origin main

# 3. Update frontend
cd RPI/frontend
npm install
npm run build

# 4. Restart services
pm2 restart all

# 5. Flash ESP32s if firmware updated
# Use Arduino IDE to upload new firmware to each ESP32
```

### Security Best Practices

```bash
# 1. Enable firewall
sudo ufw enable
sudo ufw allow ssh
sudo ufw allow 3000/tcp  # Web interface
sudo ufw allow 1883/tcp  # MQTT
sudo ufw allow 9001/tcp  # WebSocket

# 2. Regular system updates
sudo apt update && sudo apt upgrade -y

# 3. Monitor SSH access
sudo tail -f /var/log/auth.log
```

---

## 📚 Additional Resources

### Project Repository
- **GitHub**: https://github.com/bareaescobar/WineFridge
- **Branch**: main

---

## 📝 License

This project is proprietary. All rights reserved.

---

## 🙏 Acknowledgments

Developed for professional wine storage and management. Built with modern IoT technologies and best practices for reliability and scalability.

**System Version**: v8.0  
**Firmware Version**: v6.3.0  
**Last Updated**: November 2025  
**Documentation Version**: 2.0
