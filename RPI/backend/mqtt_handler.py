#!/usr/bin/env python3
import json
import paho.mqtt.client as mqtt
import serial
import time
from datetime import datetime
import threading
import re

# Define functional drawers with sensors
FUNCTIONAL_DRAWERS = ['drawer_3', 'drawer_5', 'drawer_7']

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
        self.inventory = self.load_json('/home/plasticlab/winefridge/RPI/database/inventory.json')
        self.catalog = self.load_json('/home/plasticlab/winefridge/RPI/database/wine-catalog.json')

        # Track pending operations
        self.pending_operations = {}

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
            source = message.get('source')
            
            # Only log non-heartbeat messages
            if action != 'heartbeat':
                print(f"[MQTT] {action} from {source}")

            if msg.topic == 'winefridge/system/command':
                self.handle_system_command(message)
            elif '/status' in msg.topic and action == 'bottle_event':
                drawer_id = msg.topic.split('/')[1]
                self.handle_drawer_status(drawer_id, message)
            
        except Exception as e:
            print(f"[ERROR] {e}")

    def handle_system_command(self, message):
        action = message.get('action')
        data = message.get('data', {})

        if action == 'start_load':
            self.start_bottle_load(data)
        elif action == 'start_unload':
            self.start_bottle_unload(data)

    def start_bottle_load(self, data):
        barcode = data.get('barcode', 'unknown')
        name = data.get('name', 'Unknown Wine')
        print(f"[LOAD] {name[:30]}...")

        drawer_id, position = self.find_empty_position()

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
            'timestamp': time.time()
        }

        # LED + expect commands
        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {"positions": [{"position": position, "color": "#0000FF", "brightness": 100}]},
            "timestamp": datetime.now().isoformat()
        }))

        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "expect_bottle",
            "source": "mqtt_handler",
            "data": {"position": position, "barcode": barcode, "name": name, "timeout_ms": 30000},
            "timestamp": datetime.now().isoformat()
        }))

        self.client.publish("winefridge/system/status", json.dumps({
            "action": "expect_bottle",
            "source": "mqtt_handler",
            "data": {"drawer": drawer_id, "position": position, "wine_name": name},
            "timestamp": datetime.now().isoformat()
        }))

        threading.Timer(30, self.handle_timeout, [op_id]).start()

    def start_bottle_unload(self, data):
        barcode = data.get('barcode')
        name = data.get('name', 'Unknown Wine')
        print(f"[UNLOAD] {name[:30]}...")

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
            return

        op_id = f"unload_{int(time.time())}"
        self.pending_operations[op_id] = {
            'type': 'unload',
            'barcode': barcode,
            'name': name,
            'drawer': drawer_id,
            'position': position,
            'timestamp': time.time()
        }

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

        threading.Timer(30, self.handle_timeout, [op_id]).start()

    def find_bottle_in_inventory(self, barcode):
        for drawer_id, drawer_data in self.inventory.get("drawers", {}).items():
            for position, pos_data in drawer_data.get("positions", {}).items():
                if pos_data.get("occupied") and pos_data.get("barcode") == barcode:
                    return drawer_id, int(position)
        return None

    def handle_drawer_status(self, drawer_id, message):
        data = message.get('data', {})
        event = data.get('event')
        position = data.get('position')
        weight = data.get('weight', 0)

        if event == 'placed':
            self.handle_bottle_placed(drawer_id, position, weight)
        elif event == 'removed':
            self.handle_bottle_removed(drawer_id, position)

    def handle_bottle_placed(self, drawer_id, position, weight):
        found_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op['drawer'] == drawer_id and op['position'] == position:
                found_op = (op_id, op)
                break

        if not found_op:
            return  # Manual placement

        op_id, op = found_op
        print(f"[PLACED] ✓ {drawer_id} #{position}")
        
        self.update_inventory(drawer_id, position, op['barcode'], op['name'], weight)

        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {"positions": []},
            "timestamp": datetime.now().isoformat()
        }))

        self.client.publish("winefridge/system/status", json.dumps({
            "action": "bottle_placed",
            "source": "mqtt_handler",
            "data": {
                "success": True,
                "drawer": drawer_id,
                "position": position,
                "barcode": op['barcode'],
                "wine_name": op['name'],
                "weight": weight
            },
            "timestamp": datetime.now().isoformat()
        }))

        # Reset scanner
        self.last_barcode = ""
        self.last_scan_time = 0

        del self.pending_operations[op_id]

    def handle_bottle_removed(self, drawer_id, position):
        found_op = None
        for op_id, op in list(self.pending_operations.items()):
            if op['drawer'] == drawer_id and op['position'] == position and op['type'] == 'unload':
                found_op = (op_id, op)
                break

        if not found_op:
            return  # Manual removal

        op_id, op = found_op
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

        # Reset scanner
        self.last_barcode = ""
        self.last_scan_time = 0

        del self.pending_operations[op_id]

    def clear_inventory_position(self, drawer_id, position):
        self.inventory = self.load_json('/home/plasticlab/winefridge/RPI/database/inventory.json')

        if drawer_id in self.inventory.get("drawers", {}):
            if str(position) in self.inventory["drawers"][drawer_id].get("positions", {}):
                self.inventory["drawers"][drawer_id]["positions"][str(position)] = {"occupied": False}

        self.inventory["last_updated"] = datetime.now().isoformat()
        total = sum(1 for d in self.inventory.get("drawers", {}).values() 
                   for p in d.get("positions", {}).values() if p.get("occupied", False))
        self.inventory["total_bottles"] = total

        self.save_json('/home/plasticlab/winefridge/RPI/database/inventory.json', self.inventory)

    def find_empty_position(self):
        for drawer_id in FUNCTIONAL_DRAWERS:
            positions = self.inventory.get("drawers", {}).get(drawer_id, {}).get("positions", {})
            for pos in range(1, 10):
                if str(pos) not in positions or not positions[str(pos)].get("occupied", False):
                    return drawer_id, pos
        return None, None

    def update_inventory(self, drawer_id, position, barcode, name, weight):
        self.inventory = self.load_json('/home/plasticlab/winefridge/RPI/database/inventory.json')

        if "drawers" not in self.inventory:
            self.inventory["drawers"] = {}
        if drawer_id not in self.inventory["drawers"]:
            self.inventory["drawers"][drawer_id] = {"positions": {}}

        self.inventory["drawers"][drawer_id]["positions"][str(position)] = {
            "occupied": True,
            "barcode": barcode,
            "name": name,
            "weight": weight,
            "placed_date": datetime.now().isoformat(),
            "aging_days": 0
        }

        self.inventory["last_updated"] = datetime.now().isoformat()
        total = sum(1 for d in self.inventory.get("drawers", {}).values() 
                   for p in d.get("positions", {}).values() if p.get("occupied", False))
        self.inventory["total_bottles"] = total

        self.save_json('/home/plasticlab/winefridge/RPI/database/inventory.json', self.inventory)

    def handle_timeout(self, op_id):
        if op_id in self.pending_operations:
            op = self.pending_operations[op_id]
            print(f"[TIMEOUT] {op['type']}")
            
            self.client.publish("winefridge/system/status", json.dumps({
                "action": "bottle_placed" if op['type'] == 'load' else "bottle_unloaded",
                "source": "mqtt_handler",
                "data": {"success": False, "error": f"Timeout"},
                "timestamp": datetime.now().isoformat()
            }))
            
            self.client.publish(f"winefridge/{op['drawer']}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))
            
            del self.pending_operations[op_id]

    def run(self):
        print("[START] WineFridge Unified Controller v3.0")
        try:
            self.client.loop_forever()
        finally:
            self.running = False
            if self.serial:
                self.serial.close()

if __name__ == "__main__":
    controller = WineFridgeController()
    controller.run()
