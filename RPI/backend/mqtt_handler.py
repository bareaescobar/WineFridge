import paho.mqtt.client as mqtt
import json
import time
import threading
from datetime import datetime
import os

# === File Paths ===
INVENTORY_PATH = "../database/inventory.json"
CATALOG_PATH = "../database/wine-catalog.json"
LOG_PATH = "../logs/mqtt.log"

class WineFridgeController:
    def __init__(self):
        self.inventory = self.load_json(INVENTORY_PATH)
        self.catalog = self.load_json(CATALOG_PATH)
        self.pending_operations = {}
        self.client = mqtt.Client()
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message
        self.client.connect("localhost", 1883)

    def on_connect(self, client, userdata, flags, rc):
        print("[MQTT] Connected with result code", rc)
        client.subscribe("winefridge/+/status")
        client.subscribe("winefridge/system/command")

    def on_message(self, client, userdata, msg):
        try:
            message = json.loads(msg.payload.decode())
            topic_parts = msg.topic.split('/')
            component = topic_parts[1]

            self.log_message(msg.topic, message)

            if component.startswith("drawer_"):
                self.handle_drawer_message(component, message)
            elif component == "lighting":
                pass  # Handle lighting if needed
            elif component == "system":
                self.handle_web_command(message)
        except Exception as e:
            print("[ERROR] on_message:", e)

    # === Handlers ===

    def handle_drawer_message(self, drawer_id, message):
        action = message["action"]
        data = message.get("data", {})

        if action == "heartbeat":
            pass  # You can store last_seen here
        elif action == "bottle_event":
            self.process_bottle_event(drawer_id, data)
        elif action == "error":
            self.send_to_web("error", data)

    def handle_web_command(self, message):
        action = message["action"]
        data = message.get("data", {})

        if action == "start_load":
            self.start_bottle_loading(data["barcode"])
        elif action == "start_unload":
            self.start_bottle_removal(data["position"])

    def start_bottle_loading(self, barcode):
        wine = self.catalog.get("wines", {}).get(barcode)
        if not wine:
            self.send_to_web("error", {"message": "Unknown wine"})
            return

        slot = self.find_empty_position()
        if not slot:
            self.send_to_web("error", {"message": "Fridge full"})
            return

        drawer, position = slot["drawer"], slot["position"]
        op_id = f"{drawer}_{position}_{int(time.time())}"
        self.pending_operations[op_id] = {
            "type": "load",
            "barcode": barcode,
            "drawer": drawer,
            "position": position,
            "started": time.time()
        }

        self.send_drawer_command(drawer, {
            "action": "set_leds",
            "source": "rpi",
            "data": {
                "positions": [{"position": position, "color": "#0000FF", "brightness": 100}],
                "duration_ms": 0
            }
        })

        self.send_drawer_command(drawer, {
            "action": "expect_bottle",
            "source": "rpi",
            "data": {
                "position": position,
                "barcode": barcode,
                "name": wine["name"],
                "timeout_ms": 30000
            }
        })

        self.send_to_web("expect_bottle", {
            "drawer": drawer,
            "position": position,
            "wine_name": wine["name"]
        })

        threading.Timer(30, self.check_timeout, [op_id]).start()

    def process_bottle_event(self, drawer, data):
        pos = data["position"]
        weight = data["weight"]
        op = self.find_pending_op(drawer, pos)

        if op and op["type"] == "load":
            barcode = op["barcode"]
            wine = self.catalog["wines"].get(barcode, {})
            self.inventory["drawers"].setdefault(drawer, {}).setdefault("positions", {})[str(pos)] = {
                "occupied": True,
                "barcode": barcode,
                "name": wine.get("name"),
                "weight": weight,
                "placed_date": datetime.now().isoformat(),
                "aging_days": 0
            }
            self.save_json(INVENTORY_PATH, self.inventory)

            self.send_drawer_command(drawer, {
                "action": "set_leds",
                "source": "rpi",
                "data": {
                    "positions": [{"position": pos, "color": "#00FF00", "brightness": 100}],
                    "duration_ms": 3000
                }
            })

            self.send_to_web("bottle_placed", {
                "success": True,
                "drawer": drawer,
                "position": pos,
                "wine_name": wine.get("name")
            })

            del self.pending_operations[op["id"]]

    def check_timeout(self, op_id):
        if op_id in self.pending_operations:
            op = self.pending_operations.pop(op_id)
            self.send_to_web("error", {
                "message": f"Bottle placement timed out at {op['drawer']} position {op['position']}"
            })

    def start_bottle_removal(self, position):
        # Optional: implement if you support unloading
        pass

    # === Utility ===

    def find_empty_position(self):
        drawers = self.inventory.get("drawers", {})
        for drawer in ["drawer_1", "drawer_2", "drawer_3"]:
            for pos in range(1, 10):
                if not drawers.get(drawer, {}).get("positions", {}).get(str(pos), {}).get("occupied", False):
                    return {"drawer": drawer, "position": pos}
        return None

    def find_pending_op(self, drawer, pos):
        for op_id, op in self.pending_operations.items():
            if op["drawer"] == drawer and op["position"] == pos:
                op["id"] = op_id
                return op
        return None

    def send_drawer_command(self, drawer, cmd):
        self.client.publish(f"winefridge/{drawer}/command", json.dumps(cmd))

    def send_to_web(self, action, data):
        self.client.publish("winefridge/system/status", json.dumps({
            "action": action,
            "source": "rpi",
            "data": data,
            "timestamp": datetime.now().isoformat()
        }))

    def log_message(self, topic, msg):
        with open(LOG_PATH, 'a') as f:
            f.write(f"{datetime.now()} | {topic} | {json.dumps(msg)}\n")

    def load_json(self, path):
        try:
            with open(path, 'r') as f:
                return json.load(f)
        except:
            return {}

    def save_json(self, path, data):
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, 'w') as f:
            json.dump(data, f, indent=2)

# === Start Controller ===
if __name__ == "__main__":
    controller = WineFridgeController()
    controller.client.loop_forever()
