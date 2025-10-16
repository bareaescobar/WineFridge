#!/bin/bash
# Quick Recovery Script - WineFridge
# Run this to backup current version and restore from GitHub

echo "🔄 Starting WineFridge Recovery Process..."

# ========================================
# STEP 1: CREATE BACKUP
# ========================================
echo ""
echo "📦 Step 1: Creating backup..."
cd ~/winefridge
BACKUP_DIR=~/winefridge/backups/broken-version-$(date +%Y%m%d-%H%M%S)
mkdir -p $BACKUP_DIR

# Backup files
cp RPI/backend/barcode_scanner.py $BACKUP_DIR/barcode_scanner.py.backup
cp RPI/backend/mqtt_handler.py $BACKUP_DIR/mqtt_handler.py.backup
cp RPI/frontend/web/js/load-bottle.js $BACKUP_DIR/load-bottle.js.backup
cp RPI/frontend/web/js/constants/mqtt-variables.js $BACKUP_DIR/mqtt-variables.js.backup
cp RPI/database/inventory.json $BACKUP_DIR/inventory.json.backup
cp RPI/database/wine-catalog.json $BACKUP_DIR/wine-catalog.json.backup
sudo cp /etc/mosquitto/mosquitto.conf $BACKUP_DIR/mosquitto.conf.backup 2>/dev/null

echo "✅ Backup complete: $BACKUP_DIR"
ls -la $BACKUP_DIR

# ========================================
# STEP 2: STOP SERVICES
# ========================================
echo ""
echo "🛑 Step 2: Stopping services..."
cd ~/winefridge/RPI/frontend
pm2 stop all

# ========================================
# STEP 3: RESTORE FROM GITHUB
# ========================================
echo ""
echo "⏮️  Step 3: Restoring files from GitHub..."
cd ~/winefridge

# Show what will be restored
echo "Files to restore:"
git status

# Restore backend files
git checkout RPI/backend/barcode_scanner.py
git checkout RPI/backend/mqtt_handler.py

# Restore frontend files  
git checkout RPI/frontend/web/js/load-bottle.js
git checkout RPI/frontend/web/js/constants/mqtt-variables.js

echo "✅ Files restored from GitHub"

# ========================================
# STEP 4: CHECK DATABASE FILES
# ========================================
echo ""
echo "🔍 Step 4: Checking database files..."

if grep -q "8420209033869" ~/winefridge/RPI/database/wine-catalog.json; then
    echo "✅ Silanus barcode found in wine-catalog.json"
else
    echo "⚠️  Silanus barcode NOT found in wine-catalog.json"
    echo "Restoring from backup..."
    cp $BACKUP_DIR/wine-catalog.json.backup ~/winefridge/RPI/database/wine-catalog.json
fi

# ========================================
# STEP 5: REBUILD FRONTEND
# ========================================
echo ""
echo "🔨 Step 5: Rebuilding frontend..."
cd ~/winefridge/RPI/frontend
npm run build

# ========================================
# STEP 6: FIX MOSQUITTO CONFIG
# ========================================
echo ""
echo "📡 Step 6: Fixing Mosquitto config..."

# Backup current broken config
sudo cp /etc/mosquitto/mosquitto.conf /etc/mosquitto/mosquitto.conf.broken 2>/dev/null

# Create clean config
sudo tee /etc/mosquitto/mosquitto.conf > /dev/null << 'EOF'
# Mosquitto Configuration for WineFridge

listener 1883
protocol mqtt

listener 9001
protocol websockets

allow_anonymous true

persistence true
persistence_location /var/lib/mosquitto/

log_dest file /var/log/mosquitto/mosquitto.log
log_type error
log_type warning
log_type notice
log_type information
EOF

# Create log directory
sudo mkdir -p /var/log/mosquitto
sudo chown mosquitto:mosquitto /var/log/mosquitto

# Restart mosquitto
echo "Restarting Mosquitto..."
sudo systemctl restart mosquitto
sleep 2

# ========================================
# STEP 7: RESTART SERVICES
# ========================================
echo ""
echo "🚀 Step 7: Restarting services..."
cd ~/winefridge/RPI/frontend
pm2 restart all
sleep 3

# ========================================
# STEP 8: VERIFY SERVICES
# ========================================
echo ""
echo "✅ Step 8: Verifying services..."
echo ""
echo "PM2 Status:"
pm2 status

echo ""
echo "Mosquitto Status:"
sudo systemctl status mosquitto --no-pager -l | head -10

echo ""
echo "mqtt-handler logs (last 10 lines):"
pm2 logs mqtt-handler --lines 10 --nostream

# ========================================
# STEP 9: TEST CONNECTIONS
# ========================================
echo ""
echo "🔍 Step 9: Testing connections..."

# Test MQTT
echo ""
echo "Testing MQTT port 1883..."
timeout 2 mosquitto_sub -h 192.168.1.84 -t "test" -C 1 > /dev/null 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then
    echo "✅ MQTT port 1883 working"
else
    echo "❌ MQTT port 1883 not responding"
fi

# Test WebSocket
echo ""
echo "Testing WebSocket port 9001..."
response=$(curl -s -o /dev/null -w "%{http_code}" \
  -H "Connection: Upgrade" -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: test" \
  http://192.168.1.84:9001 2>/dev/null)

if [ "$response" = "101" ]; then
    echo "✅ WebSocket port 9001 working"
else
    echo "⚠️  WebSocket port 9001 response: $response (expected 101)"
fi

# ========================================
# DONE
# ========================================
echo ""
echo "======================================"
echo "✅ RECOVERY COMPLETE"
echo "======================================"
echo ""
echo "📋 Next Steps:"
echo "1. Monitor MQTT traffic:"
echo "   mosquitto_sub -h 192.168.1.84 -t \"winefridge/#\" -v"
echo ""
echo "2. Test barcode scan:"
echo "   mosquitto_pub -h 192.168.1.84 -t \"winefridge/system/status\" -m '{\"action\":\"barcode_scanned\",\"source\":\"barcode_scanner\",\"data\":{\"barcode\":\"8420209033869\"},\"timestamp\":\"2025-10-16T12:00:00Z\"}'"
echo ""
echo "3. Check logs:"
echo "   pm2 logs mqtt-handler"
echo ""
echo "4. Open web interface:"
echo "   http://192.168.1.84:3000/load-bottle"
echo ""
echo "📁 Backup saved in: $BACKUP_DIR"
