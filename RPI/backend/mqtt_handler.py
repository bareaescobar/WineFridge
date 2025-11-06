#!/usr/bin/env python3
"""
WineFridge MQTT Handler
Version: 3.2.0
Date: 06.11.2025

FIXES:
1. Manual unload now properly marks position
2. Unload incorrect error handling added
3. Swap correct position LEDs turn gray after completion
4. Swap incorrect LED behavior fixed
5. Zone lighting control implemented
6. General improvements and bug fixes
"""

import json
import paho.mqtt.client as mqtt
import serial
import time
from datetime import datetime
import threading
import re

# Define functional drawers with sensors
FUNCTIONAL_DRAWERS = ['drawer_3', 'drawer_5', 'drawer_7']

# Wine type to drawer mapping
WINE_TYPE_DRAWERS = {
    'rose': 'drawer_3',
    'white': 'drawer_5',
    'red': 'drawer_7'
}

# Zone to lighting controller mapping
ZONE_LIGHTING_CONTROLLERS = {
    'lower': 'lighting_2',
    'middle': 'lighting_6', 
    'upper': 'lighting_8'
}

# Weight thresholds for bottle percentage
MIN_FULL_BOTTLE_WEIGHT = 700
MAX_FULL_BOTTLE_WEIGHT = 2000
EMPTY_BOTTLE_WEIGHT = 300

def find_serial_port():
    """Detect serial port automatically on RPI5"""
    import os
    possible_ports = ['/dev/ttyAMA0', '/dev/serial0', '/dev/ttyAMA1']
    
    for port in possible_ports:
        if os.path.exists(port):
            try:
                ser = serial.Serial(port, 115200, timeout=0.1)
                ser.close()
                return port
            except:
                continue
    return '/dev/ttyAMA0'

