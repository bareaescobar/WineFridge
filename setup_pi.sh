#!/bin/bash

# --- Configuración ---
TARGET_USER="plasticlab"
# ---------------------

# 1. Asegurarse de que el script se ejecuta como root (con sudo)
if [ "$EUID" -ne 0 ]; then
  echo "Por favor, ejecuta este script con sudo: sudo ./setup_pi.sh"
  exit 1
fi

# 2. Parar el script si cualquier comando falla
set -e

# 3. Definir directorios
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
USER_HOME=$(eval echo "~$TARGET_USER")

# 4. Actualizar e instalar paquetes del sistema (APT)
echo "--- 1. Actualizando e instalando paquetes APT ---"
apt update
apt upgrade -y
# Paquetes: Node, MQTT, Git, Python y curl (para arduino-cli)
apt install nodejs npm mosquitto mosquitto-clients git python3-paho-mqtt python3-serial curl -y

# 5. Instalar paquetes globales de NPM (PM2)
echo "--- 2. Instalando PM2 globalmente ---"
npm install -g pm2

# 6. Activar las interfaces de hardware (raspi-config)
echo "--- 3. Activando VNC, SPI, I2C, Serial y 1-Wire ---"
raspi-config nonint do_vnc 0
raspi-config nonint do_spi 0
raspi-config nonint do_i2c 0
raspi-config nonint do_onewire 0
raspi-config nonint do_serial_hw 0

# 7. Configurando /boot/firmware/config.txt (Límite USB)
echo "--- 4. Configurando /boot/firmware/config.txt ---"
CONFIG_FILE="/boot/firmware/config.txt"

update_config() {
  PARAM=$1
  VALUE=$2
  if grep -q "^$PARAM=" "$CONFIG_FILE"; then
    sed -i "s|^$PARAM=.*|$PARAM=$VALUE|" "$CONFIG_FILE"
    echo "$PARAM actualizado a $VALUE."
  else
    echo "$PARAM=$VALUE" >> "$CONFIG_FILE"
    echo "$PARAM añadido."
  fi
}
update_config "PSU_MAX_CURRENT" "5000"
update_config "usb_max_current_enable" "1"

# 8. Configurar el modo Multitouch (labwc)
echo "--- 5. Configurando Multitouch para '$TARGET_USER' ---"
LABWC_CONFIG="$USER_HOME/.config/labwc/rc.xml"

if [ -f "$LABWC_CONFIG" ]; then
    sudo -u $TARGET_USER sed -i '/mapToOutput="DSI-2"/ s/mouseEmulation="yes"/mouseEmulation="no"/g' "$LABWC_CONFIG"
    echo "Modo Multitouch (mouseEmulation=no) activado para DSI-2."
else
    echo "AVISO: No se encontró $LABWC_CONFIG. Inicia sesión gráficamente al menos una vez."
fi

# 9. Activar Auto-Login gráfico (B4)
echo "--- 6. Configurando Auto-Login para el escritorio ---"
raspi-config nonint do_boot_behaviour B4

# 10. Instalar Raspberry Pi Connect
echo "--- 7. Instalando Raspberry Pi Connect ---"
apt install rpi-connect -y

# 11. Configurar Mosquitto (copiando archivo)
echo "--- 8. Configurando Mosquitto ---"
sudo mkdir -p /etc/mosquitto/conf.d
sudo cp "$SCRIPT_DIR/RPI/mosquitto_winefridge.conf" /etc/mosquitto/conf.d/winefridge.conf
echo "Archivo de configuración de Mosquitto copiado."
sudo systemctl restart mosquitto
sudo systemctl enable mosquitto

# 12. Construir el Frontend
echo "--- 9. Construyendo el Frontend (npm build) ---"
sudo -u $TARGET_USER bash -c "cd '$SCRIPT_DIR/RPI/frontend' && npm install && npm run build"

# 13. Iniciar el Backend con PM2
echo "--- 10. Iniciando el Backend con PM2 ---"
sudo -u $TARGET_USER bash -c "cd '$SCRIPT_DIR/RPI' && pm2 start pm2.config.js"

# 14. Guardar la lista de procesos de PM2
echo "--- 11. Guardando la lista de procesos PM2 ---"
sudo -u $TARGET_USER pm2 save

# 15. Automatizar PM2 startup
echo "--- 12. Automatizando PM2 startup al arranque ---"
STARTUP_CMD=$(sudo -u $TARGET_USER pm2 startup | tail -n 1)
eval $STARTUP_CMD
echo "Servicio de arranque de PM2 creado."

# 16. Configurar Arduino-CLI y permisos de serie
echo "--- 13. Configurando Arduino-CLI y permisos ---"

# Añadir usuario al grupo 'dialout' para permisos de puerto serie (Arduino/ESP32)
usermod -aG dialout $TARGET_USER
echo "Usuario '$TARGET_USER' añadido al grupo 'dialout'."

# Instalar arduino-cli (el binario)
curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sudo sh
echo "arduino-cli instalado."

# Las siguientes configuraciones (placas, librerías) deben hacerse COMO EL USUARIO
echo "Configurando arduino-cli para el usuario '$TARGET_USER'..."
sudo -u $TARGET_USER bash -c "
  # 1. Inicializar la configuración
  arduino-cli config init
  
  # 2. Añadir la URL de las placas ESP32
  arduino-cli config set board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  
  # 3. Actualizar el índice e instalar el core de ESP32
  arduino-cli core update-index
  arduino-cli core install esp32:esp32
  
  # 4. Instalar las librerías necesarias
  echo 'Instalando librerías de Arduino...'
  arduino-cli lib install \"PubSubClient\"
  arduino-cli lib install \"Adafruit NeoPixel\"
  arduino-cli lib install \"ArduinoJson\"
  arduino-cli lib install \"HX711-ADC\"
  arduino-cli lib install \"Adafruit SHT31\"
  # Nota: WiFi, Wire, ArduinoOTA, etc, ya vienen con el core de ESP32
"
echo "Configuración de arduino-cli completada."


echo ""
echo "=========================================================="
echo "          ¡CONFIGURACIÓN DEL SCRIPT COMPLETADA!"
echo "=========================================================="
echo "PASOS SIGUIENTES (MANUALES):"
echo "1. Para activar el acceso remoto, ejecuta este comando:"
echo "   rpi-connect signin"
echo ""
echo "2. REINICIA la Pi para que los cambios de grupo ('dialout')"
echo "   y hardware surtan efecto:"
echo "   sudo reboot"
echo "================================D=========================="
