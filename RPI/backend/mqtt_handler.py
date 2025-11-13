#!/usr/bin/env python3
"""
WineFridge MQTT Handler
Version: 3.3.1
Date: 06.11.2025

FIXES:
1. ... (Historial de fixes) ...
7. MODIFIED: Reverted all command handling to a single, consistent
   "nested" structure (i.e., 'action' is inside 'data') to match
   all existing web clients (load.js, unload.js).
8. FIXED: `handle_zone_lighting` ahora envía los comandos múltiples
   correctos a los controladores de cajón y zona (ej. lighting_8, drawer_7)
   y usa la escala de brillo 0-100.
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
        print("[INIT] Wine Fridge Controller v3.3.1")
        
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

        # Sync LEDs with current inventory state
        self.sync_leds_with_inventory()

    def sync_leds_with_inventory(self):
        """Synchronize drawer LEDs with current inventory state on startup"""
        print("[SYNC] Synchronizing LEDs with inventory...")

        for drawer_id in FUNCTIONAL_DRAWERS:
            if drawer_id in self.inventory.get("drawers", {}):
                positions_data = self.inventory["drawers"][drawer_id].get("positions", {})
                led_positions = []

                # Add gray LEDs for occupied positions
                for pos_str, pos_data in positions_data.items():
                    if pos_data.get("occupied", False):
                        led_positions.append({
                            "position": int(pos_str),
                            "color": "#808080",
                            "brightness": 30
                        })

                # Send LED command for this drawer
                if led_positions:
                    self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                        "action": "set_leds",
                        "source": "mqtt_handler",
                        "data": {"positions": led_positions},
                        "timestamp": datetime.now().isoformat()
                    }))
                    print(f"[SYNC] → {drawer_id}: {len(led_positions)} occupied positions")
                else:
                    # Turn off all LEDs if drawer is empty
                    self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                        "action": "set_leds",
                        "source": "mqtt_handler",
                        "data": {"positions": []},
                        "timestamp": datetime.now().isoformat()
                    }))
                    print(f"[SYNC] → {drawer_id}: empty")

        print("[SYNC] ✔ LED synchronization complete\n")

    def on_disconnect(self, client, userdata, flags, rc, properties):
        if rc != 0:
            print(f"[MQTT] ✗ Disconnected (rc={rc}), reconnecting...")

    def on_message(self, client, userdata, msg):
        try:
            message = json.loads(msg.payload.decode())
            source = message.get('source', 'unknown')
            
            # MODIFIED: Always get 'action' from the 'data' object for commands,
            # or the top level for status messages.
            action = None
            if msg.topic == 'winefridge/system/command':
                action = message.get('data', {}).get('action')
            else:
                action = message.get('action') # For status like 'bottle_event'

            if action and action != 'heartbeat':
                print(f"[MQTT] ← {action} from {source}")
                
            if msg.topic == 'winefridge/system/command':
                # Pass the nested 'data' object to the command handler
                self.handle_system_command(message.get('data', {}))
                
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

    # MODIFIED: Now receives the 'data' object directly
    def handle_system_command(self, data):
        action = data.get('action')

        # All handlers will receive the 'data' object
        if action == 'start_load':
            self.start_bottle_load(data)
        elif action == 'start_unload':
            self.start_bottle_unload(data)
        elif action == 'start_swap':
            self.start_swap_bottles() # This action needs no data
        elif action == 'cancel_load':
            self.cancel_load(data)
        elif action == 'cancel_unload':
            self.cancel_unload(data)
        elif action == 'cancel_swap':
            self.cancel_swap() # This action needs no data
        elif action == 'retry_placement':
            self.retry_placement(data)
        elif action == 'load_complete':
            self.complete_load_operation(data)
        elif action == 'set_brightness':
            self.handle_zone_lighting(data)
        elif action == 'update_setting':
            self.handle_zone_settings(data)
        elif action == 'set_lighting_mode':
            self.handle_fridge_lighting(data)
        elif action == 'shutdown':
            self.handle_shutdown()

    # =========================================================================
    # FUNCIÓN CORREGIDA
    # =========================================================================
    def handle_zone_lighting(self, data):
        """
        Maneja los cambios de iluminación de la zona.
        Esto es complejo porque una "zona" (ej. 'upper') controla
        múltiples luces en diferentes tópicos de MQTT.
        """
        zone_name = data.get('zone') # 'upper', 'middle', 'lower'
        brightness_str = data.get('value', '0')
        color_temp_name = data.get('color_temp', 'neutral_white')

        print(f"[LIGHTING] Received command for {zone_name} zone: "
              f"Brightness={brightness_str}%, Temp={color_temp_name}")

        # 1. Convertir nombre de temp a valor Kelvin
        temp_map = {
            'warm_white': 2700,
            'neutral_white': 4000,
            'cool_white': 6500
        }
        temperature = temp_map.get(color_temp_name, 4000)

        # 2. Convertir brillo string a int (0-100)
        #    NO convertir a 0-255. El comando funcional usa 0-100.
        try:
            brightness = int(float(brightness_str))
            brightness = max(0, min(100, brightness)) # Asegurar entre 0-100
        except ValueError:
            brightness = 0

        # 3. Definir payloads de comando
        
        # Payload para controladores de cajón "independientes" (ej. drawer_7)
        # (Basado en tu mosquitto_pub, no necesita 'drawer' key)
        drawer_payload_data = {
            "temperature": temperature,
            "brightness": brightness
        }
        
        # Payload base para controladores "multi-cajón" (ej. lighting_8)
        # (Necesitará 'drawer' o 'zone' key añadidas)
        multi_controller_payload_data = {
            "temperature": temperature,
            "brightness": brightness
        }

        commands_to_send = []
        
        if zone_name == 'upper':
            # Zona 3, Cajones 7, 8, 9
            # Cajón 7 -> drawer_7/command
            commands_to_send.append({
                "topic": "winefridge/drawer_7/command",
                "payload": {"action": "set_general_light", "data": drawer_payload_data}
            })
            # Cajón 8 -> lighting_8/command
            commands_to_send.append({
                "topic": "winefridge/lighting_8/command",
                "payload": {"action": "set_general_light", "data": {**multi_controller_payload_data, "drawer": 8}}
            })
            # Cajón 9 -> lighting_8/command
            commands_to_send.append({
                "topic": "winefridge/lighting_8/command",
                "payload": {"action": "set_general_light", "data": {**multi_controller_payload_data, "drawer": 9}}
            })
            # Zona 3 -> lighting_8/command
            commands_to_send.append({
                "topic": "winefridge/lighting_8/command",
                "payload": {"action": "set_zone_light", "data": {**multi_controller_payload_data, "zone": 3}}
            })

        elif zone_name == 'middle':
            # Zona 2, Cajones 4, 5, 6
            # (Lógica inferida de tu patrón 'upper')
            # Cajón 4 -> drawer_4/command
            commands_to_send.append({
                "topic": "winefridge/drawer_4/command",
                "payload": {"action": "set_general_light", "data": drawer_payload_data}
            })
            # Cajón 5 -> lighting_6/command
            commands_to_send.append({
                "topic": "winefridge/lighting_6/command",
                "payload": {"action": "set_general_light", "data": {**multi_controller_payload_data, "drawer": 5}}
            })
            # Cajón 6 -> lighting_6/command
            commands_to_send.append({
                "topic": "winefridge/lighting_6/command",
                "payload": {"action": "set_general_light", "data": {**multi_controller_payload_data, "drawer": 6}}
            })
            # Zona 2 -> lighting_6/command
            commands_to_send.append({
                "topic": "winefridge/lighting_6/command",
                "payload": {"action": "set_zone_light", "data": {**multi_controller_payload_data, "zone": 2}}
            })
            
        elif zone_name == 'lower':
            # Zona 1, Cajones 1, 2, 3
            # (Lógica inferida de tu patrón 'upper')
            # Cajón 3 -> drawer_3/command
            commands_to_send.append({
                "topic": "winefridge/drawer_3/command",
                "payload": {"action": "set_general_light", "data": drawer_payload_data}
            })
            # Cajón 1 -> lighting_2/command
            commands_to_send.append({
                "topic": "winefridge/lighting_2/command",
                "payload": {"action": "set_general_light", "data": {**multi_controller_payload_data, "drawer": 1}}
            })
            # Cajón 2 -> lighting_2/command
            commands_to_send.append({
                "topic": "winefridge/lighting_2/command",
                "payload": {"action": "set_general_light", "data": {**multi_controller_payload_data, "drawer": 2}}
            })
            # Zona 1 -> lighting_2/command
            commands_to_send.append({
                "topic": "winefridge/lighting_2/command",
                "payload": {"action": "set_zone_light", "data": {**multi_controller_payload_data, "zone": 1}}
            })
        
        else:
            print(f"[LIGHTING] ✗ Zona desconocida: {zone_name}")
            return

        # 4. Publicar todos los comandos
        for cmd in commands_to_send:
            # Añadir timestamp y source al payload final
            # (fuera del objeto 'data')
            final_payload = {
                **cmd["payload"], # Esto incluye 'action' y 'data'
                "timestamp": datetime.now().isoformat(),
                "source": "mqtt_handler"
            }
            
            message = json.dumps(final_payload)
            self.client.publish(cmd["topic"], message)
            print(f"[LIGHTING] → Enviado a {cmd['topic']}: {message}")
    
    # =========================================================================
    
    # MODIFIED: Ahora recibe el 'data' object y funciona
    def handle_zone_settings(self, data):
        """Maneja los ajustes de temperatura/humedad de la zona"""
        zone = data.get('zone')
        mode = data.get('mode')
        target_temp = data.get('target')
        humidity = data.get('humidity')
        
        print(f"[SETTINGS] Recibidos ajustes para zona {zone}: "
              f"Mode={mode}, Temp={target_temp}, Humidity={humidity}")
        #
        # ... Añade tu lógica aquí para controlar temperatura/humedad ...
        #
        
        # Ejemplo: Confirmar actualización al cliente web
        time.sleep(1) # Simulando trabajo
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "settings_updated",
            "source": "mqtt_handler",
            "timestamp": datetime.now().isoformat()
        }))
        print(f"[SETTINGS] ✔ Ajustes actualizados para {zone}")

    # MODIFIED: Ahora recibe el 'data' object y funciona
    def handle_fridge_lighting(self, data):
        """Maneja los modos de iluminación de toda la nevera"""
        energy_saving = data.get('energy_saving')
        night_mode = data.get('night_mode')
        
        print(f"[LIGHTING] Recibidos ajustes generales de luces: "
              f"Energy Saving={energy_saving}, Night Mode={night_mode}")
        #
        # ... Añade tu lógica aquí para controlar los modos generales ...
        #
        print(f"[LIGHTING] ✔ Modos de luz generales actualizados")


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

    # MODIFIED: Now receives the 'data' object directly
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

    # MODIFIED: Now receives the 'data' object directly
    def start_bottle_unload(self, data):
        barcode = data.get('barcode')
        name = data.get('name', 'Unknown Wine')
        print(f"\n[UNLOAD] ═══════════════════════════════")
        print(f"[UNLOAD] Starting: {name[:40]}")
        
        # Try to find bottle location
        bottle_location = None
        
        if data.get('position') and data.get('drawer_id'):
            drawer_id = data.get('drawer_id')
            position = data.get('position')
            bottle_location = (drawer_id, position)
            print(f"[UNLOAD] Manual selection: {drawer_id} position {position}")
        elif barcode:
            if data.get('drawer'):
                bottle_location = self.find_bottle_in_drawer(barcode, data.get('drawer'))
                if not bottle_location:
                    print(f"[UNLOAD] ✗ Not found in {data.get('drawer')}")
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
            else:
                bottle_location = self.find_bottle_in_inventory(barcode)
                if not bottle_location:
                    print(f"[UNLOAD] ✗ Not found in inventory")
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

        # Cancel timer if exists
        if 'timer' in self.swap_operations and self.swap_operations['timer']:
            self.swap_operations['timer'].cancel()
            print("[SWAP] Timer cancelled")

        # Turn off all LEDs
        for bottle_info in self.swap_operations.get('bottles_removed', []):
            drawer_id = bottle_info['drawer']
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []}, # Turn off LEDs
                "timestamp": datetime.now().isoformat()
            }))

        # Also clear LEDs for bottles_to_place
        for target_info in self.swap_operations.get('bottles_to_place', []):
            drawer_id = target_info.get('target_drawer')
            if drawer_id:
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

    def cancel_load(self, data):
        """Cancel an ongoing load operation"""
        barcode = data.get('barcode', 'unknown')
        print(f"\n[LOAD] ═══════════════════════════════")
        print(f"[LOAD] Cancelling load operation for {barcode}")

        # Find and cancel any pending load operation
        cancelled = False
        for op_id, op in list(self.pending_operations.items()):
            if op.get('type') == 'load':
                drawer_id = op.get('drawer')
                position = op.get('position')

                # Cancel timer
                if 'timer' in op:
                    op['timer'].cancel()

                # Turn off all LEDs for this operation
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": []},
                    "timestamp": datetime.now().isoformat()
                }))

                # Remove the pending operation
                del self.pending_operations[op_id]
                print(f"[LOAD] ✔ Cancelled operation for {drawer_id} position {position}")
                cancelled = True
                break

        if not cancelled:
            print(f"[LOAD] No pending load operation found")

        print(f"[LOAD] ═══════════════════════════════\n")

    def cancel_unload(self, data):
        """Cancel an ongoing unload operation"""
        print(f"\n[UNLOAD] ═══════════════════════════════")
        print(f"[UNLOAD] Cancelling unload operation")

        # Find and cancel any pending unload operation
        cancelled = False
        for op_id, op in list(self.pending_operations.items()):
            if op.get('type') == 'unload':
                drawer_id = op.get('drawer')
                position = op.get('position')

                # Cancel timer
                if 'timer' in op:
                    op['timer'].cancel()

                # Turn off all LEDs for this operation
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": []},
                    "timestamp": datetime.now().isoformat()
                }))

                # Remove the pending operation
                del self.pending_operations[op_id]
                print(f"[UNLOAD] ✔ Cancelled operation for {drawer_id} position {position}")
                cancelled = True
                break

        if not cancelled:
            print(f"[UNLOAD] No pending unload operation found")

        print(f"[UNLOAD] ═══════════════════════════════\n")

    def find_bottle_in_inventory(self, barcode):
        for drawer_id in FUNCTIONAL_DRAWERS:
            if drawer_id in self.inventory.get("drawers", {}):
                drawer_data = self.inventory["drawers"][drawer_id]
                for position, pos_data in drawer_data.get("positions", {}).items():
                    if pos_data.get("occupied") and pos_data.get("barcode") == barcode:
                        return drawer_id, int(position)
        return None, None

    def find_bottle_in_drawer(self, barcode, drawer_id):
        if drawer_id in self.inventory.get("drawers", {}):
            for position, pos_data in self.inventory["drawers"][drawer_id].get("positions", {}).items():
                if pos_data.get("occupied") and pos_data.get("barcode") == barcode:
                    return drawer_id, int(position)
        return None, None

    def handle_drawer_status(self, drawer_id, message):
        """Process bottle events only when there's an active operation"""
        data = message.get('data', {})
        event = data.get('event')
        position = data.get('position')
        weight = data.get('weight', 0)

        # Process SWAP operations (highest priority)
        if self.swap_operations.get('active'):
            self.handle_swap_event(drawer_id, position, event, weight)
            return

        # Check if there's any active LOAD/UNLOAD operation
        has_active_operation = len(self.pending_operations) > 0

        # If no active operation, ignore all bottle events (ESP32 might be re-detecting existing bottles)
        if not has_active_operation:
            return

        # Process LOAD/UNLOAD operations
        if event == 'placed':
            self.handle_bottle_placed(drawer_id, position, weight)
        elif event == 'removed':
            self.handle_bottle_removed(drawer_id, position)

    def handle_wrong_placement(self, drawer_id, message):
        """Handle wrong placement events only during active LOAD operations"""
        # Only process if there's an active operation
        if not self.pending_operations:
            return

        data = message.get('data', {})
        position = data.get('position')
        expected_position = data.get('expected_position')

        # Check if this position is already occupied in inventory
        # If so, ignore (ESP32 is detecting an existing bottle, not a wrong placement)
        if drawer_id in self.inventory.get("drawers", {}):
            positions = self.inventory["drawers"][drawer_id].get("positions", {})
            if str(position) in positions and positions[str(position)].get("occupied", False):
                return

        # Find the active LOAD operation for this drawer
        for op_id, op in list(self.pending_operations.items()):
            if op.get('type') == 'load' and op.get('drawer') == drawer_id and op.get('position') == expected_position:
                print(f"[LOAD] ✗ Wrong placement! Expected #{expected_position}, got #{position}")

                # Track wrong positions
                if 'wrong_positions' not in op:
                    op['wrong_positions'] = []
                if position not in op['wrong_positions']:
                    op['wrong_positions'].append(position)

                # Notify frontend
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

                # Update LEDs: GREEN BLINKING on correct position + RED SOLID on wrong positions
                led_positions = [{
                    "position": expected_position,
                    "color": "#00FF00",
                    "brightness": 100,
                    "blink": True
                }]

                for wrong_pos in op['wrong_positions']:
                    led_positions.append({
                        "position": wrong_pos,
                        "color": "#FF0000",
                        "brightness": 100,
                        "blink": False
                    })

                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": led_positions},
                    "timestamp": datetime.now().isoformat()
                }))

                print(f"[LOAD] → LED: Red solid at {position}, Green blinking at {expected_position}")
                break

    def handle_swap_event(self, drawer_id, position, event, weight):
        """Handle events during swap operation"""
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
                already_removed = any(b['drawer'] == drawer_id and b['position'] == position
                                      for b in self.swap_operations['bottles_removed'])

                if not already_removed:
                    self.swap_operations['bottles_removed'].append(bottle_info)
                    bottle_num = len(self.swap_operations['bottles_removed'])
                    print(f"[SWAP] Bottle {bottle_num} removed: {bottle_info['name'][:40]}")

                    # LED amarillo en posición de donde se retiró
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

                    # Cuando se retiran 2 botellas, preparar intercambio
                    if len(self.swap_operations['bottles_removed']) == 2:
                        print("[SWAP] Both bottles removed, ready for swap placement")
                        bottle1 = self.swap_operations['bottles_removed'][0]
                        bottle2 = self.swap_operations['bottles_removed'][1]

                        self.swap_operations['bottles_to_place'] = [
                            {'bottle': bottle1, 'target_drawer': bottle2['drawer'], 'target_position': bottle2['position']},
                            {'bottle': bottle2, 'target_drawer': bottle1['drawer'], 'target_position': bottle1['position']}
                        ]

                        # Inicializar tracking de posiciones incorrectas
                        self.swap_operations['wrong_positions'] = []

                        # Iniciar timer de 60 segundos
                        def swap_timeout():
                            print("[SWAP] ⏱ Timeout! Cancelling swap operation")
                            self.cancel_swap()
                            self.client.publish("winefridge/system/status", json.dumps({
                                "action": "swap_completed",
                                "source": "mqtt_handler",
                                "data": {"success": False, "close_screen": True},
                                "timestamp": datetime.now().isoformat()
                            }))

                        self.swap_operations['timer'] = threading.Timer(60.0, swap_timeout)
                        self.swap_operations['timer'].start()
                        print("[SWAP] ⏱ Timeout timer started (60s)")

                        # Actualizar LEDs: 1ª posición verde parpadeando, 2ª amarillo
                        target_1 = self.swap_operations['bottles_to_place'][0]
                        target_2 = self.swap_operations['bottles_to_place'][1]

                        if target_1['target_drawer'] == target_2['target_drawer']:
                            self.client.publish(f"winefridge/{target_1['target_drawer']}/command", json.dumps({
                                "action": "set_leds",
                                "source": "mqtt_handler",
                                "data": {"positions": [
                                    {"position": target_1['target_position'], "color": "#00FF00", "brightness": 100, "blink": True},
                                    {"position": target_2['target_position'], "color": "#FFFF00", "brightness": 100}
                                ]},
                                "timestamp": datetime.now().isoformat()
                            }))
                        else:
                            self.client.publish(f"winefridge/{target_1['target_drawer']}/command", json.dumps({
                                "action": "set_leds",
                                "source": "mqtt_handler",
                                "data": {"positions": [
                                    {"position": target_1['target_position'], "color": "#00FF00", "brightness": 100, "blink": True}
                                ]},
                                "timestamp": datetime.now().isoformat()
                            }))
                            self.client.publish(f"winefridge/{target_2['target_drawer']}/command", json.dumps({
                                "action": "set_leds",
                                "source": "mqtt_handler",
                                "data": {"positions": [
                                    {"position": target_2['target_position'], "color": "#FFFF00", "brightness": 100}
                                ]},
                                "timestamp": datetime.now().isoformat()
                            }))

        elif event == 'placed' and len(self.swap_operations['bottles_to_place']) > 0:
            # Verificar si es la posición correcta
            target_info = None
            target_idx = -1

            for i, target in enumerate(self.swap_operations['bottles_to_place']):
                if target['target_drawer'] == drawer_id and target['target_position'] == position:
                    target_info = target
                    target_idx = i
                    break

            if target_info:
                # Colocación correcta
                print(f"[SWAP] ✔ Placed: {target_info['bottle']['name'][:40]} in correct position")

                # Limpiar LEDs rojos de posiciones incorrectas si había
                wrong_positions = self.swap_operations.get('wrong_positions', [])
                if wrong_positions:
                    print(f"[SWAP] → Clearing red LEDs from wrong positions: {wrong_positions}")
                    for wrong_pos_info in wrong_positions:
                        self.client.publish(f"winefridge/{wrong_pos_info['drawer']}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": []},
                            "timestamp": datetime.now().isoformat()
                        }))
                    self.swap_operations['wrong_positions'] = []

                self.update_inventory(
                    drawer_id,
                    position,
                    target_info['bottle']['barcode'],
                    target_info['bottle']['name'],
                    weight
                )

                self.swap_operations['bottles_to_place'].pop(target_idx)

                # LED gris en posición correcta
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": [{"position": position, "color": "#808080", "brightness": 30}]},
                    "timestamp": datetime.now().isoformat()
                }))

                if len(self.swap_operations['bottles_to_place']) == 1:
                    # Queda 1 botella por colocar, LED verde parpadeando
                    remaining_target = self.swap_operations['bottles_to_place'][0]
                    print(f"[SWAP] Ready for final placement: {remaining_target['bottle']['name'][:40]}")

                    self.client.publish(f"winefridge/{remaining_target['target_drawer']}/command", json.dumps({
                        "action": "set_leds",
                        "source": "mqtt_handler",
                        "data": {"positions": [
                            {"position": remaining_target['target_position'], "color": "#00FF00", "brightness": 100, "blink": True}
                        ]},
                        "timestamp": datetime.now().isoformat()
                    }))

                elif len(self.swap_operations['bottles_to_place']) == 0:
                    # Swap completado
                    print("[SWAP] ✔ Swap complete!")

                    # Cancelar timer
                    if 'timer' in self.swap_operations and self.swap_operations['timer']:
                        self.swap_operations['timer'].cancel()

                    self.client.publish("winefridge/system/status", json.dumps({
                        "action": "swap_completed",
                        "source": "mqtt_handler",
                        "data": {"success": True},
                        "timestamp": datetime.now().isoformat()
                    }))

                    self.swap_operations = {'active': False, 'bottles_removed': [], 'bottles_to_place': [], 'start_time': None}

                    def turn_off_led():
                        time.sleep(1)
                        self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                            "action": "set_leds",
                            "source": "mqtt_handler",
                            "data": {"positions": []},
                            "timestamp": datetime.now().isoformat()
                        }))
                    threading.Thread(target=turn_off_led).start()
            else:
                # Colocación incorrecta - detectar posiciones esperadas
                expected_positions = [t['target_position'] for t in self.swap_operations['bottles_to_place'] if t['target_drawer'] == drawer_id]
                if expected_positions:
                    print(f"[SWAP] ✗ Wrong placement! Expected positions: {expected_positions}, got {position}")

                    # Registrar posición incorrecta
                    wrong_pos_info = {'drawer': drawer_id, 'position': position}
                    if 'wrong_positions' not in self.swap_operations:
                        self.swap_operations['wrong_positions'] = []
                    if wrong_pos_info not in self.swap_operations['wrong_positions']:
                        self.swap_operations['wrong_positions'].append(wrong_pos_info)

                    # Notificar frontend
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

                    # Actualizar LEDs: rojo fijo en posición incorrecta + verde parpadeando en correcta
                    led_positions = []

                    # LED verde parpadeando en posiciones esperadas
                    for exp_pos in expected_positions:
                        led_positions.append({
                            "position": exp_pos,
                            "color": "#00FF00",
                            "brightness": 100,
                            "blink": True
                        })

                    # LED rojo fijo en posiciones incorrectas
                    for wrong_pos in self.swap_operations.get('wrong_positions', []):
                        if wrong_pos['drawer'] == drawer_id:
                            led_positions.append({
                                "position": wrong_pos['position'],
                                "color": "#FF0000",
                                "brightness": 100,
                                "blink": False
                            })

                    self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                        "action": "set_leds",
                        "source": "mqtt_handler",
                        "data": {"positions": led_positions},
                        "timestamp": datetime.now().isoformat()
                    }))

                    print(f"[SWAP] → LED: Red solid at position {position}, Green blinking at {expected_positions}")

        elif event == 'removed' and len(self.swap_operations['bottles_to_place']) > 0:
            # Botella levantada durante swap - verificar si es de posición incorrecta
            wrong_positions = self.swap_operations.get('wrong_positions', [])
            wrong_pos_info = next((wp for wp in wrong_positions if wp['drawer'] == drawer_id and wp['position'] == position), None)

            if wrong_pos_info:
                # Botella levantada de posición incorrecta - limpiar LED rojo
                print(f"[SWAP] → Bottle removed from wrong position {position}, clearing red LED")
                wrong_positions.remove(wrong_pos_info)

                # Actualizar LEDs: mantener verde parpadeando en posiciones correctas, quitar rojo de esta posición
                expected_positions = [t['target_position'] for t in self.swap_operations['bottles_to_place'] if t['target_drawer'] == drawer_id]
                led_positions = []

                # LED verde parpadeando en posiciones esperadas
                for exp_pos in expected_positions:
                    led_positions.append({
                        "position": exp_pos,
                        "color": "#00FF00",
                        "brightness": 100,
                        "blink": True
                    })

                # LED rojo en posiciones incorrectas restantes
                for wrong_pos in wrong_positions:
                    if wrong_pos['drawer'] == drawer_id:
                        led_positions.append({
                            "position": wrong_pos['position'],
                            "color": "#FF0000",
                            "brightness": 100,
                            "blink": False
                        })

                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": led_positions},
                    "timestamp": datetime.now().isoformat()
                }))

    def handle_bottle_placed(self, drawer_id, position, weight):
        """Handle 'placed' event during active LOAD operations"""
        op_id, op = self.find_pending_op(drawer_id, position)

        if op and op['type'] == 'load':
            print(f"[LOAD] ✔ Bottle placed in correct slot")

            op['timer'].cancel()
            self.update_inventory(drawer_id, position, op['barcode'], op['name'], weight)

            self.client.publish("winefridge/system/status", json.dumps({
                "action": "bottle_placed",
                "source": "mqtt_handler",
                "data": {
                    "success": True,
                    "drawer": drawer_id,
                    "position": position
                },
                "timestamp": datetime.now().isoformat()
            }))

            # Turn off all wrong position LEDs (if any) and set correct position to gray
            wrong_positions = op.get('wrong_positions', [])
            led_positions = []

            # Clear wrong positions
            for wrong_pos in wrong_positions:
                led_positions.append({
                    "position": wrong_pos,
                    "color": "#000000",
                    "brightness": 0
                })

            # Set correct position to gray
            led_positions.append({
                "position": position,
                "color": "#808080",
                "brightness": 30
            })

            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": led_positions},
                "timestamp": datetime.now().isoformat()
            }))

            if wrong_positions:
                print(f"[LOAD] → Cleared red LEDs from wrong positions: {wrong_positions}")

            del self.pending_operations[op_id]

            # Fade out LEDs after 2 seconds
            def fade_out():
                time.sleep(2)
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": []},
                    "timestamp": datetime.now().isoformat()
                }))
            threading.Thread(target=fade_out).start()
            return

        # Case 2: Bottle placed back in wrong position during UNLOAD operation
        for existing_op_id, existing_op in list(self.pending_operations.items()):
            if existing_op['type'] == 'unload' and existing_op['drawer'] == drawer_id:
                wrong_positions = existing_op.get('wrong_positions', [])
                if position in wrong_positions:
                    print(f"[UNLOAD] → Bottle placed back in wrong position {position}, clearing red LED")
                    wrong_positions.remove(position)

                    # Notify frontend to close error modal
                    self.client.publish("winefridge/system/status", json.dumps({
                        "action": "wrong_bottle_replaced",
                        "source": "mqtt_handler",
                        "data": {
                            "drawer": drawer_id,
                            "position": position
                        },
                        "timestamp": datetime.now().isoformat()
                    }))

                    # Update LEDs: GREEN BLINKING on correct + GRAY on replaced + RED SOLID on remaining wrong positions
                    led_positions = [{
                        "position": existing_op['position'],
                        "color": "#00FF00",
                        "brightness": 100,
                        "blink": True
                    }]

                    # Set gray LED on the position where bottle was placed back
                    led_positions.append({
                        "position": position,
                        "color": "#808080",
                        "brightness": 30,
                        "blink": False
                    })

                    # Red LEDs on remaining wrong positions
                    for wrong_pos in wrong_positions:
                        led_positions.append({
                            "position": wrong_pos,
                            "color": "#FF0000",
                            "brightness": 100,
                            "blink": False
                        })

                    self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                        "action": "set_leds",
                        "source": "mqtt_handler",
                        "data": {"positions": led_positions},
                        "timestamp": datetime.now().isoformat()
                    }))
                    return

    def handle_bottle_removed(self, drawer_id, position):
        """Handle 'removed' event during active UNLOAD/LOAD operations"""
        op_id, op = self.find_pending_op(drawer_id, position)

        # Case 1: Correct bottle removed during UNLOAD operation
        if op and op['type'] == 'unload':
            print(f"[UNLOAD] ✔ Bottle removed from correct slot")

            op['timer'].cancel()
            self.update_inventory(drawer_id, position, None, None, 0, occupied=False)

            self.client.publish("winefridge/system/status", json.dumps({
                "action": "bottle_unloaded",
                "source": "mqtt_handler",
                "data": {
                    "success": True,
                    "drawer": drawer_id,
                    "position": position
                },
                "timestamp": datetime.now().isoformat()
            }))

            # First, explicitly turn off wrong position LEDs (if any)
            wrong_positions = op.get('wrong_positions', [])
            led_positions = []

            # Clear wrong positions explicitly with brightness=0
            for wrong_pos in wrong_positions:
                led_positions.append({
                    "position": wrong_pos,
                    "color": "#000000",
                    "brightness": 0
                })

            if led_positions:
                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": led_positions},
                    "timestamp": datetime.now().isoformat()
                }))
                print(f"[UNLOAD] → Cleared red LEDs from wrong positions: {wrong_positions}")

            # Then turn off all LEDs
            self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))

            del self.pending_operations[op_id]
            return

        # Case 2: Wrong bottle removed during UNLOAD operation
        for existing_op_id, existing_op in self.pending_operations.items():
            if existing_op['type'] == 'unload' and existing_op['drawer'] == drawer_id:
                print(f"[UNLOAD] ✗ Wrong bottle removed! Expected {existing_op['position']}, got {position}")

                # Track wrong positions
                if 'wrong_positions' not in existing_op:
                    existing_op['wrong_positions'] = []
                if position not in existing_op['wrong_positions']:
                    existing_op['wrong_positions'].append(position)

                # Notify frontend
                self.client.publish("winefridge/system/status", json.dumps({
                    "action": "wrong_bottle_removed",
                    "source": "mqtt_handler",
                    "data": {
                        "drawer": drawer_id,
                        "position": position,
                        "expected_position": existing_op['position']
                    },
                    "timestamp": datetime.now().isoformat()
                }))

                # Update LEDs: GREEN BLINKING on correct + RED SOLID on wrong positions
                led_positions = [{
                    "position": existing_op['position'],
                    "color": "#00FF00",
                    "brightness": 100,
                    "blink": True
                }]

                for wrong_pos in existing_op['wrong_positions']:
                    led_positions.append({
                        "position": wrong_pos,
                        "color": "#FF0000",
                        "brightness": 100,
                        "blink": False
                    })

                self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                    "action": "set_leds",
                    "source": "mqtt_handler",
                    "data": {"positions": led_positions},
                    "timestamp": datetime.now().isoformat()
                }))

                print(f"[UNLOAD] → LED: Red solid at {position}, Green blinking at {existing_op['position']}")
                return

        # Case 3: Bottle removed from wrong position during LOAD operation
        for existing_op_id, existing_op in list(self.pending_operations.items()):
            if existing_op['type'] == 'load' and existing_op['drawer'] == drawer_id:
                wrong_positions = existing_op.get('wrong_positions', [])
                if position in wrong_positions:
                    print(f"[LOAD] → Bottle removed from wrong position {position}, clearing red LED")
                    wrong_positions.remove(position)

                    # Update LEDs: GREEN BLINKING on correct + RED SOLID on remaining wrong positions
                    led_positions = [{
                        "position": existing_op['position'],
                        "color": "#00FF00",
                        "brightness": 100,
                        "blink": True
                    }]

                    for wrong_pos in wrong_positions:
                        led_positions.append({
                            "position": wrong_pos,
                            "color": "#FF0000",
                            "brightness": 100,
                            "blink": False
                        })

                    self.client.publish(f"winefridge/{drawer_id}/command", json.dumps({
                        "action": "set_leds",
                        "source": "mqtt_handler",
                        "data": {"positions": led_positions},
                        "timestamp": datetime.now().isoformat()
                    }))
                    return

    def find_pending_op(self, drawer_id, position):
        """Find a pending operation matching the event location"""
        for op_id, op in self.pending_operations.items():
            if op.get('drawer') == drawer_id and op.get('expected_position') == position:
                return op_id, op
        return None, None

    def handle_timeout(self, op_id):
        """Handle timeout for a pending operation"""
        if op_id in self.pending_operations:
            op = self.pending_operations[op_id]
            op_type = op.get('type')
            drawer = op.get('drawer')
            position = op.get('position')
            
            print(f"[{op_type.upper()}] ✗ Timeout for {drawer} pos {position}")
            
            self.client.publish(f"winefridge/{drawer}/command", json.dumps({
                "action": "set_leds",
                "source": "mqtt_handler",
                "data": {"positions": []},
                "timestamp": datetime.now().isoformat()
            }))
            
            self.client.publish("winefridge/system/status", json.dumps({
                "action": f"{op_type}_timeout",
                "source": "mqtt_handler",
                "data": {"drawer": drawer, "position": position},
                "timestamp": datetime.now().isoformat()
            }))
            
            del self.pending_operations[op_id]

    def update_inventory(self, drawer_id, position, barcode, name, weight, occupied=True):
        """Update the inventory.json file"""
        print(f"[DB] Updating {drawer_id} pos {position}...")
        
        if "drawers" not in self.inventory:
            self.inventory["drawers"] = {}
        if drawer_id not in self.inventory["drawers"]:
            self.inventory["drawers"][drawer_id] = {"positions": {}}
        if "positions" not in self.inventory["drawers"][drawer_id]:
            self.inventory["drawers"][drawer_id]["positions"] = {}
            
        position_str = str(position)
        
        if occupied:
            percentage = self.calculate_bottle_percentage(weight)
            self.inventory["drawers"][drawer_id]["positions"][position_str] = {
                "occupied": True,
                "barcode": barcode,
                "name": name,
                "weight": weight,
                "percentage": percentage,
                "last_update": datetime.now().isoformat()
            }
            print(f"[DB] ✔ Occupied by {name[:30]} ({weight}g, {percentage}%)")
        else:
            if position_str in self.inventory["drawers"][drawer_id]["positions"]:
                self.inventory["drawers"][drawer_id]["positions"][position_str] = {
                    "occupied": False
                }
                print(f"[DB] ✔ Emptied")
            
        self.save_json('/home/plasticlab/WineFridge/RPI/database/inventory.json', self.inventory)
        
        self.client.publish("winefridge/system/status", json.dumps({
            "action": "inventory_updated",
            "source": "mqtt_handler",
            "timestamp": datetime.now().isoformat()
        }))

    # MODIFIED: Now receives the 'data' object directly
    def retry_placement(self, data):
        drawer_id = data.get('drawer_id')
        position = data.get('position')
        print(f"[RETRY] Retrying placement for {drawer_id} pos {position}")
        # Logic to handle retry

    # MODIFIED: Now receives the 'data' object directly
    def complete_load_operation(self, data):
        op_id = data.get('op_id')
        print(f"[LOAD] Force complete for op {op_id}")
        if op_id in self.pending_operations:
            op = self.pending_operations[op_id]
            op['timer'].cancel()
            
            if op['type'] == 'load':
                self.update_inventory(
                    op['drawer'], 
                    op['position'], 
                    op['barcode'], 
                    op['name'],
                    MIN_FULL_BOTTLE_WEIGHT
                )
            
            del self.pending_operations[op_id]
        
    def run(self):
        try:
            print("[MQTT] Starting MQTT loop...")
            self.client.loop_forever()
        except KeyboardInterrupt:
            print("\n[MQTT] Shutting down...")
            self.running = False
            if self.serial:
                self.serial.close()
            self.client.disconnect()
            print("[MQTT] Done")

if __name__ == "__main__":
    controller = WineFridgeController()
    controller.run()
