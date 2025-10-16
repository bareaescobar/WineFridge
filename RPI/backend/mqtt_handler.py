#!/usr/bin/env python3
import json
import paho.mqtt.client as mqtt
import time
from datetime import datetime
import threading

class WineFridgeController:
    def __init__(self):
        # Load databases
        self.inventory = self.load_json('/home/plasticlab/winefridge/RPI/database/inventory.json')
        self.catalog = self.load_json('/home/plasticlab/winefridge/RPI/database/wine-catalog.json')

        # Track pending operations
        self.pending_operations = {}

        # Setup MQTT with new API
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

        print("[INIT] Connecting to MQTT broker...")
        self.client.connect("localhost", 1883, 60)

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
            print(f"[SAVE] Updated {filepath}")
        except Exception as e:
            print(f"[ERROR] Saving {filepath}: {e}")

    def on_connect(self, client, userdata, flags, rc, properties):
        print(f"[MQTT] Connected with code {rc}")
        # Subscribe to all relevant topics
        client.subscribe("winefridge/+/status")
        client.subscribe("winefridge/system/status")
        client.subscribe("winefridge/system/command")
        print("[MQTT] Subscribed to topics")

    def on_message(self, client, userdata, msg):
        try:
            topic = msg.topic
            message = json.loads(msg.payload.decode())
            action = message.get('action', 'unknown')
            source = message.get('source', 'unknown')

            # Don't log heartbeats to reduce noise
            if action != 'heartbeat':
                print(f"[MQTT] {topic} | Action: {action} | Source: {source}")

            # CRITICAL: Skip our own messages on system/status to avoid loops
            if topic == "winefridge/system/status" and source == "mqtt_handler":
                return

            # Route messages based on topic
            if topic == "winefridge/system/status":
                self.handle_system_status(message)

            elif topic == "winefridge/system/command":
                # IGNORE start_swap bug
                if action != 'start_swap':
                    self.handle_system_command(message)
                elif action == 'start_swap':
                    print("[DEBUG] Ignoring spurious start_swap")

            elif "drawer_" in topic and "/status" in topic:
                drawer_id = topic.split('/')[1]
                if action != 'heartbeat':
                    self.handle_drawer_status(drawer_id, message)

        except Exception as e:
            print(f"[ERROR] Processing message: {e}")

    # <<< BLOQUE DUPLICADO ELIMINADO DE AQUÍ >>>

    def handle_system_status(self, message):
        """Handle barcode scans and other status messages"""
        action = message.get('action')
        source = message.get('source')

        # CRITICAL: Don't process our own messages to avoid loops
        if source == 'mqtt_handler':
            return

        if action == 'barcode_scanned':
            barcode = message['data']['barcode']
            print(f"[BARCODE] Scanned: {barcode}")

            # Check if wine exists in catalog
            wine = self.catalog.get("wines", {}).get(barcode)

            if wine:
                print(f"[BARCODE] Wine found: {wine.get('name')}")
                # Create NEW message with mqtt_handler as source
                wine_found_msg = {
                    "action": "barcode_scanned",
                    "source": "mqtt_handler",  # IMPORTANT: Mark as from us
                    "data": {
                        "barcode": barcode,
                        "wine": wine
                    },
                    "timestamp": datetime.now().isoformat()
                }
                self.client.publish("winefridge/system/status", json.dumps(wine_found_msg))
            else:
                print(f"[BARCODE] Wine NOT in catalog, using default")
                # Create a default wine entry for demo
                default_msg = {
                    "action": "barcode_scanned",
                    "source": "mqtt_handler",  # IMPORTANT: Mark as from us
                    "data": {
                        "barcode": barcode,
                        "wine": {
                            "name": "Unknown Wine",
                            "type": "Red",
                            "region": "Spain"
                        }
                    },
                    "timestamp": datetime.now().isoformat()
                }
                self.client.publish("winefridge/system/status", json.dumps(default_msg))

    def handle_system_command(self, message):
        """Handle commands from web interface"""
        action = message.get('action')
        data = message.get('data', {})

        print(f"[COMMAND] Action: {action}")

        if action == 'start_load':
            self.start_bottle_load(data)

    def start_bottle_load(self, data):
        """Start the bottle loading process"""
        barcode = data.get('barcode', 'unknown')
        name = data.get('name', 'Unknown Wine')

        print(f"[LOAD] Starting load process for {name} ({barcode})")

        # Find empty position in drawer 3
        position = self.find_empty_position('drawer_3')

        if not position:
            print("[ERROR] No empty positions in drawer_3")
            error_msg = {
                "action": "load_error",
                "source": "mqtt_handler",
                "data": {
                    "error": "No empty positions available"
                },
                "timestamp": datetime.now().isoformat()
            }
            self.client.publish("winefridge/system/status", json.dumps(error_msg))
            return

        print(f"[LOAD] Found empty position: drawer_3, position {position}")

        # Create operation tracking
        op_id = f"load_{int(time.time())}"
        self.pending_operations[op_id] = {
            'type': 'load',
            'barcode': barcode,
            'name': name,
            'drawer': 'drawer_3',
            'position': position,
            'timestamp': time.time()
        }

        print(f"[LOAD] Created operation {op_id}")

        # 1. Send LED command to highlight position
        led_cmd = {
            "action": "set_leds",
            "source": "mqtt_handler",
            "data": {
                "positions": [
                    {"position": position, "color": "#0000FF", "brightness": 100}
                ]
            },
            "timestamp": datetime.now().isoformat()
        }
        self.client.publish("winefridge/drawer_3/command", json.dumps(led_cmd))
        print(f"[LOAD] LED command sent for position {position}")

        # 2. Send expect_bottle command
        expect_cmd = {
            "action": "expect_bottle",
            "source": "mqtt_handler",
            "data": {
                "position": position,
                "barcode": barcode,
                "name": name,
                "timeout_ms": 30000
            },
            "timestamp": datetime.now().isoformat()
        }
        self.client.publish("winefridge/drawer_3/command", json.dumps(expect_cmd))
        print(f"[LOAD] Expect bottle command sent")

        # 3. Notify web to show drawer position (CHANGED ACTION NAME)
        web_msg = {
            "action": "expect_bottle",  # Changed from show_position
            "source": "mqtt_handler",
            "data": {
                "drawer": "drawer_3",
                "position": position,
                "wine_name": name
            },
            "timestamp": datetime.now().isoformat()
        }
        self.client.publish("winefridge/system/status", json.dumps(web_msg))
        print(f"[LOAD] Web notified with expect_bottle")

        # Set timeout
        threading.Timer(30, self.handle_timeout, [op_id]).start()

    def handle_drawer_status(self, drawer_id, message):
        """Handle status messages from drawers"""
        action = message.get('action')

        if action == 'bottle_event':
            data = message.get('data', {})
            event = data.get('event')
            position = data.get('position')
            weight = data.get('weight', 0)

            print(f"[DRAWER] {drawer_id} position {position}: {event} (weight: {weight}g)")

            if event == 'placed':
                self.handle_bottle_placed(drawer_id, position, weight)

    def handle_bottle_placed(self, drawer_id, position, weight):
        """Handle bottle placement detection"""
        print(f"[PLACED] Processing placement at {drawer_id} position {position}")
        print(f"[PLACED] Pending operations: {self.pending_operations}")

        # Find matching pending operation
        found_op = None
        for op_id, op in list(self.pending_operations.items()):
            print(f"[PLACED] Checking op {op_id}: drawer={op['drawer']}, pos={op['position']}")
            if op['drawer'] == drawer_id and op['position'] == position:
                found_op = (op_id, op)
                break

        if found_op:
            op_id, op = found_op
            print(f"[PLACED] Found matching operation {op_id}")

            # Update inventory
            self.update_inventory(drawer_id, position, op['barcode'], op['name'], weight)

            # Turn off LED
            led_off = {
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {
                    "positions": []
                },
                "timestamp": datetime.now().isoformat()
            }
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps(led_off))
            print(f"[PLACED] LED turned off")

            # Notify web of success
            success_msg = {
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
            }
            self.client.publish("winefridge/system/status", json.dumps(success_msg))
            print(f"[PLACED] Success message sent to web")

            # Remove operation
            del self.pending_operations[op_id]
            print(f"[PLACED] Operation {op_id} completed and removed")
        else:
            print(f"[PLACED] No matching operation found for {drawer_id} position {position}")

    def find_empty_position(self, drawer_id):
        """Find first empty position in drawer"""
        drawer = self.inventory.get("drawers", {}).get(drawer_id, {})
        positions = drawer.get("positions", {})

        # Check positions 1-9
        for pos in range(1, 10):
            pos_str = str(pos)
            if pos_str not in positions or not positions[pos_str].get("occupied", False):
                return pos

        return None

    def update_inventory(self, drawer_id, position, barcode, name, weight):
        """Update inventory file"""
        print(f"[INVENTORY] Updating {drawer_id} position {position}")

        # Reload inventory to get latest state
        self.inventory = self.load_json('/home/plasticlab/winefridge/RPI/database/inventory.json')

        # Ensure structure exists
        if "drawers" not in self.inventory:
            self.inventory["drawers"] = {}
        if drawer_id not in self.inventory["drawers"]:
            self.inventory["drawers"][drawer_id] = {"positions": {}}
        if "positions" not in self.inventory["drawers"][drawer_id]:
            self.inventory["drawers"][drawer_id]["positions"] = {}

        # Update position
        self.inventory["drawers"][drawer_id]["positions"][str(position)] = {
            "occupied": True,
            "barcode": barcode,
            "name": name,
            "weight": weight,
            "placed_date": datetime.now().isoformat(),
            "aging_days": 0
        }

        # Update last_updated
        self.inventory["last_updated"] = datetime.now().isoformat()

        # Count total bottles
        total = 0
        for drawer in self.inventory.get("drawers", {}).values():
            for pos in drawer.get("positions", {}).values():
                if pos.get("occupied", False):
                    total += 1
        self.inventory["total_bottles"] = total

        # Save to file
        self.save_json('/home/plasticlab/winefridge/RPI/database/inventory.json', self.inventory)
        print(f"[INVENTORY] Successfully updated. Total bottles: {total}")

    def handle_timeout(self, op_id):
        """Handle operation timeout"""
        if op_id in self.pending_operations:
            op = self.pending_operations[op_id]
            print(f"[TIMEOUT] Operation {op_id} timed out")

            # Clear LED
            led_off = {
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }
            self.client.publish(f"winefridge/{op['drawer']}/command", json.dumps(led_off))

            # Notify web
            timeout_msg = {
                "action": "bottle_placed",
                "source": "mqtt_handler",
                "data": {
                    "success": False,
                    "error": "Timeout - bottle not placed"
                },
                "timestamp": datetime.now().isoformat()
            }
            self.client.publish("winefridge/system/status", json.dumps(timeout_msg))

            del self.pending_operations[op_id]

    def run(self):
        print("[START] WineFridge MQTT Handler v2.1")
        try:
            self.client.loop_forever()
        except KeyboardInterrupt:
            print("\n[STOP] Shutting down...")
            self.client.disconnect()

if __name__ == "__main__":
    controller = WineFridgeController()
    controller.run()
