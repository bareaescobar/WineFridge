#!/bin/bash

BROKER="192.168.1.84"
LIGHTING="lighting_6"
DRAWER=6

echo "============================================"
echo "  TESTING DRAWER 6 - lighting_6"
echo "============================================"
echo ""

# Activar debug mode
echo "✓ Activating debug mode..."
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{"action":"debug_mode","data":{"enabled":true}}'
sleep 2

echo ""
echo "--- GENERAL LIGHTS (COB) ---"
echo ""

# Test 1: General warm 0%
echo "1. General COB - Warm (2700K) @ 0%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_general_light",
  "data": {
    "drawer": '$DRAWER',
    "temperature": 2700,
    "brightness": 0
  }
}'
sleep 2

# Test 2: General warm 50%
echo "2. General COB - Warm (2700K) @ 50%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_general_light",
  "data": {
    "drawer": '$DRAWER',
    "temperature": 2700,
    "brightness": 50
  }
}'
sleep 3

# Test 3: General warm 100%
echo "3. General COB - Warm (2700K) @ 100%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_general_light",
  "data": {
    "drawer": '$DRAWER',
    "temperature": 2700,
    "brightness": 100
  }
}'
sleep 3

# Test 4: General cool 50%
echo "4. General COB - Cool (6500K) @ 50%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_general_light",
  "data": {
    "drawer": '$DRAWER',
    "temperature": 6500,
    "brightness": 50
  }
}'
sleep 3

# Test 5: General cool 100%
echo "5. General COB - Cool (6500K) @ 100%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_general_light",
  "data": {
    "drawer": '$DRAWER',
    "temperature": 6500,
    "brightness": 100
  }
}'
sleep 3

# Apagar
echo "6. General COB - OFF"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_general_light",
  "data": {
    "drawer": '$DRAWER',
    "temperature": 4000,
    "brightness": 0
  }
}'
sleep 2

echo ""
echo "--- FRONT LEDs (WS2812B Strip) ---"
echo ""

# Test 7: Front LEDs - Warm white 0% (OFF)
echo "7. Front LEDs - Warm White @ 0% (OFF)"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_all_leds",
  "data": {
    "drawer": '$DRAWER',
    "color": "#FFE5B4",
    "brightness": 0
  }
}'
sleep 2

# Test 8: Front LEDs - Warm white 50%
echo "8. Front LEDs - Warm White @ 50% (brightness=50)"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_all_leds",
  "data": {
    "drawer": '$DRAWER',
    "color": "#FFE5B4",
    "brightness": 50
  }
}'
sleep 3

# Test 9: Front LEDs - Warm white 100%
echo "9. Front LEDs - Warm White @ 100%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_all_leds",
  "data": {
    "drawer": '$DRAWER',
    "color": "#FFE5B4",
    "brightness": 100
  }
}'
sleep 3

# Test 10: Front LEDs - Cool white 50%
echo "10. Front LEDs - Cool White @ 50%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_all_leds",
  "data": {
    "drawer": '$DRAWER',
    "color": "#E0F4FF",
    "brightness": 50
  }
}'
sleep 3

# Test 11: Front LEDs - Cool white 100%
echo "11. Front LEDs - Cool White @ 100%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_all_leds",
  "data": {
    "drawer": '$DRAWER',
    "color": "#E0F4FF",
    "brightness": 100
  }
}'
sleep 3

# Test 12: Front LEDs - Pure white 50%
echo "12. Front LEDs - Pure White @ 50%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_all_leds",
  "data": {
    "drawer": '$DRAWER',
    "color": "#FFFFFF",
    "brightness": 50
  }
}'
sleep 3

# Test 13: Front LEDs - Pure white 100%
echo "13. Front LEDs - Pure White @ 100%"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_all_leds",
  "data": {
    "drawer": '$DRAWER',
    "color": "#FFFFFF",
    "brightness": 100
  }
}'
sleep 3

echo ""
echo "--- INDIVIDUAL LEDs TEST ---"
echo ""

# Test 14: Individual LEDs - algunos encendidos
echo "14. Individual LEDs - Some LEDs (1, 5, 10, 17)"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{
  "action": "set_leds",
  "data": {
    "drawer": '$DRAWER',
    "leds": [
      {"led": 1, "color": "#FF0000", "brightness": 100},
      {"led": 5, "color": "#00FF00", "brightness": 100},
      {"led": 10, "color": "#0000FF", "brightness": 100},
      {"led": 17, "color": "#FFFFFF", "brightness": 100}
    ]
  }
}'
sleep 3

# Test 15: Apagar todo
echo "15. ALL OFF"
mosquitto_pub -h $BROKER -t "winefridge/$LIGHTING/command" -m \
'{"action":"all_off","data":{}}'
sleep 2

echo ""
echo "============================================"
echo "  TEST COMPLETE!"
echo "============================================"
echo ""
echo "Check Serial Monitor for debug output"
