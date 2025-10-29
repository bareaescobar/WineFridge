#!/usr/bin/env python3
import serial
import paho.mqtt.client as mqtt
import json
from datetime import datetime
import time

class BarcodeScanner:
    def __init__(self):
        self.serial_port = '/dev/ttyAMA0'
        self.baud_rate = 115200
        
        # MQTT setup with new API
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        self.client.connect("localhost", 1883, 60)
        
        self.buffer = ""
        self.last_barcode = None
        self.last_scan_time = 0
        
        print(f"[SCANNER] Starting on {self.serial_port}")
        
    def run(self):
        try:
            ser = serial.Serial(
                port=self.serial_port,
                baudrate=self.baud_rate,
                timeout=0.1
            )
            
            print("[SCANNER] Connected to serial port")
            
            while True:
                if ser.in_waiting > 0:
                    data = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    self.buffer += data
                    
                    if '\n' in self.buffer:
                        lines = self.buffer.split('\n')
                        for line in lines[:-1]:
                            barcode = line.strip()
                            if barcode and len(barcode) > 5:
                                # Avoid duplicate scans within 2 seconds
                                current_time = time.time()
                                if barcode != self.last_barcode or (current_time - self.last_scan_time) > 2:
                                    self.publish_barcode(barcode)
                                    self.last_barcode = barcode
                                    self.last_scan_time = current_time
                        
                        self.buffer = lines[-1]
                
                time.sleep(0.01)
                
        except serial.SerialException as e:
            print(f"[ERROR] Serial port: {e}")
            time.sleep(5)
            self.run()
            
        except KeyboardInterrupt:
            print("\n[STOP] Scanner shutting down...")
            
        except Exception as e:
            print(f"[ERROR] Unexpected: {e}")
    
    def publish_barcode(self, barcode):
        message = {
            "action": "barcode_scanned",
            "source": "barcode_scanner",
            "data": {
                "barcode": barcode
            },
            "timestamp": datetime.now().isoformat()
        }
        
        self.client.publish("winefridge/system/status", json.dumps(message))
        print(f"[SCAN] Published: {barcode}")

if __name__ == "__main__":
    scanner = BarcodeScanner()
    scanner.run()
