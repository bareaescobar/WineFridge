#!/usr/bin/env python3
import serial
import time
import os
import glob

def get_rpi5_uart_ports():
    """Obtener todos los puertos UART posibles en RPI5"""
    ports = []
    
    # Puertos típicos en RPI5
    possible_ports = [
        '/dev/serial0',    # Enlace simbólico al UART principal
        '/dev/serial1',    # Enlace simbólico al UART secundario
        '/dev/ttyAMA0',    # UART0 físico
        '/dev/ttyAMA1',    # UART1 físico
        '/dev/ttyAMA2',    # UART2 físico
        '/dev/ttyAMA3',    # UART3 físico
        '/dev/ttyAMA4',    # UART4 físico
        '/dev/ttyS0',      # UART serie tradicional
        '/dev/ttyAMA10',      # UART??

    ]
    
    # Verificar qué puertos existen realmente
    for port in possible_ports:
        if os.path.exists(port):
            try:
                # Verificar que sea un dispositivo de caracteres
                stat = os.stat(port)
                if os.path.stat.S_ISCHR(stat.st_mode):
                    ports.append(port)
                    print(f"✅ Puerto encontrado: {port}")
                else:
                    print(f"⚠️  Existe pero no es dispositivo serie: {port}")
            except OSError as e:
                print(f"❌ Error accediendo {port}: {e}")
        else:
            print(f"❌ No existe: {port}")
    
    return ports

def show_port_info(port):
    """Mostrar información sobre un puerto serie"""
    try:
        # Mostrar información del enlace simbólico si aplica
        if os.path.islink(port):
            real_path = os.path.realpath(port)
            print(f"🔗 {port} -> {real_path}")
        else:
            print(f"📍 Puerto físico: {port}")
            
        # Mostrar permisos
        stat = os.stat(port)
        print(f"👤 Permisos: {oct(stat.st_mode)[-3:]}")
        
    except Exception as e:
        print(f"❌ Error obteniendo info de {port}: {e}")

def test_port_for_scanner(port, duration=10):
    """Probar un puerto específico para detectar datos del escáner"""
    print(f"\n🔍 Probando {port}")
    show_port_info(port)
    print(f"⏱️  Escuchando {duration} segundos...")
    print("📱 ¡ESCANEA UN CÓDIGO DE BARRAS AHORA!")
    print("-" * 40)
    
    try:
        # Configuración optimizada para escáneres de códigos de barras
        ser = serial.Serial(
            port=port,
            baudrate=115200,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.5,
            xonxoff=False,
            rtscts=False,
            dsrdtr=False
        )
        
        print(f"✅ Puerto {port} abierto correctamente")
        
        buffer = ""
        data_received = False
        start_time = time.time()
        
        while (time.time() - start_time) < duration:
            # Mostrar countdown
            remaining = int(duration - (time.time() - start_time))
            print(f"⏳ {remaining}s ", end='', flush=True)
            
            if ser.in_waiting > 0:
                try:
                    # Leer datos disponibles
                    data = ser.read(ser.in_waiting)
                    print(f"\n📥 ¡DATOS DETECTADOS!")
                    print(f"📥 RAW bytes: {data}")
                    print(f"📥 HEX: {data.hex()}")
                    
                    # Intentar decodificar como texto
                    try:
                        decoded = data.decode('utf-8', errors='replace')
                        buffer += decoded
                        print(f"📥 Texto: {repr(decoded)}")
                        
                        # Procesar líneas completas
                        if '\n' in buffer or '\r' in buffer:
                            lines = buffer.replace('\r', '\n').split('\n')
                            for line in lines[:-1]:
                                if line.strip():
                                    print(f"🎯 CÓDIGO ESCANEADO: '{line.strip()}'")
                                    data_received = True
                            buffer = lines[-1]
                    except:
                        print(f"📥 Datos binarios: {[f'0x{b:02x}' for b in data]}")
                        data_received = True
                        
                except Exception as e:
                    print(f"\n❌ Error leyendo datos: {e}")
            
            time.sleep(0.2)
        
        ser.close()
        
        if data_received:
            print(f"\n🎉 ¡ESCÁNER DETECTADO EN {port}!")
            return True
        else:
            print(f"\n😔 No se detectaron datos en {port}")
            return False
            
    except serial.SerialException as e:
        print(f"\n❌ Error de puerto serie: {e}")
        return False
    except PermissionError:
        print(f"\n❌ Sin permisos para acceder a {port}")
        print("💡 Ejecuta: sudo usermod -a -G dialout $USER")
        print("💡 Luego cierra sesión y vuelve a entrar")
        return False
    except Exception as e:
        print(f"\n❌ Error inesperado: {e}")
        return False

def main():
    print("🍇 DETECTOR DE PUERTO DEL ESCÁNER - RPI5")
    print("=" * 50)
    
    # Paso 1: Encontrar puertos disponibles
    print("\n📍 Paso 1: Buscando puertos UART en RPI5...")
    ports = get_rpi5_uart_ports()
    
    if not ports:
        print("\n❌ No se encontraron puertos UART")
        print("🔧 Verificar:")
        print("   - dtparam=uart0=on en /boot/firmware/config.txt")
        print("   - Reiniciar después de cambios")
        return
    
    print(f"\n✅ Encontrados {len(ports)} puerto(s) UART")
    
    # Paso 2: Probar cada puerto
    print("\n📍 Paso 2: Probando cada puerto...")
    
    working_ports = []
    
    for i, port in enumerate(ports, 1):
        print(f"\n{'='*20} PUERTO {i}/{len(ports)} {'='*20}")
        
        if test_port_for_scanner(port, 10):
            working_ports.append(port)
            
            # Preguntar si continuar probando otros puertos
            if i < len(ports):
                continue_test = input(f"\n¿Probar otros puertos? (y/n): ").lower().strip()
                if continue_test != 'y':
                    break
    
    # Paso 3: Mostrar resultados
    print("\n" + "="*50)
    print("📋 RESULTADOS:")
    
    if working_ports:
        print(f"🎉 ¡Escáner detectado en: {working_ports}")
        
        # Mostrar código para actualizar barcode_scanner.py
        best_port = working_ports[0]
        print(f"\n💡 ACTUALIZAR barcode_scanner.py:")
        print(f"   Cambiar la línea del puerto serie por:")
        print(f"   port='{best_port}',")
        
        # Generar código de actualización
        print(f"\n🔧 Comando para actualizar automáticamente:")
        print(f"sed -i \"s|port='/dev/.*',|port='{best_port}',|g\" barcode_scanner.py")
        
    else:
        print("😔 No se detectó el escáner en ningún puerto")
        print("\n🔧 Troubleshooting:")
        print("   - Verificar conexión física (TX<->RX cruzados)")
        print("   - Verificar alimentación del escáner")
        print("   - Probar velocidades diferentes (9600, 38400 baud)")
        print("   - Verificar configuración del escáner")
        
        # Mostrar información adicional de debug
        print(f"\n🔍 Debug info:")
        print(f"   Usuario actual: {os.getenv('USER')}")
        print(f"   Grupos: ", end="")
        try:
            import grp
            groups = [g.gr_name for g in grp.getgrall() if os.getenv('USER') in g.gr_mem]
            print(", ".join(groups))
        except:
            print("No se pudo obtener")

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n👋 Detección interrumpida por el usuario")
