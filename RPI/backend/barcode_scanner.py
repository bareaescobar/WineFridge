import serial
import paho.mqtt.client as mqtt
import json
from datetime import datetime
import time

def find_serial_port():
    import glob
    ports = glob.glob('/dev/ttyUSB*') + glob.glob('/dev/ttyAMA*') + glob.glob('/dev/ttyS*')
    for port in ports:
        try:
            ser = serial.Serial(port, 115200, timeout=0.1)
            ser.close()
            return port
        except:
            continue
    return None

class BarcodeScanner:
    def __init__(self):
        port = find_serial_port() or '/dev/ttyAMA0'
        self.serial = serial.Serial(
            port=port,
            baudrate=115200,
            timeout=0.1
        )

        # Configure serial port (adjust as needed)
        self.serial = serial.Serial(
            port='/dev/'ttyAMA0,      # or '/dev/ttyS0' depending on your Pi model
            baudrate=115200,
            timeout=0.1
        )

        # Setup MQTT
        self.client = mqtt.Client()
        self.client.connect("localhost", 1883)

        self.buffer = ""

    def run(self):
        print("[Scanner] Barcode scanner running...")

        while True:
            if self.serial.in_waiting > 0:
                data = self.serial.read(self.serial.in_waiting).decode('utf-8', errors='ignore')
                self.buffer += data

                if '\n' in self.buffer:
                    lines = self.buffer.split('\n')
                    for line in lines[:-1]:
                        barcode = line.strip()
                        if barcode:
                            self.publish_barcode(barcode)
                    self.buffer = lines[-1]
            time.sleep(0.05)

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
        print(f"[Scanner] Scanned barcode: {barcode}")

if __name__ == "__main__":
    scanner = BarcodeScanner()
    scanner.run()
