
#!/usr/bin/env python3
"""
WineFridge MQTT Handler
Version: 3.0.3
Date: 28.10.2025 18:15h

Handles MQTT communication between RPI, ESP32 drawers, and web interface.
Main features:
1. Barcode scanning and wine catalog lookup
2. Load/Unload/Swap bottle workflows
3. Weight-based inventory management
4. LED control for visual feedback
5. Real-time status monitoring
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

# Weight thresholds for bottle percentage
# Adjusted for prototype usage - more permissive thresholds
# Since 99% of bottles will be 100% full, use lower threshold to account for sensor variations
MIN_FULL_BOTTLE_WEIGHT = 700    # grams (minimum for 100% full) - lowered from 900g for sensor tolerance
MAX_FULL_BOTTLE_WEIGHT = 2000   # grams (maximum bottle weight)
EMPTY_BOTTLE_WEIGHT = 300       # grams (empty bottle) - lowered from 350g for sensor tolerance

def find_serial_port():
    """Detectar automáticamente el puerto serie en RPI5"""
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
            print(f"[SCANNER] Connected to {port}")
        except Exception as e:
            print(f"[SCANNER] Error: {e}")
            self.serial = None

        # Setup MQTT
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.on_disconnect = self.on_disconnect

        print("[INIT] Connecting to MQTT broker...")
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
        """Verificar si un código parece válido"""
        code = code.strip()
        if len(code) < 8 or len(code) > 20:
            return False
        if not re.match(r'^[A-Za-z0-9]+$', code):
            return False
        return True

    def calculate_bottle_percentage(self, weight):
        """
        Calculate bottle fill percentage based on weight

        Optimized for prototype where 99% of bottles are 100% full.
        Uses more permissive thresholds to account for sensor variations.

        Logic:
        - >= 700g = 100% (full bottle) - permissive threshold for sensor tolerance
        - 300g to 700g = proportional (partially consumed)
        - < 300g = 0% (empty)
        """
        if weight < EMPTY_BOTTLE_WEIGHT:
            return 0

        # Bottles >= 700g are considered 100% full (more permissive for sensor variations)
        if weight >= MIN_FULL_BOTTLE_WEIGHT:
            return 100

        # Calculate percentage between empty (300g) and full (700g)
        wine_weight = weight - EMPTY_BOTTLE_WEIGHT
        full_wine_weight = MIN_FULL_BOTTLE_WEIGHT - EMPTY_BOTTLE_WEIGHT
        percentage = (wine_weight / full_wine_weight) * 100

        return max(0, min(100, int(percentage)))

    def get_wine_type(self, barcode):
        """Get wine type from catalog"""
        if barcode in self.catalog.get('wines', {}):
            wine_data = self.catalog['wines'][barcode]
            wine_type = wine_data.get('type', '').lower()
            # Normalize wine type
            if 'rose' in wine_type or 'rosé' in wine_type or 'rosado' in wine_type:
                return 'rose'
            elif 'white' in wine_type or 'blanco' in wine_type:
                return 'white'
            elif 'red' in wine_type or 'tinto' in wine_type:
                return 'red'
        return None

    def run_scanner(self):
        """Barcode scanner loop running in background thread"""
        print("[SCANNER] Ready")
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

                            print(f"[SCANNER] Detected: {cleaned}")
                            self.handle_barcode_scanned(cleaned)

                            self.last_barcode = cleaned
                            self.last_scan_time = current_time

                        self.barcode_buffer = ""

                else:
                    # Timeout processing
                    current_time = time.time()
                    if (self.barcode_buffer and
                        current_time - last_data_time > 0.5 and
                        self.is_valid_barcode(self.barcode_buffer.strip())):

                        cleaned = self.barcode_buffer.strip()
                        if (cleaned != self.last_barcode or
                            current_time - self.last_scan_time > self.scan_cooldown):

                            print(f"[SCANNER] Detected (timeout): {cleaned}")
                            self.handle_barcode_scanned(cleaned)

                            self.last_barcode = cleaned
                            self.last_scan_time = current_time

                        self.barcode_buffer = ""

                time.sleep(0.05)

            except Exception as e:
                print(f"[SCANNER] Error: {e}")
                time.sleep(1)

    def handle_barcode_scanned(self, barcode):
        """Handle barcode scan - publish to MQTT"""
        message = {
            "action": "barcode_scanned",
            "source": "barcode_scanner",
            "data": {"barcode": barcode},
            "timestamp": datetime.now().isoformat()
        }
        self.client.publish("winefridge/system/status", json.dumps(message))

    def on_connect(self, client, userdata, flags, rc, properties):
        if rc == 0:
            print(f"[MQTT] ✓ Connected")
        else:
            print(f"[MQTT] ✗ Connection failed: {rc}")

        client.subscribe("winefridge/+/status")
        client.subscribe("winefridge/system/command")
        print("[MQTT] Subscribed")

    def on_disconnect(self, client, userdata, flags, rc, properties):
        """Callback for disconnect - VERSION2 API with flags parameter"""
        if rc != 0:
            print(f"[MQTT] ✗ Disconnected (rc={rc}), reconnecting...")

    def on_message(self, client, userdata, msg):
        try:
            message = json.loads(msg.payload.decode())
            action = message.get('action')
            source = message.get('source', 'unknown')

            # Only log non-heartbeat messages
            if action != 'heartbeat':
                if source == 'unknown':
                    print(f"[MQTT] {action} (no source field in message) - topic: {msg.topic}")
                else:
                    print(f"[MQTT] {action} from {source}")

            if msg.topic == 'winefridge/system/command':
                self.handle_system_command(message)
            elif '/status' in msg.topic:
                drawer_id = msg.topic.split('/')[1]
                if action == 'bottle_event':
                    self.handle_drawer_status(drawer_id, message)
                elif action == 'wrong_placement':
                    self.handle_wrong_placement(drawer_id, message)

        except Exception as e:
            print(f"[ERROR] {e}")

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

    def find_empty_position(self, preferred_drawer=None):
        """Find empty position, preferring specified drawer"""
        # First check preferred drawer
        if preferred_drawer and preferred_drawer in FUNCTIONAL_DRAWERS:
            positions = self.inventory.get("drawers", {}).get(preferred_drawer, {}).get("positions", {})
            for pos in range(1, 10):
                if str(pos) not in positions or not positions[str(pos)].get("occupied", False):
                    return preferred_drawer, pos

        # Then check all functional drawers
        for drawer_id in FUNCTIONAL_DRAWERS:
            if drawer_id == preferred_drawer:
                continue  # Already checked
            positions = self.inventory.get("drawers", {}).get(drawer_id, {}).get("positions", {})
            for pos in range(1, 10):
                if str(pos) not in positions or not positions[str(pos)].get("occupied", False):
                    return drawer_id, pos

        return None, None

    def start_bottle_load(self, data):
        barcode = data.get('barcode', 'unknown')
        name = data.get('name', 'Unknown Wine')
        print(f"[LOAD] {name[:30]}...")

        # Determine preferred drawer based on wine type
        wine_type = self.get_wine_type(barcode)
        preferred_drawer = WINE_TYPE_DRAWERS.get(wine_type) if wine_type else None

        if wine_type:
            print(f"[LOAD] Wine type: {wine_type} → Preferred drawer: {preferred_drawer}")

        drawer_id, position = self.find_empty_position(preferred_drawer)

        if not drawer_id or not position:
            print("[ERROR] No empty positions")
            error_msg = {
                "action": "load_error",
                "source": "mqtt_handler",
                "data": {"error": "No empty positions available"},
                "timestamp": datetime.now().isoformat()
            }
            self.client.publish("winefridge/system/status", json.dumps(error_msg))
            return

        print(f"[LOAD] → {drawer_id} #{position}")

        op_id = f"load_{int(time.time())}"
        self.pending_operations[op_id] = {
            'type': 'load',
            'barcode': barcode,
            'name': name,
            'drawer': drawer_id,
            'position': position,
            'expected_position': position,  # Track expected position
            'timestamp': time.time()
        }

        # LED GREEN BLINKING = action expected - place bottle here
        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {"positions": [{"position": position, "color": "#00FF00", "brightness": 100, "blink": True}]},
            "timestamp": datetime.now().isoformat()
        }))

        # Tell ESP32 which position to expect (so it can detect wrong placement without weighing)
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

        # Start timeout timer and store it so we can cancel/reset it
        timer = threading.Timer(60, self.handle_timeout, [op_id])
        timer.start()
        self.pending_operations[op_id]['timer'] = timer

    def start_bottle_unload(self, data):
        barcode = data.get('barcode')
        name = data.get('name', 'Unknown Wine')
        preferred_drawer = data.get('drawer')  # Get drawer from UI if specified
        print(f"[UNLOAD] {name[:30]}... (barcode: {barcode})")

        # If drawer specified, search in that drawer first
        if preferred_drawer:
            bottle_location = self.find_bottle_in_drawer(barcode, preferred_drawer)
            if bottle_location:
                drawer_id, position = bottle_location
            else:
                print(f"[ERROR] Bottle not found in specified drawer {preferred_drawer}")
                self.client.publish("winefridge/system/status", json.dumps({
                    "action": "unload_error",
                    "source": "mqtt_handler",
                    "data": {"error": f"Bottle not found in drawer {preferred_drawer}"},
                    "timestamp": datetime.now().isoformat()
                }))
                return
        else:
            # Fallback to searching all drawers
            bottle_location = self.find_bottle_in_inventory(barcode)
            if not bottle_location:
                print(f"[ERROR] Bottle not found")
                self.client.publish("winefridge/system/status", json.dumps({
                    "action": "unload_error",
                    "source": "mqtt_handler",
                    "data": {"error": "Bottle not found in inventory"},
                    "timestamp": datetime.now().isoformat()
                }))
                return
            drawer_id, position = bottle_location

        print(f"[UNLOAD] → {drawer_id} #{position}")

        if drawer_id not in FUNCTIONAL_DRAWERS:
            print(f"[ERROR] {drawer_id} has no sensors")
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
            'barcode': barcode,
            'name': name,
            'drawer': drawer_id,
            'position': position,
            'expected_position': position,  # Track expected position for wrong placement detection
            'timestamp': time.time()
        }

        # GREEN LED = action expected (remove bottle from here)
        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {"positions": [{"position": position, "color": "#00FF00", "brightness": 100}]},
            "timestamp": datetime.now().isoformat()
        }))

        self.client.publish("winefridge/system/status", json.dumps({
            "action": "expect_removal",
            "source": "mqtt_handler",
            "data": {"drawer": drawer_id, "position": position, "wine_name": name},
            "timestamp": datetime.now().isoformat()
        }))

        # Start timeout timer and store it
        timer = threading.Timer(60, self.handle_timeout, [op_id])
        timer.start()
        self.pending_operations[op_id]['timer'] = timer

    def start_swap_bottles(self):
        """Start swap bottle operation"""
        print("[SWAP] Starting swap operation...")

        self.swap_operations = {
            'active': True,
            'bottles_removed': [],
            'bottles_to_place': [],
            'start_time': time.time()
        }

        # Notify UI that swap has started
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "swap_started",
            "source": "mqtt_handler",
            "timestamp": datetime.now().isoformat()
        }))

    def cancel_swap(self):
        """Cancel swap operation"""
        print("[SWAP] Cancelling swap operation...")

        # Clear all LEDs
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

    def find_bottle_in_inventory(self, barcode):
        """Find bottle in inventory, prioritizing functional drawers"""
        # First search in functional drawers only (3, 5, 7)
        for drawer_id in FUNCTIONAL_DRAWERS:
            if drawer_id in self.inventory.get("drawers", {}):
                drawer_data = self.inventory["drawers"][drawer_id]
                for position, pos_data in drawer_data.get("positions", {}).items():
                    if pos_data.get("occupied") and pos_data.get("barcode") == barcode:
                        return drawer_id, int(position)

        # If not found in functional drawers, return None (don't search in non-functional drawers)
        return None

    def find_bottle_in_drawer(self, barcode, drawer_id):
        """Find a bottle by barcode in a specific drawer"""
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
        """Handle wrong position placement during LOAD operation"""
        data = message.get('data', {})
        position = data.get('position')
        expected_position = data.get('expected_position')

        print(f"[LOAD] ❌ Wrong placement! Expected #{expected_position}, got #{position} in {drawer_id}")

        # Find the current load operation
        for op_id, op in list(self.pending_operations.items()):
            if op.get('type') == 'load' and op.get('drawer') == drawer_id and op.get('position') == expected_position:
                # Mark wrong position in operation
                if 'wrong_positions' not in op:
                    op['wrong_positions'] = []
                if position not in op['wrong_positions']:
                    op['wrong_positions'].append(position)

                # Notify frontend to show error modal (frontend expects "placement_error")
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

    def handle_swap_event(self, drawer_id, position, event, weight):
        """
        Handle events during swap operation with improved LED feedback

        LED colors during swap:
        - YELLOW: Original position to lift bottle from
        - GREEN: Target position to place bottle
        - BLUE blinking: Weighing in progress
        """

        if event == 'removed':
            # Get bottle info from inventory
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
                print(f"[SWAP] Bottle {bottle_num} removed: {bottle_info['name']}")

                # Keep YELLOW on lifted position
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": [{"position": position, "color": "#FFFF00", "brightness": 100}]},
                    "timestamp": datetime.now().isoformat()
                }))

                # Notify UI
                self.client.publish("winefridge/system/status", json.dumps({
                    "action": "bottle_event",
                    "data": {
                        "event": "removed",
                        "drawer": drawer_id,
                        "position": position
                    },
                    "timestamp": datetime.now().isoformat()
                }))

                # If two bottles removed, start placement phase
                if len(self.swap_operations['bottles_removed']) == 2:
                    print("[SWAP] Both bottles removed, waiting for swap placement...")
                    # Set up expected placements (swapped positions)
                    bottle1 = self.swap_operations['bottles_removed'][0]
                    bottle2 = self.swap_operations['bottles_removed'][1]

                    # Expect bottle 1 in position 2 and bottle 2 in position 1
                    self.swap_operations['bottles_to_place'] = [
                        {'bottle': bottle1, 'target_drawer': bottle2['drawer'], 'target_position': bottle2['position']},
                        {'bottle': bottle2, 'target_drawer': bottle1['drawer'], 'target_position': bottle1['position']}
                    ]

                    # Change LED colors: GREEN BLINKING on target position for first bottle, YELLOW on second
                    # If same drawer, send both positions in one command to avoid overwriting
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
                        # Different drawers, send separately
                        # First target (where to place bottle1 - was bottle2's position) -> GREEN BLINKING
                        self.client.publish(f"winefridge/{bottle2['drawer']}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": bottle2['position'], "color": "#00FF00", "brightness": 100, "blink": True}]},
                            "timestamp": datetime.now().isoformat()
                        }))

                        # Keep YELLOW on first bottle's original position (where to place bottle2 later)
                        self.client.publish(f"winefridge/{bottle1['drawer']}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": bottle1['position'], "color": "#FFFF00", "brightness": 100}]},
                            "timestamp": datetime.now().isoformat()
                        }))

        elif event == 'placed':
            # Check if placement matches expected swap
            if len(self.swap_operations['bottles_to_place']) > 0:
                # Check if this placement matches any expected position
                match_found = False
                for i, expected in enumerate(self.swap_operations['bottles_to_place']):
                    if expected['target_drawer'] == drawer_id and expected['target_position'] == position:
                        match_found = True
                        print(f"[SWAP] Bottle placed correctly in swapped position")

                        # BLUE blinking during weighing
                        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": position, "color": "#0000FF", "brightness": 100, "blink": True}]},
                            "timestamp": datetime.now().isoformat()
                        }))

                        # Update inventory with swapped bottle - preserve original weight
                        bottle_info = expected['bottle']
                        original_weight = bottle_info.get('weight', weight)  # Use stored weight
                        fill_percentage = self.calculate_bottle_percentage(original_weight)

                        self.update_inventory(drawer_id, position, bottle_info['barcode'],
                                             bottle_info['name'], original_weight, fill_percentage)

                        # Remove from pending placements
                        self.swap_operations['bottles_to_place'].pop(i)

                        # After weighing, set to GREEN
                        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": [{"position": position, "color": "#00FF00", "brightness": 100}]},
                            "timestamp": datetime.now().isoformat()
                        }))

                        # Notify UI
                        self.client.publish("winefridge/system/status", json.dumps({
                            "action": "bottle_event",
                            "data": {
                                "event": "placed",
                                "drawer": drawer_id,
                                "position": position
                            },
                            "timestamp": datetime.now().isoformat()
                        }))

                        # If first bottle placed, change second target from YELLOW to GREEN
                        if len(self.swap_operations['bottles_to_place']) == 1:
                            remaining = self.swap_operations['bottles_to_place'][0]
                            target_drawer = remaining['target_drawer']
                            target_position = remaining['target_position']

                            print(f"[SWAP] First bottle placed, now place second bottle at {target_drawer} #{target_position}")

                            # Change from YELLOW to GREEN BLINKING for second placement
                            self.client.publish(f"winefridge/{target_drawer}/command", json.dumps({
                                "action": "set_leds",
                                "source": "mqtt_handler",
                                "data": {"positions": [{"position": target_position, "color": "#00FF00", "brightness": 100, "blink": True}]},
                                "timestamp": datetime.now().isoformat()
                            }))

                        # If all bottles placed, swap complete
                        elif len(self.swap_operations['bottles_to_place']) == 0:
                            print("[SWAP] ✓ Swap completed successfully!")

                            # Clear all LEDs (deduplicate drawers to avoid multiple commands to same drawer)
                            drawers_to_clear = set(bottle['drawer'] for bottle in self.swap_operations['bottles_removed'])
                            for drawer in drawers_to_clear:
                                self.client.publish(f"winefridge/{drawer}/command", json.dumps({
                                    "action": "set_leds",
                                    "source": "mqtt_handler",
                                    "data": {"positions": []},
                                    "timestamp": datetime.now().isoformat()
                                }))

                            # Notify UI of success
                            self.client.publish("winefridge/system/status", json.dumps({
                                "action": "swap_completed",
                                "source": "mqtt_handler",
                                "data": {"success": True},
                                "timestamp": datetime.now().isoformat()
                            }))

                            # Reset swap operation
                            self.swap_operations = {
                                'active': False,
                                'bottles_removed': [],
                                'bottles_to_place': [],
                                'start_time': None
                            }
                        break

                # Wrong position during swap
                if not match_found:
                    print(f"[SWAP ERROR] Wrong placement at {drawer_id} #{position}")

                    # Build LED positions: RED on wrong, GREEN BLINKING on expected
                    led_positions = [{"position": position, "color": "#FF0000", "brightness": 100}]

                    # Add expected positions (GREEN BLINKING for current targets)
                    for expected in self.swap_operations['bottles_to_place']:
                        if expected['target_drawer'] == drawer_id:
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

                    # Notify UI of wrong placement
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
        """Handle normal bottle placement (non-swap)"""

        # Calculate fill percentage
        fill_percentage = self.calculate_bottle_percentage(weight)

        # Find matching pending operation
        found_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op['drawer'] == drawer_id and op['type'] == 'load':
                # Check if placed in correct position
                if op['expected_position'] == position:
                    found_op = (op_id, op, True)  # Correct placement
                else:
                    found_op = (op_id, op, False)  # Wrong placement
                break

        if not found_op:
            return  # Manual placement

        op_id, op, correct_placement = found_op

        if not correct_placement:
            # Wrong position - immediate feedback
            print(f"[ERROR] Wrong placement! Expected #{op['expected_position']}, got #{position}")

            # RED LED on wrong position + GREEN BLINKING LED on correct position
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [
                    {"position": position, "color": "#FF0000", "brightness": 100, "blink": False},
                    {"position": op['expected_position'], "color": "#00FF00", "brightness": 100, "blink": True}
                ]},
                "timestamp": datetime.now().isoformat()
            }))

            # Notify UI immediately about wrong placement
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

            # Reset timeout timer
            if 'timer' in op:
                op['timer'].cancel()
                print("[TIMEOUT] Cancelled old timer, starting new 60s timer")

            new_timer = threading.Timer(60, self.handle_timeout, [op_id])
            new_timer.start()
            op['timer'] = new_timer

            # Keep operation pending but mark the wrong position
            op['wrong_position'] = position

        else:
            # Correct placement
            print(f"[PLACED] ✓ {drawer_id} #{position} - {fill_percentage}% full ({weight}g)")

            # Update inventory
            self.update_inventory(drawer_id, position, op['barcode'], op['name'], weight, fill_percentage)

            # Show SOLID GREEN (ESP32 was showing green blinking during weighing, now show solid)
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [{"position": position, "color": "#00FF00", "brightness": 100, "blink": False}]},
                "timestamp": datetime.now().isoformat()
            }))

            # Store drawer and position for later LED change to white
            op['confirmed_drawer'] = drawer_id
            op['confirmed_position'] = position
            op['confirmed'] = True  # Mark as ready for white LED after DONE

            # Notify success with fill percentage
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

            # Cancel timeout timer
            if 'timer' in op:
                op['timer'].cancel()

            # Reset scanner
            self.last_barcode = ""
            self.last_scan_time = 0

            # Keep operation in pending until user clicks DONE (will change LED to white then)
            # Do NOT delete the operation yet

    def handle_bottle_removed(self, drawer_id, position):
        """Handle bottle removal with wrong position detection"""

        # Check if this is part of wrong placement recovery (LOAD operation)
        wrong_placement_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op.get('wrong_position') == position and op['drawer'] == drawer_id and op['type'] == 'load':
                wrong_placement_op = (op_id, op)
                break

        if wrong_placement_op:
            op_id, op = wrong_placement_op
            print(f"[REMOVED] Wrong bottle removed from #{position}")

            # Clear red LED
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))

            # Re-highlight correct position (GREEN BLINKING = place bottle here)
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [{"position": op['expected_position'], "color": "#00FF00", "brightness": 100, "blink": True}]},
                "timestamp": datetime.now().isoformat()
            }))

            # Remove wrong position marker
            if 'wrong_position' in op:
                del op['wrong_position']

            # Notify web that wrong bottle was removed
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

        # Check for UNLOAD operations
        found_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op['drawer'] == drawer_id and op['type'] == 'unload':
                # Check if removed from correct position
                if op['expected_position'] == position:
                    found_op = (op_id, op, True)  # Correct removal
                else:
                    found_op = (op_id, op, False)  # Wrong removal
                break

        if not found_op:
            return  # Manual removal

        op_id, op, correct_removal = found_op

        if not correct_removal:
            # Wrong position removed during unload
            print(f"[ERROR] Wrong bottle removed! Expected #{op['expected_position']}, got #{position}")

            # Red LED on wrong position
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": [
                    {"position": position, "color": "#FF0000", "brightness": 100},
                    {"position": op['expected_position'], "color": "#00FF00", "brightness": 100}
                ]},
                "timestamp": datetime.now().isoformat()
            }))

            # Notify UI about wrong removal
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "unload_error",
                "source": "mqtt_handler",
                "data": {
                    "error": "wrong_bottle_removed",
                    "drawer": drawer_id,
                    "wrong_position": position,
                    "correct_position": op['expected_position'],
                    "wine_name": op['name']
                },
                "timestamp": datetime.now().isoformat()
            }))

            # Mark wrong position for tracking
            op['wrong_removal_position'] = position

            return

        # Correct removal
        print(f"[REMOVED] ✓ {drawer_id} #{position}")

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

        # Cancel timeout timer
        if 'timer' in op:
            op['timer'].cancel()

        # Reset scanner
        self.last_barcode = ""
        self.last_scan_time = 0

        del self.pending_operations[op_id]

    def retry_placement(self, data):
        """Handle retry after wrong placement"""
        print("[RETRY] User requested retry...")

        # Find and cancel the pending operation
        for op_id in list(self.pending_operations.keys()):
            op = self.pending_operations[op_id]
            if 'wrong_position' in op:
                # Cancel timeout timer
                if 'timer' in op:
                    op['timer'].cancel()

                # Clear all LEDs
                self.client.publish(f"winefridge/{op['drawer']}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": []},
                    "timestamp": datetime.now().isoformat()
                }))

                del self.pending_operations[op_id]
                print(f"[RETRY] Cancelled operation {op_id}")
                break

        # Reset scanner to allow new scan
        self.last_barcode = ""
        self.last_scan_time = 0

        # Notify UI
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "retry_ready",
            "source": "mqtt_handler",
            "timestamp": datetime.now().isoformat()
        }))

    def complete_load_operation(self, data):
        """Handle load completion when user clicks DONE - change LED to white, clear only load-related LEDs"""
        print("[LOAD] User clicked DONE - changing LED to white...")

        # Find the confirmed load operation
        for op_id in list(self.pending_operations.keys()):
            op = self.pending_operations[op_id]
            if op.get('confirmed') and op.get('type') == 'load':
                drawer_id = op['confirmed_drawer']
                position = op['confirmed_position']

                # Build list of LEDs to update
                leds_to_update = []

                # Turn off any wrong position LEDs (red)
                if 'wrong_positions' in op:
                    for wrong_pos in op['wrong_positions']:
                        leds_to_update.append({"position": wrong_pos, "color": "#000000", "brightness": 0, "blink": False})

                # Change correct position from GREEN to WHITE/OCCUPIED
                leds_to_update.append({"position": position, "color": "#444444", "brightness": 100, "blink": False})

                # Send single command with all LED changes
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": leds_to_update},
                    "timestamp": datetime.now().isoformat()
                }))

                # Reset expectedPosition to 0 (no expectation)
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "expect_bottle",
                    "source": "mqtt_handler",
                    "data": {"position": 0},
                    "timestamp": datetime.now().isoformat()
                }))

                print(f"[LOAD] ✓ Completed - LED changed to WHITE at {drawer_id} #{position}")

                # Clean up operation
                del self.pending_operations[op_id]
                break

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

        # Calculate fill percentage if not provided
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
            print(f"[TIMEOUT] {op['type']}")

            # Send timeout error with close_screen flag for load operations
            timeout_message = {
                "action": "bottle_placed" if op['type'] == 'load' else "bottle_unloaded",
                "source": "mqtt_handler",
                "data": {
                    "success": False,
                    "error": "Timeout",
                    "close_screen": True if op['type'] == 'load' else False  # Signal frontend to close load screen
                },
                "timestamp": datetime.now().isoformat()
            }

            self.client.publish("winefridge/system/status", json.dumps(timeout_message))

            # Clear LEDs
            self.client.publish(f"winefridge/{op['drawer']}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))

            del self.pending_operations[op_id]

    def run(self):
        print("[START] WineFridge MQTT Handler - Complete v4.0")
        print("[INFO] Features: Wrong placement detection, weight %, wine routing, swap bottles")
        try:
            self.client.loop_forever()
        finally:
            self.running = False
            if self.serial:
                self.serial.close()

if __name__ == "__main__":
    controller = WineFridgeController()
    controller.run()
