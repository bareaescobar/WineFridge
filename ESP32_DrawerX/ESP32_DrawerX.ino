/*
 * WineFridge Drawer ESP32 - Simplified with Debug
 * Production version with simple debugging
* ESP32 v21 - 23.07.2025 14.50h
    
    Conections:
    switch1 - D4  ✅
    switch2 - D5  ✅  
    switch3 - D23 ✅
    switch4 - D32 ✅
    switch5 - D33 ✅
    switch6 - D26 ✅
    switch7 - D27 ✅
    switch8 - D14 ✅
    switch9 - D12 ✅

    temp SCL - D22 ✅
    temp SDA - D21 ✅
    
    weight DT - D19 ✅
    weight SCK - D18 ✅
    
    Front LEDs - D13 ✅

    COB Warm - D2  ✅ 
    COB Cool - D15 ✅ 
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// ==================== CONFIGURATION ====================
#define DRAWER_ID "drawer_1"

// Network
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing
#define HEARTBEAT_INTERVAL 30000
#define SENSOR_CHECK_INTERVAL 1000
#define CONNECTION_CHECK_INTERVAL 10000
#define DEBUG_STATUS_INTERVAL 30000

// Hardware - PINOUT CORREGIDO
const uint8_t SWITCH_PINS[9] = {4, 5, 23, 32, 33, 26, 27, 14, 12};  // Orden: sw1-sw9
const uint8_t bottleToLed[9] = {32, 28, 24, 20, 16, 12, 8, 4, 0};
#define WS2812_DATA_PIN 13
#define NUM_LEDS 33
#define HX711_DT_PIN 19
#define HX711_SCK_PIN 18
#define COB_WARM_PIN 2    // CAMBIADO desde 35 (pin solo-entrada)
#define COB_COOL_PIN 15   // CAMBIADO desde 34 (pin solo-entrada)

// Constants
#define WEIGHT_CALIBRATION_FACTOR 94.302184
#define WEIGHT_THRESHOLD 50.0
#define DEBOUNCE_TIME 50
#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
#define TEMP_MIN 2700
#define TEMP_MAX 6500

// Colors
#define COLOR_AVAILABLE 0x0000FF
#define COLOR_OCCUPIED  0x444444
#define COLOR_OFF       0x000000

// ==================== GLOBALS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

struct {
  bool positions[9];
  float individualWeights[9];
  float lastTotalWeight;
  uint16_t currentTemperature = 4000;
  uint8_t currentBrightness = 0;
  float ambientTemperature = 0.0;
  float ambientHumidity = 0.0;
  bool weightSensorReady = false;
  bool tempSensorReady = false;
  unsigned long uptime = 0;
} state;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== DEBUG UTILITIES ====================
void debugPrint(String message) {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("] ");
  Serial.println(message);
}

void debugStatus() {
  Serial.println("\n=== SYSTEM STATUS ===");
  Serial.printf("WiFi: %s (RSSI: %d)\n", WiFi.isConnected() ? "OK" : "FAIL", WiFi.RSSI());
  Serial.printf("MQTT: %s\n", mqttClient.connected() ? "OK" : "FAIL");
  Serial.printf("Weight Sensor: %s\n", state.weightSensorReady ? "OK" : "FAIL");
  Serial.printf("Temp Sensor: %s\n", state.tempSensorReady ? "OK" : "FAIL");
  Serial.printf("Total Weight: %.1fg\n", state.lastTotalWeight);
  Serial.printf("Temp: %.1f°C, Humidity: %.1f%%\n", state.ambientTemperature, state.ambientHumidity);
  Serial.printf("Uptime: %lu minutes\n", millis() / 60000);
  Serial.println("==================\n");
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  debugPrint("=== WineFridge Drawer Starting ===");

  // Configurar pin 2 inmediatamente para evitar conflictos
  pinMode(COB_WARM_PIN, OUTPUT);
  digitalWrite(COB_WARM_PIN, LOW);  // TIP120 no conduce = LED apagado
  pinMode(COB_COOL_PIN, OUTPUT); 
  digitalWrite(COB_COOL_PIN, LOW);  // Por consistencia
  delay(100);  // Dar tiempo al hardware para estabilizar
  
  // Initialize state
  memset(&state.positions, 0, sizeof(state.positions));
  memset(&state.individualWeights, 0, sizeof(state.individualWeights));
  
  // MQTT topics
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  // Hardware initialization
  debugPrint("Initializing hardware...");
  
  // Position switches
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
  }
  
  // LED strip
  strip.begin();
  strip.clear();
  strip.show();
  
  // COB LEDs
  ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  //ledcWrite(COB_WARM_PIN, 0);
  //ledcWrite(COB_COOL_PIN, 0);
  
  // Weight sensor - inicialización más robusta
  debugPrint("Initializing weight sensor...");
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  delay(1000); // Dar tiempo al sensor para estabilizar
  
  if (scale.is_ready()) {
    scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
    delay(500);
    
    // Tare solo si no hay botellas detectadas
    bool anyBottlesPresent = false;
    for (int i = 0; i < 9; i++) {
      if (digitalRead(SWITCH_PINS[i]) == LOW) {
        anyBottlesPresent = true;
        break;
      }
    }
    
    if (!anyBottlesPresent) {
      debugPrint("No bottles detected, performing tare...");
      scale.tare();
      state.lastTotalWeight = 0.0;
    } else {
      debugPrint("Bottles detected, skipping tare");
      state.lastTotalWeight = scale.get_units(3);
    }
    
    state.weightSensorReady = true;
    debugPrint("Weight sensor: OK");
  } else {
    debugPrint("Weight sensor: FAILED");
  }
  
  // Temperature sensor (I2C en pines 21-SDA, 22-SCL)
  Wire.begin(21, 22); // SDA=21, SCL=22
  if (sht31.begin(0x44)) {
    state.tempSensorReady = true;
    debugPrint("Temperature sensor: OK");
  } else {
    debugPrint("Temperature sensor: FAILED");
  }
  
  // Network setup
  setupNetwork();
  
  // Initial sensor reading
  readAllPositions();
  updateTemperatureHumidity();
  
  debugPrint("System Ready!");
  debugStatus();
}

// ==================== NETWORK SETUP ====================
void setupNetwork() {
  debugPrint("Setting up network...");
  
  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  debugPrint("Connecting to WiFi...");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    debugPrint("WiFi connected! IP: " + WiFi.localIP().toString());
    
    // MQTT setup
    mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
    mqttClient.setCallback(onMQTTMessage);
    mqttClient.setBufferSize(2048);
    
    // Connect MQTT
    connectMQTT();
  } else {
    debugPrint("WiFi connection failed!");
  }
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  debugPrint("Connecting to MQTT...");
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    debugPrint("MQTT connected!");
    mqttClient.subscribe(mqtt_topic_command);
  } else {
    debugPrint("MQTT connection failed! State: " + String(mqttClient.state()));
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  // Yield al sistema para evitar watchdog
  yield();
  
  state.uptime = millis();
  
  // Connection maintenance
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    maintainConnections();
    lastConnectionCheck = millis();
  }
  
  // MQTT processing
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  // Sensor monitoring (reducido a cada 2 segundos para estabilidad)
  static unsigned long lastSensorCheck = 0;
  if (millis() - lastSensorCheck > (SENSOR_CHECK_INTERVAL * 2)) {
    checkPositionChanges();
    lastSensorCheck = millis();
  }
  
  // Temperature reading (every 15 seconds)
  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 15000) {
    updateTemperatureHumidity();
    lastTempRead = millis();
  }
  
  // Heartbeat
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }
  
  // Debug status
  static unsigned long lastDebugStatus = 0;
  if (millis() - lastDebugStatus > DEBUG_STATUS_INTERVAL) {
    debugStatus();
    lastDebugStatus = millis();
  }
  
  // Pequeña pausa para estabilidad
  delay(10);
}

void maintainConnections() {
  // WiFi check
  if (WiFi.status() != WL_CONNECTED) {
    debugPrint("WiFi lost, reconnecting...");
    WiFi.reconnect();
    return;
  }
  
  // MQTT check
  if (!mqttClient.connected()) {
    debugPrint("MQTT lost, reconnecting...");
    connectMQTT();
  }
}

// ==================== SENSORS ====================
void readAllPositions() {
  state.lastTotalWeight = readTotalWeight();
  for (int i = 0; i < 9; i++) {
    state.positions[i] = (digitalRead(SWITCH_PINS[i]) == LOW);
  }
}

float readTotalWeight() {
  if (!state.weightSensorReady || !scale.is_ready()) {
    debugPrint("Weight sensor not ready");
    return state.lastTotalWeight; // Devolver último valor conocido
  }
  
  float totalWeight = 0;
  int validReadings = 0;
  
  // Lecturas más conservadoras para evitar problemas
  for (int i = 0; i < 3; i++) {
    yield(); // Permitir al sistema procesar otras tareas
    
    if (scale.is_ready()) {
      float reading = scale.get_units(1);
      if (reading >= -100 && reading <= 5000 && !isnan(reading) && !isinf(reading)) {
        totalWeight += reading;
        validReadings++;
      } else {
        debugPrint("Invalid weight reading: " + String(reading));
      }
    }
    delay(50); // Mayor delay entre lecturas para estabilidad
  }
  
  if (validReadings > 0) {
    float result = max(0.0f, totalWeight / validReadings);
    debugPrint("Weight reading: " + String(result) + "g (" + String(validReadings) + " valid samples)");
    return result;
  }
  
  debugPrint("No valid weight readings");
  return state.lastTotalWeight; // Devolver último valor si no hay lecturas válidas
}

void updateTemperatureHumidity() {
  if (!state.tempSensorReady) return;
  
  float t = sht31.readTemperature();
  float h = sht31.readHumidity();
  
  if (!isnan(t) && !isnan(h)) {
    state.ambientTemperature = round(t * 10.0) / 10.0;
    state.ambientHumidity = round(h * 10.0) / 10.0;
  }
}

void checkPositionChanges() {
  static bool lastPositions[9] = {false};
  static unsigned long lastDebounce[9] = {0};
  static bool processingChange = false;  // Evitar procesar múltiples cambios simultáneamente
  
  if (processingChange) return;  // Ya procesando un cambio
  
  for (int i = 0; i < 9; i++) {
    bool currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
    
    // Solo procesar si ha pasado el tiempo de debounce
    if (millis() - lastDebounce[i] < DEBOUNCE_TIME) continue;
    
    if (currentState != lastPositions[i]) {
      lastDebounce[i] = millis();
      processingChange = true;  // Bloquear otros cambios
      
      // Doble verificación tras debounce
      delay(DEBOUNCE_TIME);
      currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
      
      if (currentState != lastPositions[i]) {
        lastPositions[i] = currentState;
        state.positions[i] = currentState;
        
        debugPrint("Position " + String(i + 1) + " changed to: " + String(currentState ? "OCCUPIED" : "FREE"));
        
        float weightChange = 0.0;
        
        if (currentState) {
          // Botella colocada
          debugPrint("Stabilizing weight measurement...");
          delay(1500);
          float newWeight = readTotalWeight();
          weightChange = newWeight - state.lastTotalWeight;
          
          if (weightChange > 10.0) {
            state.individualWeights[i] = weightChange;
            state.lastTotalWeight = newWeight;
            setPositionLED(i + 1, COLOR_OCCUPIED, 50);
            debugPrint("Bottle PLACED at position " + String(i + 1) + " (Weight: " + String(weightChange) + "g)");
            sendBottleEvent(i + 1, currentState, weightChange);
          } else {
            debugPrint("Weight change too small, ignoring");
            lastPositions[i] = !currentState;
            state.positions[i] = !currentState;
          }
        } else {
          // Botella retirada
          weightChange = state.individualWeights[i];
          state.individualWeights[i] = 0.0;
          delay(1000);
          state.lastTotalWeight = readTotalWeight();
          setPositionLED(i + 1, COLOR_OFF, 0);
          debugPrint("Bottle REMOVED from position " + String(i + 1) + " (Weight was: " + String(weightChange) + "g)");
          sendBottleEvent(i + 1, currentState, weightChange);
        }
        
        processingChange = false;  // Liberar bloqueo
        return; // Solo un cambio por vez
      }
      
      processingChange = false;  // Liberar bloqueo si no hubo cambio real
    }
  }
}

// ==================== LED CONTROL ====================
void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness) {
if (position < 1 || position > 9) {
    debugPrint("Invalid position: " + String(position));
    return;
  }
  
  uint8_t ledIndex = bottleToLed[position - 1];
  debugPrint("Setting LED at index " + String(ledIndex) + " for position " + String(position));
  
  // Verificar que el índice esté dentro del rango
  if (ledIndex >= NUM_LEDS) {
    debugPrint("LED index out of range: " + String(ledIndex));
    return;
  }
  
  // Extraer componentes RGB del color
  uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;  
  uint8_t b = (color & 0xFF) * brightness / 255;
  
  debugPrint("Setting LED " + String(position) + " (index " + String(ledIndex) + 
             ") to RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
  
  // Set the pixel - IMPORTANTE: WS2812B usa GRB, no RGB
  strip.setPixelColor(ledIndex, strip.Color(r, g, b));
  strip.show();
  
  debugPrint("LED " + String(position) + " set successfully");
}

void setCOBLighting(uint16_t temperature, uint8_t brightness) {
  temperature = constrain(temperature, TEMP_MIN, TEMP_MAX);
  brightness = constrain(brightness, 0, 100);
  
  float warmRatio = map(temperature, TEMP_MIN, TEMP_MAX, 100, 0) / 100.0;
  float coolRatio = 1.0 - warmRatio;
  
  uint8_t maxPWM = map(brightness, 0, 100, 0, 255);
  uint8_t warmPWM = maxPWM * warmRatio;
  uint8_t coolPWM = maxPWM * coolRatio;
  
  ledcWrite(COB_WARM_PIN, warmPWM);
  ledcWrite(COB_COOL_PIN, coolPWM);
  
  state.currentTemperature = temperature;
  state.currentBrightness = brightness;
  
  debugPrint("COB light set: " + String(temperature) + "K, " + String(brightness) + "%");
}

// ==================== MQTT MESSAGES ====================
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  
  if (length > 1500) {
    debugPrint("MQTT message too long: " + String(length) + " bytes - ignored");
    return;
  }
  
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  debugPrint("MQTT received (" + String(length) + " bytes): " + String(message).substring(0, 100) + "...");
    
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, message) == DeserializationError::Ok) {
    handleCommand(doc);
  }
}

void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  debugPrint("Command: " + action);
  
  if (action == "set_leds") {
    JsonArray positions = data["positions"];
    for (JsonObject pos : positions) {
      uint8_t position = pos["position"];
      String colorStr = pos["color"];
      uint8_t brightness = pos["brightness"];
      
      if (position >= 1 && position <= 9) {
        uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
        setPositionLED(position, color, brightness);
        debugPrint("Set LED " + String(position) + " to color " + colorStr + " brightness " + String(brightness));
      }
    }
    
    // NUEVO: Procesar duration_ms si existe
    if (data.containsKey("duration_ms")) {
      int duration = data["duration_ms"];
      debugPrint("LED duration set to: " + String(duration) + "ms");
      // Aquí podrías implementar un timer para apagar los LEDs después del tiempo
    }
  } 
  else if (action == "set_general_light") {
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setCOBLighting(temperature, brightness);
  }
  else if (action == "expect_bottle") {
    uint8_t position = data["position"];
    setPositionLED(position, COLOR_AVAILABLE, 100);
    debugPrint("Expecting bottle at position " + String(position));
  } 
  else if (action == "read_sensors") {
    sendHeartbeat();
  }
  else {
    debugPrint("Unknown command: " + action);
  }
}

void sendHeartbeat() {
  if (!mqttClient.connected()) {
    debugPrint("Heartbeat skipped - MQTT not connected");
    return;
  }
  
  StaticJsonDocument<1024> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = state.uptime;
  data["wifi_rssi"] = WiFi.RSSI();
  data["total_weight"] = state.lastTotalWeight;
  data["sensors_ready"] = state.weightSensorReady && state.tempSensorReady;
  data["temperature"] = state.ambientTemperature;
  data["humidity"] = state.ambientHumidity;
  data["general_light_temperature"] = state.currentTemperature;
  data["general_light_brightness"] = state.currentBrightness;
  
  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    JsonObject pos = positions.createNestedObject();
    pos["position"] = i + 1;
    pos["occupied"] = state.positions[i];
    pos["weight"] = round(state.individualWeights[i] * 10.0) / 10.0;
  }
  
  String output;
  serializeJson(doc, output);
  
  bool sent = mqttClient.publish(mqtt_topic_status, output.c_str());
  debugPrint("Heartbeat " + String(sent ? "SENT" : "FAILED"));
}

void sendBottleEvent(uint8_t position, bool occupied, float weight) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["action"] = "bottle_event";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["event"] = occupied ? "placed" : "removed";
  data["position"] = position;
  data["weight"] = weight;
  data["total_weight"] = state.lastTotalWeight;
  
  String output;
  serializeJson(doc, output);
  
  bool sent = mqttClient.publish(mqtt_topic_status, output.c_str());
  debugPrint("Bottle event " + String(sent ? "SENT" : "FAILED"));
}