class WineFridgeController:
    def __init__(self):
        print("[INIT] Wine Fridge Controller v3.2.0")
        
        # Load databases
        self.inventory = self.load_json('/home/plasticlab/WineFridge/RPI/database/inventory.json')
        self.catalog = self.load_json('/home/plasticlab/WineFridge/RPI/database/wine-catalog.json')
        
        # Track pending operations
        self.pending_operations = {}
        
        # Track swap operations
        self.swap_operations = {
            'active': False,
            'bottles_removed': [],
            'bottles_to_place': [],
            'start_time': None
        }
        
        # Barcode scanner state
        self.barcode_buffer = ""
        self.last_barcode = ""
        self.last_scan_time = 0
        self.scan_cooldown = 2.0
        
        # Setup serial port for barcode scanner
        try:
            port = find_serial_port()
            self.serial = serial.Serial(
                port=port,
                baudrate=115200,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=0.1,
                xonxoff=False,
                rtscts=False,
                dsrdtr=False
            )
            print(f"[SCANNER] ✔ Connected to {port}")
        except Exception as e:
            print(f"[SCANNER] ✗ Error: {e}")
            self.serial = None
            
        # Setup MQTT
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect
        
        print("[MQTT] Connecting to broker...")
        self.client.connect("localhost", 1883, 60)
        
        # Start barcode scanner thread
        self.running = True
        if self.serial:
            self.scanner_thread = threading.Thread(target=self.run_scanner, daemon=True)
            self.scanner_thread.start()

    def load_json(self, filepath):
        try:
            with open(filepath, 'r') as f:
                return json.load(f)
        except Exception as e:
            print(f"[ERROR] Loading {filepath}: {e}")
            return {}

    def save_json(self, filepath, data):
        try:
            with open(filepath, 'w') as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            print(f"[ERROR] Saving {filepath}: {e}")

    def is_valid_barcode(self, code):
        """Verify if a barcode seems valid"""
        code = code.strip()
        if len(code) < 8 or len(code) > 20:
            return False
        if not re.match(r'^[A-Za-z0-9]+$', code):
            return False
        return True

    def calculate_bottle_percentage(self, weight):
        """Calculate bottle fill percentage based on weight"""
        if weight < EMPTY_BOTTLE_WEIGHT:
            return 0
            
        if weight >= MIN_FULL_BOTTLE_WEIGHT:
            return 100
            
        wine_weight = weight - EMPTY_BOTTLE_WEIGHT
        full_wine_weight = MIN_FULL_BOTTLE_WEIGHT - EMPTY_BOTTLE_WEIGHT
        percentage = (wine_weight / full_wine_weight) * 100
        
        return max(0, min(100, int(percentage)))

    def get_wine_type(self, barcode):
        """Get wine type from catalog"""
        if barcode in self.catalog.get('wines', {}):
            wine_data = self.catalog['wines'][barcode]
            wine_type = wine_data.get('type', '').lower()
            if 'rose' in wine_type or 'rosé' in wine_type or 'rosado' in wine_type:
                return 'rose'
            elif 'white' in wine_type or 'blanco' in wine_type:
                return 'white'
            elif 'red' in wine_type or 'tinto' in wine_type:
                return 'red'
        return None

    def run_scanner(self):
        """Barcode scanner loop running in background thread"""
        print("[SCANNER] Ready and listening...")
        last_data_time = 0
        
        while self.running:
            try:
                if self.serial.in_waiting > 0:
                    data = self.serial.read(self.serial.in_waiting)
                    last_data_time = time.time()
                    
                    decoded = data.decode('utf-8', errors='ignore')
                    self.barcode_buffer += decoded
                    
                    cleaned = self.barcode_buffer.strip()
                    if self.is_valid_barcode(cleaned):
                        current_time = time.time()
                        
                        if (cleaned != self.last_barcode or 
                            current_time - self.last_scan_time > self.scan_cooldown):
                            
                            print(f"[SCANNER] ✔ Detected: {cleaned}")
                            self.publish_barcode(cleaned)
                            
                            self.last_barcode = cleaned
                            self.last_scan_time = current_time
                            
                        self.barcode_buffer = ""
                        
                else:
                    current_time = time.time()
                    if (self.barcode_buffer and 
                        current_time - last_data_time > 0.5 and
                        self.is_valid_barcode(self.barcode_buffer.strip())):
                        
                        cleaned = self.barcode_buffer.strip()
                        if (cleaned != self.last_barcode or
                            current_time - self.last_scan_time > self.scan_cooldown):
                            
                            print(f"[SCANNER] ✔ Detected (timeout): {cleaned}")
                            self.publish_barcode(cleaned)
                            
                            self.last_barcode = cleaned
                            self.last_scan_time = current_time
                            
                        self.barcode_buffer = ""
                        
                time.sleep(0.05)
                
            except Exception as e:
                print(f"[SCANNER] Error: {e}")
                time.sleep(1)

    def publish_barcode(self, barcode):
        """Publish barcode to MQTT"""
        message = {
            "action": "barcode_scanned",
            "source": "barcode_scanner",
            "data": {"barcode": barcode},
            "timestamp": datetime.now().isoformat()
        }
        self.client.publish("winefridge/system/status", json.dumps(message))
        print(f"[SCANNER] → Published to MQTT")

    def on_connect(self, client, userdata, flags, rc, properties):
        if rc == 0:
            print(f"[MQTT] ✔ Connected")
        else:
            print(f"[MQTT] ✗ Connection failed: {rc}")
            
        client.subscribe("winefridge/+/status")
        client.subscribe("winefridge/system/command")
        client.subscribe("winefridge/system/status")
        print("[MQTT] ✔ Subscribed to topics")

    def on_disconnect(self, client, userdata, flags, rc, properties):
        if rc != 0:
            print(f"[MQTT] ✗ Disconnected (rc={rc}), reconnecting...")

    def on_message(self, client, userdata, msg):
        try:
            message = json.loads(msg.payload.decode())
            action = message.get('action')
            source = message.get('source', 'unknown')
            
            if action != 'heartbeat':
                print(f"[MQTT] ← {action} from {source}")
                
            if msg.topic == 'winefridge/system/command':
                self.handle_system_command(message)
                
            elif msg.topic == 'winefridge/system/status':
                if action == 'barcode_scanned' and source == 'barcode_scanner':
                    self.handle_barcode_scanned(message.get('data', {}))
                    
            elif '/status' in msg.topic:
                drawer_id = msg.topic.split('/')[1]
                if action == 'bottle_event':
                    self.handle_drawer_status(drawer_id, message)
                elif action == 'wrong_placement':
                    self.handle_wrong_placement(drawer_id, message)
                    
        except Exception as e:
            print(f"[ERROR] Processing message: {e}")

    def handle_barcode_scanned(self, data):
        """Process scanned barcode from physical scanner"""
        barcode = data.get('barcode', '')
        print(f"[BARCODE] Processing: {barcode}")
        
        if barcode not in self.catalog.get('wines', {}):
            print(f"[BARCODE] ✗ Not found in catalog")
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "scan_error",
                "source": "mqtt_handler",
                "data": {
                    "error": "Wine not found in catalog",
                    "barcode": barcode
                },
                "timestamp": datetime.now().isoformat()
            }))
            return
            
        wine = self.catalog['wines'][barcode]
        wine_name = wine.get('name', 'Unknown Wine')
        wine_type = self.get_wine_type(barcode)
        
        print(f"[BARCODE] ✔ Found: {wine_name}")
        if wine_type:
            print(f"[BARCODE]   Type: {wine_type}")
            
        print(f"[BARCODE] Ready - waiting for frontend to call start_load")

    def handle_system_command(self, message):
        action = message.get('action')
        data = message.get('data', {})
        
        if action == 'start_load':
            self.start_bottle_load(data)
        elif action == 'start_unload':
            self.start_bottle_unload(data)
        elif action == 'start_swap':
            self.start_swap_bottles()
        elif action == 'cancel_swap':
            self.cancel_swap()
        elif action == 'retry_placement':
            self.retry_placement(data)
        elif action == 'load_complete':
            self.complete_load_operation(data)
        elif action == 'set_brightness':
            self.handle_zone_lighting(data)
        elif action == 'shutdown':
            self.handle_shutdown()

    # FIX #5: Zone lighting control implementation
    def handle_zone_lighting(self, data):
        """Handle zone lighting brightness and color temperature changes"""
        zone = data.get('zone')
        brightness = data.get('value', '0')
        color_temp = data.get('color_temp', 'neutral_white')
        
        if zone not in ZONE_LIGHTING_CONTROLLERS:
            print(f"[LIGHTING] Unknown zone: {zone}")
            return
            
        # Convert color_temp name to Kelvin value
        temp_map = {
            'warm_white': 2700,
            'neutral_white': 4000,
            'cool_white': 6500
        }
        temperature = temp_map.get(color_temp, 4000)
        
        # Zone controller mapping:
        # lower zone (drawers 1,2,3) -> lighting_2
        # middle zone (drawers 4,5,6) -> lighting_6  
        # upper zone (drawers 7,8,9) -> lighting_8
        controller = ZONE_LIGHTING_CONTROLLERS[zone]
        
        print(f"[LIGHTING] Setting {zone} zone via {controller}: brightness={brightness}%, temp={temperature}K")
        
        # Convert percentage to 0-255 scale for PWM
        brightness_pwm = int(float(brightness) * 2.55)
        
        cmd = {
            "action": "set_zone_light",
            "source": "mqtt_handler",
            "data": {
                "temperature": temperature,
                "brightness": brightness_pwm
            },
            "timestamp": datetime.now().isoformat()
        }
        self.client.publish(f"winefridge/{controller}/command", json.dumps(cmd))
        print(f"[LIGHTING] → Command sent to {controller}")

    def handle_shutdown(self):
        """Shutdown RPI safely"""
        print("[SHUTDOWN] Received shutdown command from web")
        print("[SHUTDOWN] Closing connections...")
        
        shutdown_msg = {
            "action": "system_shutdown",
            "source": "mqtt_handler",
            "timestamp": datetime.now().isoformat()
        }
        
        for drawer in ['drawer_3', 'drawer_5', 'drawer_7', 'lighting_2', 'lighting_6', 'lighting_8']:
            self.client.publish(f"winefridge/{drawer}/command", json.dumps(shutdown_msg))
            
        print("[SHUTDOWN] Executing system shutdown in 3 seconds...")
        time.sleep(3)
        
        import subprocess
        subprocess.run(['sudo', 'shutdown', 'now'])

    def find_empty_position(self, preferred_drawer=None):
        """Find empty position, preferring specified drawer"""
        if preferred_drawer and preferred_drawer in FUNCTIONAL_DRAWERS:
            positions = self.inventory.get("drawers", {}).get(preferred_drawer, {}).get("positions", {})
            for pos in range(1, 10):
                if str(pos) not in positions or not positions[str(pos)].get("occupied", False):
                    return preferred_drawer, pos
                    
        for drawer_id in FUNCTIONAL_DRAWERS:
            if drawer_id == preferred_drawer:
                continue
            positions = self.inventory.get("drawers", {}).get(drawer_id, {}).get("positions", {})
            for pos in range(1, 10):
                if str(pos) not in positions or not positions[str(pos)].get("occupied", False):
                    return drawer_id, pos
                    
        return None, None

    def start_bottle_load(self, data):
        barcode = data.get('barcode', 'unknown')
        name = data.get('name', 'Unknown Wine')
        print(f"\n[LOAD] ═══════════════════════════════")
        print(f"[LOAD] Starting: {name[:40]}")
        
        wine_type = self.get_wine_type(barcode)
        preferred_drawer = WINE_TYPE_DRAWERS.get(wine_type) if wine_type else None
        
        if wine_type:
            print(f"[LOAD] Type: {wine_type} → Target: {preferred_drawer}")
            
        drawer_id, position = self.find_empty_position(preferred_drawer)
        
        if not drawer_id or not position:
            print("[LOAD] ✗ No empty positions available")
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "load_error",
                "source": "mqtt_handler",
                "data": {"error": "No empty positions available"},
                "timestamp": datetime.now().isoformat()
            }))
            return
            
        print(f"[LOAD] Position: {drawer_id} slot #{position}")
        
        op_id = f"load_{int(time.time())}"
        self.pending_operations[op_id] = {
            'type': 'load',
            'barcode': barcode,
            'name': name,
            'drawer': drawer_id,
            'position': position,
            'expected_position': position,
            'timestamp': time.time()
        }
        
        print(f"[LOAD] → LED: Green blinking at position {position}")
        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {"positions": [{"position": position, "color": "#00FF00", "brightness": 100, "blink": True}]},
            "timestamp": datetime.now().isoformat()
        }))
        
        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "expect_bottle",
            "source": "mqtt_handler",
            "data": {"position": position},
            "timestamp": datetime.now().isoformat()
        }))
        
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "expect_bottle",
            "source": "mqtt_handler",
            "data": {"drawer": drawer_id, "position": position, "wine_name": name},
            "timestamp": datetime.now().isoformat()
        }))
        
        timer = threading.Timer(60, self.handle_timeout, [op_id])
        timer.start()
        self.pending_operations[op_id]['timer'] = timer
        print(f"[LOAD] ⏱ Timeout timer started (60s)")
        print(f"[LOAD] ═══════════════════════════════\n")

    # FIX #1: Manual unload now properly finds and marks position
    def start_bottle_unload(self, data):
        barcode = data.get('barcode')
        name = data.get('name', 'Unknown Wine')
        print(f"\n[UNLOAD] ═══════════════════════════════")
        print(f"[UNLOAD] Starting: {name[:40]}")
        
        # Try to find bottle location
        bottle_location = None
        
        # Check if manual selection with position provided
        if data.get('position') and data.get('drawer_id'):
            drawer_id = data.get('drawer_id')
            position = data.get('position')
            bottle_location = (drawer_id, position)
            print(f"[UNLOAD] Manual selection: {drawer_id} position {position}")
        # Otherwise use barcode to find bottle
        elif barcode:
            # Check specific drawer if provided
            if data.get('drawer'):
                bottle_location = self.find_bottle_in_drawer(barcode, data.get('drawer'))
                if not bottle_location:
                    print(f"[UNLOAD] ✗ Not found in {data.get('drawer')}")
                    # FIX #2: Send proper error for UI modal
                    self.client.publish("winefridge/system/status", json.dumps({
                        "action": "unload_incorrect",
                        "source": "mqtt_handler",
                        "data": {
                            "error": f"Bottle not found in drawer {data.get('drawer')}",
                            "show_error_modal": True
                        },
                        "timestamp": datetime.now().isoformat()
                    }))
                    return
            # Search all drawers
            else:
                bottle_location = self.find_bottle_in_inventory(barcode)
                if not bottle_location:
                    print(f"[UNLOAD] ✗ Not found in inventory")
                    # FIX #2: Send proper error for UI modal
                    self.client.publish("winefridge/system/status", json.dumps({
                        "action": "unload_incorrect",
                        "source": "mqtt_handler",
                        "data": {
                            "error": "Bottle not found in inventory",
                            "show_error_modal": True
                        },
                        "timestamp": datetime.now().isoformat()
                    }))
                    return
        else:
            print(f"[UNLOAD] ✗ Missing identification")
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "unload_error",
                "source": "mqtt_handler",
                "data": {"error": "Missing bottle identification"},
                "timestamp": datetime.now().isoformat()
            }))
            return
            
        drawer_id, position = bottle_location
        print(f"[UNLOAD] Position: {drawer_id} slot #{position}")
        
        if drawer_id not in FUNCTIONAL_DRAWERS:
            print(f"[UNLOAD] ✗ {drawer_id} has no sensors")
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "unload_error",
                "source": "mqtt_handler",
                "data": {"error": f"Drawer {drawer_id} has no weight sensors"},
                "timestamp": datetime.now().isoformat()
            }))
            return
            
        op_id = f"unload_{int(time.time())}"
        self.pending_operations[op_id] = {
            'type': 'unload',
            'barcode': barcode if barcode else 'manual',
            'name': name,
            'drawer': drawer_id,
            'position': position,
            'expected_position': position,
            'timestamp': time.time()
        }
        
        print(f"[UNLOAD] → LED: Green blinking at position {position}")
        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {"positions": [{"position": position, "color": "#00FF00", "brightness": 100, "blink": True}]},
            "timestamp": datetime.now().isoformat()
        }))
        
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "expect_removal",
            "source": "mqtt_handler",
            "data": {"drawer": drawer_id, "position": position, "wine_name": name},
            "timestamp": datetime.now().isoformat()
        }))
        
        timer = threading.Timer(60, self.handle_timeout, [op_id])
        timer.start()
        self.pending_operations[op_id]['timer'] = timer
        print(f"[UNLOAD] ⏱ Timeout timer started (60s)")
        print(f"[UNLOAD] ═══════════════════════════════\n")

    def start_swap_bottles(self):
        print("\n[SWAP] ═══════════════════════════════")
        print("[SWAP] Starting swap operation...")
        print("[SWAP] ═══════════════════════════════")
        self.swap_operations = {
            'active': True,
            'bottles_removed': [],
            'bottles_to_place': [],
            'start_time': time.time()
        }
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "swap_started",
            "source": "mqtt_handler",
            "timestamp": datetime.now().isoformat()
        }))
        print("[SWAP] ═══════════════════════════════\n")

    def cancel_swap(self):
        print("\n[SWAP] Cancelling swap operation...")
        for bottle_info in self.swap_operations.get('bottles_removed', []):
            drawer_id = bottle_info['drawer']
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))
        self.swap_operations = {
            'active': False,
            'bottles_removed': [],
            'bottles_to_place': [],
            'start_time': None
        }
        print("[SWAP] Cancelled\n")

    def find_bottle_in_inventory(self, barcode):
        for drawer_id in FUNCTIONAL_DRAWERS:
            if drawer_id in self.inventory.get("drawers", {}):
                drawer_data = self.inventory["drawers"][drawer_id]
                for position, pos_data in drawer_data.get("positions", {}).items():
                    if pos_data.get("occupied") and pos_data.get("barcode") == barcode:
                        return drawer_id, int(position)
        return None

    def find_bottle_in_drawer(self, barcode, drawer_id):
        if drawer_id in self.inventory.get("drawers", {}):
            for position, pos_data in self.inventory["drawers"][drawer_id].get("positions", {}).items():
                if pos_data.get("occupied") and pos_data.get("barcode") == barcode:
                    return drawer_id, int(position)
        return None

    def handle_drawer_status(self, drawer_id, message):
        data = message.get('data', {})
        event = data.get('event')
        position = data.get('position')
        weight = data.get('weight', 0)
        
        if self.swap_operations.get('active'):
            self.handle_swap_event(drawer_id, position, event, weight)
        elif event == 'placed':
            self.handle_bottle_placed(drawer_id, position, weight)
        elif event == 'removed':
            self.handle_bottle_removed(drawer_id, position)

    def handle_wrong_placement(self, drawer_id, message):
        data = message.get('data', {})
        position = data.get('position')
        expected_position = data.get('expected_position')
        
        print(f"[LOAD] ✗ Wrong placement! Expected #{expected_position}, got #{position}")
        
        for op_id, op in list(self.pending_operations.items()):
            if op.get('type') == 'load' and op.get('drawer') == drawer_id and op.get('position') == expected_position:
                if 'wrong_positions' not in op:
                    op['wrong_positions'] = []
                if position not in op['wrong_positions']:
                    op['wrong_positions'].append(position)
                    
                self.client.publish("winefridge/system/status", json.dumps({
                    "action": "placement_error",
                    "source": "mqtt_handler",
                    "data": {
                        "drawer": drawer_id,
                        "position": position,
                        "expected_position": expected_position
                    },
                    "timestamp": datetime.now().isoformat()
                }))
                break

    # FIX #4: Swap incorrect LED behavior fixed
    def handle_swap_event(self, drawer_id, position, event, weight):
        """Handle events during swap operation with corrected LED feedback"""
        if event == 'removed':
            bottle_info = None
            if drawer_id in self.inventory.get("drawers", {}):
                pos_data = self.inventory["drawers"][drawer_id].get("positions", {}).get(str(position))
                if pos_data and pos_data.get("occupied"):
                    bottle_info = {
                        'drawer': drawer_id,
                        'position': position,
                        'barcode': pos_data.get('barcode'),
                        'name': pos_data.get('name'),
                        'weight': pos_data.get('weight', 0)
                    }
                    
            if bottle_info:
                self.swap_operations['bottles_removed'].append(bottle_info)
                bottle_num = len(self.swap_operations['bottles_removed'])
                print(f"[SWAP] Bottle {bottle_num} removed: {bottle_info['name'][:40]}")
                
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": [{"position": position, "color": "#FFFF00", "brightness": 100}]},
                    "timestamp": datetime.now().isoformat()
                }))
                
                self.client.publish("winefridge/system/status", json.dumps({
                    "action": "bottle_event",
                    "source": "mqtt_handler",
                    "data": {
                        "event": "removed",
                        "drawer": drawer_id,
                        "position": position
                    },
                    "timestamp": datetime.now().isoformat()
                }))
                
                if len(self.swap_operations['bottles_removed']) == 2:
                    print("[SWAP] Both bottles removed, ready for swap placement")
                    bottle1 = self.swap_operations['bottles_removed'][0]
                    bottle2 = self.swap_operations['bottles_removed'][1]
                    
                    self.swap_operations['bottles_to_place'] = [
                        {'bottle': bottle1, 'target_drawer': bottle2['drawer'], 'target_position': bottle2['position']},
                        {'bottle': bottle2, 'target_drawer': bottle1['drawer'], 'target_position': bottle1['position']}
                    ]
                    
                    # FIX: Only show blinking green on first target position
                    # Second position stays yellow until first is placed
                    if bottle1['drawer'] == bottle2['drawer']:
                        self.client.publish(f"winefridge/{bottle1['drawer']}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [
                                {"position": bottle2['position'], "color": "#00FF00", "brightness": 100, "blink": True},
                                {"position": bottle1['position'], "color": "#FFFF00", "brightness": 100}
                            ]},
                            "timestamp": datetime.now().isoformat()
                        }))
                    else:
                        # Only first target blinks
                        self.client.publish(f"winefridge/{bottle2['drawer']}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": bottle2['position'], "color": "#00FF00", "brightness": 100, "blink": True}]},
                            "timestamp": datetime.now().isoformat()
                        }))
                        # Second stays yellow
                        self.client.publish(f"winefridge/{bottle1['drawer']}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": bottle1['position'], "color": "#FFFF00", "brightness": 100}]},
                            "timestamp": datetime.now().isoformat()
                        }))
                        
        elif event == 'placed':
            if len(self.swap_operations['bottles_to_place']) > 0:
                match_found = False
                for i, expected in enumerate(self.swap_operations['bottles_to_place']):
                    if expected['target_drawer'] == drawer_id and expected['target_position'] == position:
                        match_found = True
                        print(f"[SWAP] ✓ Bottle placed correctly in swapped position")
                        
                        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": position, "color": "#0000FF", "brightness": 100, "blink": True}]},
                            "timestamp": datetime.now().isoformat()
                        }))
                        
                        bottle_info = expected['bottle']
                        original_weight = bottle_info.get('weight', weight)
                        fill_percentage = self.calculate_bottle_percentage(original_weight)
                        
                        self.update_inventory(drawer_id, position, bottle_info['barcode'],
                                             bottle_info['name'], original_weight, fill_percentage)
                        
                        self.swap_operations['bottles_to_place'].pop(i)
                        
                        # Temporary green confirmation
                        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": position, "color": "#00FF00", "brightness": 100}]},
                            "timestamp": datetime.now().isoformat()
                        }))
                        
                        self.client.publish("winefridge/system/status", json.dumps({
                            "action": "bottle_event",
                            "source": "mqtt_handler",
                            "data": {
                                "event": "placed",
                                "drawer": drawer_id,
                                "position": position
                            },
                            "timestamp": datetime.now().isoformat()
                        }))
                        
                        if len(self.swap_operations['bottles_to_place']) == 1:
                            remaining = self.swap_operations['bottles_to_place'][0]
                            target_drawer = remaining['target_drawer']
                            target_position = remaining['target_position']
                            print(f"[SWAP] First done, place second at {target_drawer} #{target_position}")
                            
                            # Now blink the second position
                            self.client.publish(f"winefridge/{target_drawer}/command", json.dumps({
                                "action": "set_leds",
                                "source": "mqtt_handler",
                                "data": {"positions": [{"position": target_position, "color": "#00FF00", "brightness": 100, "blink": True}]},
                                "timestamp": datetime.now().isoformat()
                            }))
                            
                        elif len(self.swap_operations['bottles_to_place']) == 0:
                            print("[SWAP] ✓✓ Swap completed successfully!")
                            
                            # FIX #3: Turn all LEDs to gray after successful swap
                            drawers_to_clear = set()
                            for bottle in self.swap_operations['bottles_removed']:
                                drawers_to_clear.add((bottle['drawer'], bottle['position']))
                                
                            for drawer, pos in drawers_to_clear:
                                self.client.publish(f"winefridge/{drawer}/command", json.dumps({
                                    "action": "set_leds",
                                    "source": "mqtt_handler",
                                    "data": {"positions": [{"position": pos, "color": "#444444", "brightness": 50}]},
                                    "timestamp": datetime.now().isoformat()
                                }))
                            
                            self.client.publish("winefridge/system/status", json.dumps({
                                "action": "swap_completed",
                                "source": "mqtt_handler",
                                "data": {"success": True},
                                "timestamp": datetime.now().isoformat()
                            }))
                            
                            self.swap_operations = {
                                'active': False,
                                'bottles_removed': [],
                                'bottles_to_place': [],
                                'start_time': None
                            }
                        break
                        
                if not match_found:
                    print(f"[SWAP] ✗ Wrong placement at {drawer_id} #{position}")
                    
                    led_positions = [{"position": position, "color": "#FF0000", "brightness": 100}]
                    
                    # Only show expected position if it's in this drawer
                    for expected in self.swap_operations['bottles_to_place']:
                        if expected['target_drawer'] == drawer_id:
                            # Only blink if it's the next expected position
                            if self.swap_operations['bottles_to_place'].index(expected) == 0:
                                led_positions.append({
                                    "position": expected['target_position'],
                                    "color": "#00FF00",
                                    "brightness": 100,
                                    "blink": True
                                })
                                
                    self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                        "action": "set_leds",
                        "source": "mqtt_handler",
                        "data": {"positions": led_positions},
                        "timestamp": datetime.now().isoformat()
                    }))
                    
                    expected_positions = [e['target_position'] for e in self.swap_operations['bottles_to_place'] if e['target_drawer'] == drawer_id]
                    self.client.publish("winefridge/system/status", json.dumps({
                        "action": "swap_error",
                        "source": "mqtt_handler",
                        "data": {
                            "error": "wrong_swap_position",
                            "drawer": drawer_id,
                            "wrong_position": position,
                            "expected_positions": expected_positions
                        },
                        "timestamp": datetime.now().isoformat()
                    }))

    def handle_bottle_placed(self, drawer_id, position, weight):
        fill_percentage = self.calculate_bottle_percentage(weight)
        
        found_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op['drawer'] == drawer_id and op['type'] == 'load':
                if op['expected_position'] == position:
                    found_op = (op_id, op, True)
                else:
                    found_op = (op_id, op, False)
                break
                
        if not found_op:
            return
            
        op_id, op, correct_placement = found_op
        
        if not correct_placement:
            print(f"[LOAD] ✗ Wrong position! Expected #{op['expected_position']}, got #{position}")
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [
                    {"position": position, "color": "#FF0000", "brightness": 100, "blink": False},
                    {"position": op['expected_position'], "color": "#00FF00", "brightness": 100, "blink": True}
                ]},
                "timestamp": datetime.now().isoformat()
            }))
            
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "placement_error",
                "source": "mqtt_handler",
                "data": {
                    "error": "placement_wrong_spot",
                    "drawer": drawer_id,
                    "wrong_position": position,
                    "correct_position": op['expected_position'],
                    "wine_name": op['name']
                },
                "timestamp": datetime.now().isoformat()
            }))
            
            if 'timer' in op:
                op['timer'].cancel()
            new_timer = threading.Timer(60, self.handle_timeout, [op_id])
            new_timer.start()
            op['timer'] = new_timer
            op['wrong_position'] = position
            
        else:
            print(f"[LOAD] ✓ Placed at {drawer_id} #{position}")
            print(f"[LOAD]   Weight: {weight}g ({fill_percentage}% full)")
            
            # Update inventory
            self.update_inventory(drawer_id, position, op['barcode'], op['name'], weight, fill_percentage)
            
            # Change LED to GRAY immediately after weighing
            print(f"[LOAD] → LED: Changing to gray at position {position}")
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [{"position": position, "color": "#444444", "brightness": 50, "blink": False}]},
                "timestamp": datetime.now().isoformat()
            }))
            
            # Reset expected_position on ESP32
            print(f"[LOAD] → Resetting expected_position to 0")
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "expect_bottle",
                "source": "mqtt_handler",
                "data": {"position": 0},
                "timestamp": datetime.now().isoformat()
            }))
            
            # Notify frontend success
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "bottle_placed",
                "source": "mqtt_handler",
                "data": {
                    "success": True,
                    "drawer": drawer_id,
                    "position": position,
                    "barcode": op['barcode'],
                    "wine_name": op['name'],
                    "weight": weight,
                    "fill_percentage": fill_percentage
                },
                "timestamp": datetime.now().isoformat()
            }))
            
            # Cancel timer and clean up operation immediately
            if 'timer' in op:
                op['timer'].cancel()
                
            del self.pending_operations[op_id]
            print(f"[LOAD] ✓✓ Operation complete - ready for next scan")
            print(f"[LOAD] ═══════════════════════════════\n")
            
            # Reset scanner cooldown
            self.last_barcode = ""
            self.last_scan_time = 0

    def handle_bottle_removed(self, drawer_id, position):
        # Check wrong placement recovery
        wrong_placement_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op.get('wrong_position') == position and op['drawer'] == drawer_id and op['type'] == 'load':
                wrong_placement_op = (op_id, op)
                break
                
        if wrong_placement_op:
            op_id, op = wrong_placement_op
            print(f"[LOAD] ✓ Wrong bottle removed from #{position}")
            
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))
            
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [{"position": op['expected_position'], "color": "#00FF00", "brightness": 100, "blink": True}]},
                "timestamp": datetime.now().isoformat()
            }))
            
            if 'wrong_position' in op:
                del op['wrong_position']
                
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "wrong_bottle_removed",
                "source": "mqtt_handler",
                "data": {
                    "drawer": drawer_id,
                    "expected_position": op['expected_position']
                },
                "timestamp": datetime.now().isoformat()
            }))
            return
            
        # Check UNLOAD operations
        found_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op['drawer'] == drawer_id and op['type'] == 'unload':
                if op['expected_position'] == position:
                    found_op = (op_id, op, True)
                else:
                    found_op = (op_id, op, False)
                break
                
        if not found_op:
            return
            
        op_id, op, correct_removal = found_op
        
        if not correct_removal:
            print(f"[UNLOAD] ✗ Wrong bottle! Expected #{op['expected_position']}, got #{position}")
            
            # FIX #2: Send unload_incorrect for UI modal
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "unload_incorrect",
                "source": "mqtt_handler",
                "data": {
                    "error": "wrong_bottle_removed",
                    "drawer": drawer_id,
                    "wrong_position": position,
                    "correct_position": op['expected_position'],
                    "wine_name": op['name'],
                    "show_error_modal": True
                },
                "timestamp": datetime.now().isoformat()
            }))
            
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [
                    {"position": position, "color": "#FF0000", "brightness": 100},
                    {"position": op['expected_position'], "color": "#00FF00", "brightness": 100}
                ]},
                "timestamp": datetime.now().isoformat()
            }))
            
            op['wrong_removal_position'] = position
            return
            
        print(f"[UNLOAD] ✓ Removed from {drawer_id} #{position}")
        
        self.clear_inventory_position(drawer_id, position)
        
        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {"positions": []},
            "timestamp": datetime.now().isoformat()
        }))
        
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "bottle_unloaded",
            "source": "mqtt_handler",
            "data": {
                "success": True,
                "drawer": drawer_id,
                "position": position,
                "barcode": op['barcode'],
                "wine_name": op['name']
            },
            "timestamp": datetime.now().isoformat()
        }))
        
        if 'timer' in op:
            op['timer'].cancel()
            
        self.last_barcode = ""
        self.last_scan_time = 0
        
        del self.pending_operations[op_id]
        print(f"[UNLOAD] ✓✓ Operation complete")
        print(f"[UNLOAD] ═══════════════════════════════\n")

    def retry_placement(self, data):
        print("[RETRY] User requested retry...")
        for op_id in list(self.pending_operations.keys()):
            op = self.pending_operations[op_id]
            if 'wrong_position' in op:
                if 'timer' in op:
                    op['timer'].cancel()
                self.client.publish(f"winefridge/{op['drawer']}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": []},
                    "timestamp": datetime.now().isoformat()
                }))
                del self.pending_operations[op_id]
                break
                
        self.last_barcode = ""
        self.last_scan_time = 0
        
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "retry_ready",
            "source": "mqtt_handler",
            "timestamp": datetime.now().isoformat()
        }))

    def complete_load_operation(self, data):
        """Legacy function - now handled automatically in handle_bottle_placed"""
        print("[LOAD] Complete command received (legacy - already handled automatically)")

    def clear_inventory_position(self, drawer_id, position):
        self.inventory = self.load_json('/home/plasticlab/WineFridge/RPI/database/inventory.json')
        if drawer_id in self.inventory.get("drawers", {}):
            if str(position) in self.inventory["drawers"][drawer_id].get("positions", {}):
                self.inventory["drawers"][drawer_id]["positions"][str(position)] = {"occupied": False}
        self.inventory["last_updated"] = datetime.now().isoformat()
        total = sum(1 for d in self.inventory.get("drawers", {}).values()
                   for p in d.get("positions", {}).values() if p.get("occupied", False))
        self.inventory["total_bottles"] = total
        self.save_json('/home/plasticlab/WineFridge/RPI/database/inventory.json', self.inventory)

    def update_inventory(self, drawer_id, position, barcode, name, weight, fill_percentage=None):
        self.inventory = self.load_json('/home/plasticlab/WineFridge/RPI/database/inventory.json')
        if "drawers" not in self.inventory:
            self.inventory["drawers"] = {}
        if drawer_id not in self.inventory["drawers"]:
            self.inventory["drawers"][drawer_id] = {"positions": {}}
            
        if fill_percentage is None:
            fill_percentage = self.calculate_bottle_percentage(weight)
            
        self.inventory["drawers"][drawer_id]["positions"][str(position)] = {
            "occupied": True,
            "barcode": barcode,
            "name": name,
            "weight": weight,
            "fill_percentage": fill_percentage,
            "placed_date": datetime.now().isoformat(),
            "aging_days": 0
        }
        
        self.inventory["last_updated"] = datetime.now().isoformat()
        total = sum(1 for d in self.inventory.get("drawers", {}).values()
                   for p in d.get("positions", {}).values() if p.get("occupied", False))
        self.inventory["total_bottles"] = total
        self.save_json('/home/plasticlab/WineFridge/RPI/database/inventory.json', self.inventory)

    def handle_timeout(self, op_id):
        if op_id in self.pending_operations:
            op = self.pending_operations[op_id]
            print(f"[TIMEOUT] ⏱ Operation {op['type']} timed out")
            
            timeout_message = {
                "action": "bottle_placed" if op['type'] == 'load' else "bottle_unloaded",
                "source": "mqtt_handler",
                "data": {
                    "success": False,
                    "error": "Timeout",
                    "close_screen": True if op['type'] == 'load' else False
                },
                "timestamp": datetime.now().isoformat()
            }
            self.client.publish("winefridge/system/status", json.dumps(timeout_message))
            
            self.client.publish(f"winefridge/{op['drawer']}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))
            
            del self.pending_operations[op_id]

    def run(self):
        print("\n" + "="*50)
        print("  Wine Fridge MQTT Handler v3.2.0")
        print("  All fixes applied:")
        print("  1. Manual unload position marking")
        print("  2. Unload error modal")
        print("  3. Swap LEDs turn gray on completion")
        print("  4. Swap incorrect LED sequencing")
        print("  5. Zone lighting control")
        print("="*50 + "\n")
        try:
            self.client.loop_forever()
        finally:
            self.running = False
            if self.serial:
                self.serial.close()

if __name__ == "__main__":
    controller = WineFridgeController()
    controller.run()
