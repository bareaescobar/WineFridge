import serial
import paho.mqtt.client as mqtt
import json
from datetime import datetime
import time
import re

def find_serial_port():
    """Detectar automáticamente el puerto serie en RPI5"""
    import os
    
    # Puertos típicos en RPI5
    possible_ports = ['/dev/ttyAMA0', '/dev/serial0', '/dev/ttyAMA1']
    
    for port in possible_ports:
        if os.path.exists(port):
            try:
                ser = serial.Serial(port, 115200, timeout=0.1)
                ser.close()
                return port
            except:
                continue
    return '/dev/ttyAMA0'  # Fallback al puerto detectado

class BarcodeScanner:
    def __init__(self):
        # Detectar puerto automáticamente
        port = find_serial_port()
        print(f"[Scanner] Usando puerto: {port}")
        
        try:
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
            print(f"[Scanner] Puerto serie {port} abierto correctamente")
        except Exception as e:
            print(f"[Scanner] Error abriendo puerto {port}: {e}")
            raise

        # Setup MQTT
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION1)
        try:
            self.client.connect("localhost", 1883)
            print("[Scanner] Conectado a MQTT broker")
        except Exception as e:
            print(f"[Scanner] Error conectando a MQTT: {e}")
            raise

        self.buffer = ""
        self.last_barcode = ""
        self.last_scan_time = 0
        self.scan_cooldown = 2.0  # Evitar escaneos duplicados en 2 segundos

    def is_valid_barcode(self, code):
        """Verificar si un código parece válido"""
        # Eliminar espacios y caracteres no imprimibles
        code = code.strip()
        
        # Debe tener al menos 8 caracteres y máximo 20
        if len(code) < 8 or len(code) > 20:
            return False
            
        # Debe contener solo números y/o letras (códigos alfanuméricos válidos)
        if not re.match(r'^[A-Za-z0-9]+$', code):
            return False
            
        return True

    def process_data(self, data):
        """Procesar datos recibidos del escáner"""
        try:
            # Decodificar datos
            decoded = data.decode('utf-8', errors='ignore')
            self.buffer += decoded
            
            # Este escáner no envía saltos de línea, así que procesamos cuando:
            # 1. Recibimos suficientes datos (8-20 caracteres)
            # 2. Hay una pausa en la transmisión
            
            # Si el buffer tiene datos y parece un código completo
            cleaned_buffer = self.buffer.strip()
            
            if self.is_valid_barcode(cleaned_buffer):
                current_time = time.time()
                
                # Evitar duplicados (mismo código en menos de 2 segundos)
                if (cleaned_buffer != self.last_barcode or 
                    current_time - self.last_scan_time > self.scan_cooldown):
                    
                    print(f"[Scanner] Código detectado: {cleaned_buffer}")
                    self.publish_barcode(cleaned_buffer)
                    
                    # Actualizar variables de control
                    self.last_barcode = cleaned_buffer
                    self.last_scan_time = current_time
                
                # Limpiar buffer después de procesar
                self.buffer = ""
                
        except Exception as e:
            print(f"[Scanner] Error procesando datos: {e}")
            self.buffer = ""  # Limpiar buffer en caso de error

    def run(self):
        print("[Scanner] Barcode scanner ejecutándose...")
        print("[Scanner] Listo para escanear códigos")

        last_data_time = 0
        
        while True:
            try:
                if self.serial.in_waiting > 0:
                    # Leer todos los datos disponibles
                    data = self.serial.read(self.serial.in_waiting)
                    last_data_time = time.time()
                    
                    print(f"[Scanner] Datos recibidos: {data}")
                    self.process_data(data)
                
                else:
                    # Si no hay datos nuevos pero tenemos buffer pendiente
                    current_time = time.time()
                    if (self.buffer and 
                        current_time - last_data_time > 0.5 and  # 500ms sin nuevos datos
                        self.is_valid_barcode(self.buffer.strip())):
                        
                        cleaned_buffer = self.buffer.strip()
                        
                        # Verificar duplicados
                        if (cleaned_buffer != self.last_barcode or 
                            current_time - self.last_scan_time > self.scan_cooldown):
                            
                            print(f"[Scanner] Código detectado (timeout): {cleaned_buffer}")
                            self.publish_barcode(cleaned_buffer)
                            
                            self.last_barcode = cleaned_buffer
                            self.last_scan_time = current_time
                        
                        self.buffer = ""
                
                time.sleep(0.05)  # Pequeña pausa para no saturar la CPU
                
            except Exception as e:
                print(f"[Scanner] Error en bucle principal: {e}")
                time.sleep(1)  # Pausa más larga si hay error

    def publish_barcode(self, barcode):
        """Publicar código de barras a MQTT"""
        try:
            message = {
                "action": "barcode_scanned",
                "source": "barcode_scanner",
                "data": {
                    "barcode": barcode
                },
                "timestamp": datetime.now().isoformat()
            }

            self.client.publish("winefridge/system/status", json.dumps(message))
            print(f"[Scanner] Publicado código: {barcode}")
            
        except Exception as e:
            print(f"[Scanner] Error publicando a MQTT: {e}")

    def __del__(self):
        """Limpiar recursos al finalizar"""
        try:
            if hasattr(self, 'serial') and self.serial.is_open:
                self.serial.close()
                print("[Scanner] Puerto serie cerrado")
        except:
            pass
            
        try:
            if hasattr(self, 'client'):
                self.client.disconnect()
                print("[Scanner] MQTT desconectado")
        except:
            pass

if __name__ == "__main__":
    try:
        scanner = BarcodeScanner()
        scanner.run()
    except KeyboardInterrupt:
        print("\n[Scanner] Detenido por usuario")
    except Exception as e:
        print(f"[Scanner] Error fatal: {e}")
        import traceback
        traceback.print_exc()
