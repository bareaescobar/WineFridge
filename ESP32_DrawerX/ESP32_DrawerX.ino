/*
 * WineFridge Drawer ESP32 - Combined Best Version
 * Claude v34 - 14:48 @ 30.06.2025
 * Simple, efficient, but with good debugging capabilities
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HX711.h>  // Weight sensor library

// ==================== CONFIGURATION ====================
// Drawer Identity
#define DRAWER_ID "drawer_1"

// WiFi Configuration - CORRECT CREDENTIALS
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"

// MQTT Configuration - CORRECT IP
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing Configuration
#define HEARTBEAT_INTERVAL 30000        // 30 seconds
#define SENSOR_READ_INTERVAL 1000       // 1 second
#define DEBOUNCE_TIME 50               // 50ms debounce
#define CONNECTION_CHECK_INTERVAL 10000 // 10 seconds

// ==================== PIN DEFINITIONS ====================
// Position Detection Switches (Normally Open, using internal pullup)
// Positions follow zigzag pattern: front-back-front-back...
const uint8_t SWITCH_PINS[9] = {
    4,   // Position 1 - Front left
    5,   // Position 2 - Back left
    23,  // Position 3 - Front second
    14,  // Position 4 - Back second
    27,  // Position 5 - Front middle
    26,  // Position 6 - Back third
    12,  // Position 7 - Front fourth
    33,  // Position 8 - Back right
    32   // Position 9 - Front right
};

// LED indexes for each position (WS2812 strip has 33 LEDs total)
// Only these 9 LEDs are used for bottle positions, rest stay off
const uint8_t bottleToLed[9] = {
    32,  // Position 1 LED (Front left)
    28,  // Position 2 LED (Back left)  
    24,  // Position 3 LED (Front second)
    20,  // Position 4 LED (Back second)
    16,  // Position 5 LED (Front middle)
    12,  // Position 6 LED (Back third)
    8,   // Position 7 LED (Front fourth)
    4,   // Position 8 LED (Back right)
    0    // Position 9 LED (Front right)
};

// TODO: Future general illumination could use the remaining 24 LEDs

// LED Control
#define WS2812_DATA_PIN 13
#define NUM_LEDS 33  // Total LEDs in WS2812 strip (only 9 used for positions)

// Weight Sensor (HX711)
#define HX711_DT_PIN  19    // Data pin
#define HX711_SCK_PIN 18    // Clock pin

// Weight Sensor Configuration
#define WEIGHT_CALIBRATION_FACTOR 107.50
#define WEIGHT_THRESHOLD 50.0           // 50g minimum change to report
#define WEIGHT_STABILIZATION_TIME 2000  // 2 seconds to stabilize
#define MAX_WEIGHT 2000.0              // 2kg max per position

// ==================== LED COLOR DEFINITIONS ====================
#define COLOR_AVAILABLE   0x0000FF  // Blue - position available
#define COLOR_SUCCESS     0x00FF00  // Green - success/confirmation
#define COLOR_ERROR       0xFF0000  // Red - error
#define COLOR_OCCUPIED    0x444444  // Dim white - position occupied
#define COLOR_OFF         0x000000  // Off

// ==================== GLOBAL OBJECTS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;  // Weight sensor

// ==================== STATE VARIABLES ====================
struct DrawerState {
  bool positions[9];              // Track occupied positions (true = occupied)
  float weights[9];               // Weight at each position (grams)
  unsigned long lastHeartbeat;    // Last heartbeat time
  unsigned long uptime;           // System uptime
  bool systemReady;               // System initialization complete
  bool weightSensorReady;         // Weight sensor calibrated and ready
} drawerState;

// Pre-calculated MQTT topics for efficiency
char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("    WineFridge Drawer - Combined v1.0");
  Serial.println("========================================");
  Serial.print("Drawer ID: ");
  Serial.println(DRAWER_ID);
  Serial.println();
  
  // Initialize state
  memset(&drawerState, 0, sizeof(drawerState));
  
  // Initialize weights array to zero
  for (int i = 0; i < 9; i++) {
    drawerState.weights[i] = 0.0;
  }
  
  // Pre-calculate MQTT topics
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  // Initialize position switches
  Serial.println("→ Initializing position switches...");
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    drawerState.positions[i] = false; // Start assuming all empty
  }
  Serial.println("✓ Position switches ready");
  
  // Initialize LED strip
  Serial.println("→ Initializing LED strip...");
  strip.begin();
  strip.clear();
  strip.show();
  testLEDsOnStartup();
  Serial.println("✓ LED strip ready");
  
  // Initialize weight sensor
  Serial.println("→ Initializing weight sensor...");
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
  scale.tare();  // Reset to zero
  drawerState.weightSensorReady = true;
  Serial.println("✓ Weight sensor ready");
  Serial.print("✓ Calibration factor: ");
  Serial.println(WEIGHT_CALIBRATION_FACTOR);
  
  // Connect to WiFi
  connectWiFi();
  
  // Setup MQTT (only if WiFi connected)
  if (WiFi.status() == WL_CONNECTED) {
    setupMQTT();
    connectMQTT();
  }
  
  // Read initial position states
  Serial.println("→ Reading initial positions...");
  readAllPositions();
  Serial.println("✓ Position states read");
  
  drawerState.systemReady = true;
  drawerState.lastHeartbeat = millis();
  
  Serial.println("\n========================================");
  Serial.println("    System Ready! Type 'HELP' for commands");
  Serial.println("========================================\n");
  
  // Send initial heartbeat if MQTT is connected
  if (mqttClient.connected()) {
    sendHeartbeat();
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  // Maintain connections (check every 10 seconds to avoid spam)
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    
    // Check WiFi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting...");
      connectWiFi();
    }
    
    // Check MQTT (only if WiFi is connected)
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
      Serial.println("MQTT lost, reconnecting...");
      connectMQTT();
    }
    
    lastConnectionCheck = millis();
  }
  
  // Only run MQTT loop if connected
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  // Update uptime
  drawerState.uptime = millis();
  
  // Check for position changes
  static unsigned long lastPositionCheck = 0;
  if (millis() - lastPositionCheck > SENSOR_READ_INTERVAL) {
    checkPositionChanges();
    lastPositionCheck = millis();
  }
  
  // Send heartbeat (only if MQTT is connected)
  if (mqttClient.connected() && millis() - drawerState.lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    drawerState.lastHeartbeat = millis();
  }
  
  // Handle serial commands for debugging
  handleSerialCommands();
}

// ==================== WiFi CONNECTION ====================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return; // Already connected
  }
  
  Serial.println("=== CONNECTING TO WIFI ===");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Better for MQTT stability
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected!");
    Serial.print("✓ IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(" | Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ WiFi connection failed!");
    Serial.println("✗ Check SSID and password");
  }
  Serial.println("============================");
}

// ==================== WEIGHT SENSOR FUNCTIONS ====================
float readCurrentWeight() {
  if (!drawerState.weightSensorReady) {
    return 0.0;
  }
  
  // Take multiple readings and average them
  float totalWeight = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 5; i++) {
    if (scale.is_ready()) {
      float reading = scale.get_units(1);
      
      // Filter out obviously bad readings
      if (reading >= 0 && reading <= MAX_WEIGHT) {
        totalWeight += reading;
        validReadings++;
      }
    }
    delay(10); // Small delay between readings
  }
  
  if (validReadings > 0) {
    return totalWeight / validReadings;
  }
  
  return 0.0;
}

float readStabilizedWeight() {
  Serial.print("Reading weight");
  
  // Wait for weight to stabilize
  float weight1 = readCurrentWeight();
  delay(WEIGHT_STABILIZATION_TIME);
  float weight2 = readCurrentWeight();
  
  Serial.print(" (");
  Serial.print(weight1);
  Serial.print("g → ");
  Serial.print(weight2);
  Serial.print("g)");
  
  // If weights are close, use the second reading
  if (abs(weight2 - weight1) < WEIGHT_THRESHOLD) {
    Serial.println(" - Stable");
    return weight2;
  } else {
    Serial.println(" - Unstable, using average");
    return (weight1 + weight2) / 2.0;
  }
}

void calibrateWeightSensor() {
  Serial.println("\n=== WEIGHT SENSOR CALIBRATION ===");
  Serial.println("1. Remove all weight from sensor");
  Serial.println("2. Press ENTER to tare");
  
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readString(); // Clear input
  
  scale.tare();
  Serial.println("✓ Sensor tared (zeroed)");
  
  Serial.println("3. Place known weight (e.g., 1000g)");
  Serial.println("4. Enter weight in grams and press ENTER:");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  float knownWeight = Serial.parseFloat();
  Serial.readString(); // Clear remaining input
  
  if (knownWeight > 0) {
    float reading = scale.get_units(10); // Average of 10 readings
    float newFactor = reading / knownWeight;
    
    Serial.print("Current reading: ");
    Serial.println(reading);
    Serial.print("Known weight: ");
    Serial.println(knownWeight);
    Serial.print("New calibration factor: ");
    Serial.println(newFactor);
    Serial.println("Update WEIGHT_CALIBRATION_FACTOR in code");
    
    // Apply new factor for this session
    scale.set_scale(newFactor);
    Serial.println("✓ New factor applied for this session");
  }
  
  Serial.println("==============================\n");
}

// ==================== MQTT SETUP & CONNECTION ====================
void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqttClient.setCallback(onMQTTMessage);
  mqttClient.setSocketTimeout(10);
  mqttClient.setKeepAlive(60);
}

void connectMQTT() {
  if (mqttClient.connected()) {
    return; // Already connected
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ Cannot connect MQTT: WiFi not connected");
    return;
  }
  
  Serial.println("\n=== CONNECTING TO MQTT ===");
  Serial.print("Broker: ");
  Serial.print(MQTT_BROKER_IP);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  
  // Test TCP connection first
  if (!testBrokerConnection()) {
    Serial.println("✗ Cannot reach MQTT broker");
    Serial.println("===========================");
    return;
  }
  
  Serial.print("✓ Broker reachable, connecting... ");
  
  // Attempt MQTT connection
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("SUCCESS!");
    Serial.print("✓ Client ID: ");
    Serial.println(clientId);
    
    // Subscribe to command topic
    if (mqttClient.subscribe(mqtt_topic_command)) {
      Serial.print("✓ Subscribed to: ");
      Serial.println(mqtt_topic_command);
    } else {
      Serial.println("✗ Failed to subscribe");
    }
    
    Serial.println("✓ MQTT ready!");
  } else {
    Serial.print("FAILED (rc=");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
  Serial.println("===========================");
}

// Test if we can reach the MQTT broker via TCP
bool testBrokerConnection() {
  WiFiClient testClient;
  if (testClient.connect(MQTT_BROKER_IP, MQTT_PORT)) {
    testClient.stop();
    return true;
  }
  return false;
}

// ==================== MQTT MESSAGE HANDLER ====================
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT ← ");
  Serial.println(topic);
  
  // Convert payload to string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  // Parse JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("✗ JSON error: ");
    Serial.println(error.c_str());
    return;
  }
  
  handleCommand(doc);
}

// ==================== COMMAND HANDLER ====================
void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  Serial.print("Command: ");
  Serial.println(action);
  
  if (action == "set_leds") {
    // Set individual LED positions
    JsonArray positions = data["positions"];
    
    for (JsonObject pos : positions) {
      uint8_t position = pos["position"];
      String colorStr = pos["color"];
      uint8_t brightness = pos["brightness"];
      
      if (position >= 1 && position <= 9) {
        // Convert hex color string to uint32_t
        uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
        setPositionLED(position, color, brightness);
        
        Serial.print("LED ");
        Serial.print(position);
        Serial.print(" → ");
        Serial.print(colorStr);
        Serial.print(" @ ");
        Serial.print(brightness);
        Serial.println("%");
      }
    }
    
  } else if (action == "expect_bottle") {
    // Highlight position for bottle placement
    uint8_t position = data["position"];
    String barcode = data["barcode"];
    String name = data["name"];
    
    Serial.print("Expecting bottle at position ");
    Serial.print(position);
    Serial.print(": ");
    Serial.println(name);
    
    // Light up the position
    setPositionLED(position, COLOR_AVAILABLE, 100);
    
  } else if (action == "read_sensors") {
    // Send immediate sensor reading
    Serial.println("Sending immediate heartbeat with current weights");
    
    // Update current weights before sending
    for (int i = 0; i < 9; i++) {
      if (drawerState.positions[i] && drawerState.weightSensorReady) {
        drawerState.weights[i] = readCurrentWeight();
      }
    }
    
    sendHeartbeat();
    
  } else if (action == "test_leds") {
    // Test all LEDs
    Serial.println("Testing all LEDs");
    testAllLEDs();
    
  } else {
    Serial.print("Unknown command: ");
    Serial.println(action);
  }
}

// ==================== POSITION SWITCH READING ====================
void readAllPositions() {
  Serial.println("Reading all positions with weights...");
  
  for (int i = 0; i < 9; i++) {
    // LOW = bottle present (switch closed), HIGH = empty (switch open)
    bool occupied = (digitalRead(SWITCH_PINS[i]) == LOW);
    drawerState.positions[i] = occupied;
    
    // Read current weight if position is occupied
    if (occupied && drawerState.weightSensorReady) {
      float weight = readCurrentWeight();
      drawerState.weights[i] = weight;
      Serial.print("Position ");
      Serial.print(i + 1);
      Serial.print(": OCCUPIED (");
      Serial.print(weight);
      Serial.println("g)");
    } else {
      drawerState.weights[i] = 0.0;
      Serial.print("Position ");
      Serial.print(i + 1);
      Serial.println(": EMPTY");
    }
  }
}

void checkPositionChanges() {
  static bool lastPositions[9] = {false};
  
  for (int i = 0; i < 9; i++) {
    // Read current state
    bool currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
    
    // Check if state changed
    if (currentState != lastPositions[i]) {
      // Debounce
      delay(DEBOUNCE_TIME);
      
      // Read again to confirm
      currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
      
      if (currentState != lastPositions[i]) {
        // Confirmed change
        Serial.print("Position ");
        Serial.print(i + 1);
        Serial.print(" → ");
        Serial.println(currentState ? "OCCUPIED" : "EMPTY");
        
        // Update state
        drawerState.positions[i] = currentState;
        lastPositions[i] = currentState;
        
        // Read weight when bottle is placed or removed
        float weight = 0.0;
        if (currentState) {
          // Bottle placed - read stabilized weight
          weight = readStabilizedWeight();
          drawerState.weights[i] = weight;
          Serial.print("✓ Weight recorded: ");
          Serial.print(weight);
          Serial.println("g");
        } else {
          // Bottle removed - zero the weight
          drawerState.weights[i] = 0.0;
          Serial.println("✓ Weight cleared");
        }
        
        // Send bottle event to RPI with weight
        sendBottleEvent(i + 1, currentState, weight);
        
        // Update LED to show status
        if (currentState) {
          setPositionLED(i + 1, COLOR_OCCUPIED, 50); // Dim white for occupied
        } else {
          setPositionLED(i + 1, COLOR_OFF, 0); // Turn off for empty
        }
      }
    }
  }
}

// ==================== LED CONTROL ====================
void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness) {
  if (position < 1 || position > 9) return;
  
  // Use zigzag mapping: position → LED index
  uint8_t ledIndex = bottleToLed[position - 1];
  
  // Apply brightness scaling
  uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
  uint8_t b = (color & 0xFF) * brightness / 255;
  
  strip.setPixelColor(ledIndex, r, g, b);
  strip.show();
}

void testLEDsOnStartup() {
  Serial.print("Testing position LEDs (zigzag mapping)");
  
  // Light up each position LED briefly to show mapping
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
    Serial.print(".");
  }
  
  delay(500);
  
  // Turn all off (clears entire strip)
  strip.clear();
  strip.show();
  Serial.println(" done");
}

void testAllLEDs() {
  // Cycle through colors for debugging (only position LEDs)
  Serial.println("LED test: Red (positions only)");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_ERROR, 100);
    delay(100); // Brief delay to see sequence
  }
  delay(1000);
  
  Serial.println("LED test: Green (positions only)");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_SUCCESS, 100);
    delay(100);
  }
  delay(1000);
  
  Serial.println("LED test: Blue (positions only)");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
  }
  delay(1000);
  
  Serial.println("LED test: Off (clearing all)");
  strip.clear(); // This clears all 33 LEDs
  strip.show();
  
  Serial.println("LED test complete - only position LEDs used");
}

// ==================== MQTT PUBLISHING ====================
void sendHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = drawerState.uptime;
  data["wifi_rssi"] = WiFi.RSSI();
  data["weight_sensor_ready"] = drawerState.weightSensorReady;
  
  // Add position data with weights
  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    JsonObject pos = positions.createNestedObject();
    pos["position"] = i + 1;
    pos["occupied"] = drawerState.positions[i];
    pos["weight"] = drawerState.weights[i];  // Include weight data
  }
  
  String output;
  serializeJson(doc, output);
  
  if (mqttClient.publish(mqtt_topic_status, output.c_str())) {
    Serial.print("Heartbeat sent (uptime: ");
    Serial.print(drawerState.uptime / 1000);
    Serial.println("s)");
  } else {
    Serial.println("✗ Failed to send heartbeat");
  }
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
  data["weight"] = weight;  // Include actual weight measurement
  
  String output;
  serializeJson(doc, output);
  
  if (mqttClient.publish(mqtt_topic_status, output.c_str())) {
    Serial.print("Event sent: Position ");
    Serial.print(position);
    Serial.print(" ");
    Serial.print(occupied ? "PLACED" : "REMOVED");
    Serial.print(" (");
    Serial.print(weight);
    Serial.println("g)");
  } else {
    Serial.println("✗ Failed to send bottle event");
  }
}

// ==================== SERIAL DEBUG COMMANDS ====================
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "STATUS") {
      printStatus();
    }
    else if (cmd == "HEARTBEAT") {
      sendHeartbeat();
    }
    else if (cmd == "TEST") {
      testAllLEDs();
    }
    else if (cmd == "POSITIONS") {
      readAllPositions();
    }
    else if (cmd == "MAPPING") {
      printLEDMapping();
    }
    else if (cmd == "PING") {
      connectMQTT();
    }
    else if (cmd == "WIFI") {
      connectWiFi();
    }
    else if (cmd == "WEIGHT") {
      testWeight();
    }
    else if (cmd == "CALIBRATE") {
      calibrateWeightSensor();
    }
    else if (cmd == "TARE") {
      scale.tare();
      Serial.println("✓ Weight sensor tared (zeroed)");
    }
    else if (cmd.startsWith("LED")) {
      handleLEDCommand(cmd);
    }
    else if (cmd == "HELP") {
      printHelp();
    }
    else if (cmd != "") {
      Serial.print("Unknown command: ");
      Serial.print(cmd);
      Serial.println(" (type HELP for commands)");
    }
  }
}

void handleLEDCommand(String cmd) {
  // Parse command like "LED 3 BLUE" or "LED 5 OFF"
  int firstSpace = cmd.indexOf(' ');
  int secondSpace = cmd.indexOf(' ', firstSpace + 1);
  
  if (firstSpace != -1 && secondSpace != -1) {
    int position = cmd.substring(firstSpace + 1, secondSpace).toInt();
    String color = cmd.substring(secondSpace + 1);
    
    if (position >= 1 && position <= 9) {
      if (color == "BLUE") {
        setPositionLED(position, COLOR_AVAILABLE, 100);
      } else if (color == "RED") {
        setPositionLED(position, COLOR_ERROR, 100);
      } else if (color == "GREEN") {
        setPositionLED(position, COLOR_SUCCESS, 100);
      } else if (color == "WHITE") {
        setPositionLED(position, COLOR_OCCUPIED, 100);
      } else if (color == "OFF") {
        setPositionLED(position, COLOR_OFF, 0);
      }
      
      Serial.print("Set position ");
      Serial.print(position);
      Serial.print(" to ");
      Serial.println(color);
    }
  }
}

void printLEDMapping() {
  Serial.println("\n=== LED POSITION MAPPING ===");
  Serial.println("Position → LED Index (Zigzag)");
  for (int i = 0; i < 9; i++) {
    Serial.print("Position ");
    Serial.print(i + 1);
    Serial.print(" → LED ");
    Serial.println(bottleToLed[i]);
  }
  Serial.println("Total LEDs: 33 (24 unused)");
  Serial.println("============================\n");
}

void testWeight() {
  Serial.println("\n=== WEIGHT SENSOR TEST ===");
  Serial.print("Sensor ready: ");
  Serial.println(drawerState.weightSensorReady ? "YES" : "NO");
  Serial.print("Calibration factor: ");
  Serial.println(WEIGHT_CALIBRATION_FACTOR);
  
  if (drawerState.weightSensorReady) {
    Serial.println("Taking 5 readings...");
    for (int i = 0; i < 5; i++) {
      float weight = readCurrentWeight();
      Serial.print("Reading ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(weight);
      Serial.println("g");
      delay(1000);
    }
    
    Serial.println("Stabilized reading:");
    float stableWeight = readStabilizedWeight();
    Serial.print("Final weight: ");
    Serial.print(stableWeight);
    Serial.println("g");
  } else {
    Serial.println("Weight sensor not ready!");
  }
  Serial.println("=========================\n");
}

void printStatus() {
  Serial.println("\n=== DRAWER STATUS ===");
  Serial.print("ID: ");
  Serial.println(DRAWER_ID);
  Serial.print("WiFi: ");
  if (WiFi.isConnected()) {
    Serial.print("Connected (");
    Serial.print(WiFi.localIP());
    Serial.print(", RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm)");
  } else {
    Serial.println("Disconnected");
  }
  Serial.print("MQTT: ");
  Serial.println(mqttClient.connected() ? "Connected" : "Disconnected");
  Serial.print("Weight sensor: ");
  Serial.println(drawerState.weightSensorReady ? "Ready" : "Not ready");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  // Show current weight reading
  if (drawerState.weightSensorReady) {
    Serial.print("Current weight reading: ");
    Serial.print(readCurrentWeight());
    Serial.println("g");
  }
  
  Serial.println("\nPositions:");
  for (int i = 0; i < 9; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(": ");
    if (drawerState.positions[i]) {
      Serial.print("OCCUPIED (");
      Serial.print(drawerState.weights[i]);
      Serial.println("g)");
    } else {
      Serial.println("EMPTY");
    }
  }
  Serial.println("====================\n");
}

void printHelp() {
  Serial.println("\n=== SERIAL COMMANDS ===");
  Serial.println("STATUS     - Show system status");
  Serial.println("HEARTBEAT  - Send heartbeat now");
  Serial.println("TEST       - Test all LEDs");
  Serial.println("POSITIONS  - Read all positions");
  Serial.println("MAPPING    - Show LED position mapping");
  Serial.println("WEIGHT     - Test weight sensor");
  Serial.println("CALIBRATE  - Calibrate weight sensor");
  Serial.println("TARE       - Zero weight sensor");
  Serial.println("PING       - Reconnect MQTT");
  Serial.println("WIFI       - Reconnect WiFi");
  Serial.println("LED 3 BLUE - Set position 3 blue");
  Serial.println("LED 5 OFF  - Turn off position 5");
  Serial.println("HELP       - Show this help");
  Serial.println("========================\n");
}1~/*
 * WineFridge Drawer ESP32 - Combined Best Version
 * Combines simplicity of v2.1 with robustness of v1
 * Simple, efficient, but with good debugging capabilities
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HX711.h>  // Weight sensor library

// ==================== CONFIGURATION ====================
// Drawer Identity
#define DRAWER_ID "drawer_1"

// WiFi Configuration - CORRECT CREDENTIALS
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"

// MQTT Configuration - CORRECT IP
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing Configuration
#define HEARTBEAT_INTERVAL 30000        // 30 seconds
#define SENSOR_READ_INTERVAL 1000       // 1 second
#define DEBOUNCE_TIME 50               // 50ms debounce
#define CONNECTION_CHECK_INTERVAL 10000 // 10 seconds

// ==================== PIN DEFINITIONS ====================
// Position Detection Switches (Normally Open, using internal pullup)
// Positions follow zigzag pattern: front-back-front-back...
const uint8_t SWITCH_PINS[9] = {
    4,   // Position 1 - Front left
    5,   // Position 2 - Back left
    23,  // Position 3 - Front second
    14,  // Position 4 - Back second
    27,  // Position 5 - Front middle
    26,  // Position 6 - Back third
    12,  // Position 7 - Front fourth
    33,  // Position 8 - Back right
    32   // Position 9 - Front right
};

// LED indexes for each position (WS2812 strip has 33 LEDs total)
// Only these 9 LEDs are used for bottle positions, rest stay off
const uint8_t bottleToLed[9] = {
    32,  // Position 1 LED (Front left)
    28,  // Position 2 LED (Back left)  
    24,  // Position 3 LED (Front second)
    20,  // Position 4 LED (Back second)
    16,  // Position 5 LED (Front middle)
    12,  // Position 6 LED (Back third)
    8,   // Position 7 LED (Front fourth)
    4,   // Position 8 LED (Back right)
    0    // Position 9 LED (Front right)
};

// TODO: Future general illumination could use the remaining 24 LEDs

// LED Control
#define WS2812_DATA_PIN 13
#define NUM_LEDS 33  // Total LEDs in WS2812 strip (only 9 used for positions)

// Weight Sensor (HX711)
#define HX711_DT_PIN  19    // Data pin
#define HX711_SCK_PIN 18    // Clock pin

// Weight Sensor Configuration
#define WEIGHT_CALIBRATION_FACTOR 107.50
#define WEIGHT_THRESHOLD 50.0           // 50g minimum change to report
#define WEIGHT_STABILIZATION_TIME 2000  // 2 seconds to stabilize
#define MAX_WEIGHT 2000.0              // 2kg max per position

// ==================== LED COLOR DEFINITIONS ====================
#define COLOR_AVAILABLE   0x0000FF  // Blue - position available
#define COLOR_SUCCESS     0x00FF00  // Green - success/confirmation
#define COLOR_ERROR       0xFF0000  // Red - error
#define COLOR_OCCUPIED    0x444444  // Dim white - position occupied
#define COLOR_OFF         0x000000  // Off

// ==================== GLOBAL OBJECTS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;  // Weight sensor

// ==================== STATE VARIABLES ====================
struct DrawerState {
  bool positions[9];              // Track occupied positions (true = occupied)
  float weights[9];               // Weight at each position (grams)
  unsigned long lastHeartbeat;    // Last heartbeat time
  unsigned long uptime;           // System uptime
  bool systemReady;               // System initialization complete
  bool weightSensorReady;         // Weight sensor calibrated and ready
} drawerState;

// Pre-calculated MQTT topics for efficiency
char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("    WineFridge Drawer - Combined v1.0");
  Serial.println("========================================");
  Serial.print("Drawer ID: ");
  Serial.println(DRAWER_ID);
  Serial.println();
  
  // Initialize state
  memset(&drawerState, 0, sizeof(drawerState));
  
  // Initialize weights array to zero
  for (int i = 0; i < 9; i++) {
    drawerState.weights[i] = 0.0;
  }
  
  // Pre-calculate MQTT topics
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  // Initialize position switches
  Serial.println("→ Initializing position switches...");
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    drawerState.positions[i] = false; // Start assuming all empty
  }
  Serial.println("✓ Position switches ready");
  
  // Initialize LED strip
  Serial.println("→ Initializing LED strip...");
  strip.begin();
  strip.clear();
  strip.show();
  testLEDsOnStartup();
  Serial.println("✓ LED strip ready");
  
  // Initialize weight sensor
  Serial.println("→ Initializing weight sensor...");
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
  scale.tare();  // Reset to zero
  drawerState.weightSensorReady = true;
  Serial.println("✓ Weight sensor ready");
  Serial.print("✓ Calibration factor: ");
  Serial.println(WEIGHT_CALIBRATION_FACTOR);
  
  // Connect to WiFi
  connectWiFi();
  
  // Setup MQTT (only if WiFi connected)
  if (WiFi.status() == WL_CONNECTED) {
    setupMQTT();
    connectMQTT();
  }
  
  // Read initial position states
  Serial.println("→ Reading initial positions...");
  readAllPositions();
  Serial.println("✓ Position states read");
  
  drawerState.systemReady = true;
  drawerState.lastHeartbeat = millis();
  
  Serial.println("\n========================================");
  Serial.println("    System Ready! Type 'HELP' for commands");
  Serial.println("========================================\n");
  
  // Send initial heartbeat if MQTT is connected
  if (mqttClient.connected()) {
    sendHeartbeat();
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  // Maintain connections (check every 10 seconds to avoid spam)
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    
    // Check WiFi
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi lost, reconnecting...");
      connectWiFi();
    }
    
    // Check MQTT (only if WiFi is connected)
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
      Serial.println("MQTT lost, reconnecting...");
      connectMQTT();
    }
    
    lastConnectionCheck = millis();
  }
  
  // Only run MQTT loop if connected
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  // Update uptime
  drawerState.uptime = millis();
  
  // Check for position changes
  static unsigned long lastPositionCheck = 0;
  if (millis() - lastPositionCheck > SENSOR_READ_INTERVAL) {
    checkPositionChanges();
    lastPositionCheck = millis();
  }
  
  // Send heartbeat (only if MQTT is connected)
  if (mqttClient.connected() && millis() - drawerState.lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    drawerState.lastHeartbeat = millis();
  }
  
  // Handle serial commands for debugging
  handleSerialCommands();
}

// ==================== WiFi CONNECTION ====================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return; // Already connected
  }
  
  Serial.println("=== CONNECTING TO WIFI ===");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // Better for MQTT stability
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected!");
    Serial.print("✓ IP: ");
    Serial.print(WiFi.localIP());
    Serial.print(" | Signal: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("✗ WiFi connection failed!");
    Serial.println("✗ Check SSID and password");
  }
  Serial.println("============================");
}

// ==================== WEIGHT SENSOR FUNCTIONS ====================
float readCurrentWeight() {
  if (!drawerState.weightSensorReady) {
    return 0.0;
  }
  
  // Take multiple readings and average them
  float totalWeight = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 5; i++) {
    if (scale.is_ready()) {
      float reading = scale.get_units(1);
      
      // Filter out obviously bad readings
      if (reading >= 0 && reading <= MAX_WEIGHT) {
        totalWeight += reading;
        validReadings++;
      }
    }
    delay(10); // Small delay between readings
  }
  
  if (validReadings > 0) {
    return totalWeight / validReadings;
  }
  
  return 0.0;
}

float readStabilizedWeight() {
  Serial.print("Reading weight");
  
  // Wait for weight to stabilize
  float weight1 = readCurrentWeight();
  delay(WEIGHT_STABILIZATION_TIME);
  float weight2 = readCurrentWeight();
  
  Serial.print(" (");
  Serial.print(weight1);
  Serial.print("g → ");
  Serial.print(weight2);
  Serial.print("g)");
  
  // If weights are close, use the second reading
  if (abs(weight2 - weight1) < WEIGHT_THRESHOLD) {
    Serial.println(" - Stable");
    return weight2;
  } else {
    Serial.println(" - Unstable, using average");
    return (weight1 + weight2) / 2.0;
  }
}

void calibrateWeightSensor() {
  Serial.println("\n=== WEIGHT SENSOR CALIBRATION ===");
  Serial.println("1. Remove all weight from sensor");
  Serial.println("2. Press ENTER to tare");
  
  while (!Serial.available()) {
    delay(100);
  }
  Serial.readString(); // Clear input
  
  scale.tare();
  Serial.println("✓ Sensor tared (zeroed)");
  
  Serial.println("3. Place known weight (e.g., 1000g)");
  Serial.println("4. Enter weight in grams and press ENTER:");
  
  while (!Serial.available()) {
    delay(100);
  }
  
  float knownWeight = Serial.parseFloat();
  Serial.readString(); // Clear remaining input
  
  if (knownWeight > 0) {
    float reading = scale.get_units(10); // Average of 10 readings
    float newFactor = reading / knownWeight;
    
    Serial.print("Current reading: ");
    Serial.println(reading);
    Serial.print("Known weight: ");
    Serial.println(knownWeight);
    Serial.print("New calibration factor: ");
    Serial.println(newFactor);
    Serial.println("Update WEIGHT_CALIBRATION_FACTOR in code");
    
    // Apply new factor for this session
    scale.set_scale(newFactor);
    Serial.println("✓ New factor applied for this session");
  }
  
  Serial.println("==============================\n");
}

// ==================== MQTT SETUP & CONNECTION ====================
void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqttClient.setCallback(onMQTTMessage);
  mqttClient.setSocketTimeout(10);
  mqttClient.setKeepAlive(60);
}

void connectMQTT() {
  if (mqttClient.connected()) {
    return; // Already connected
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ Cannot connect MQTT: WiFi not connected");
    return;
  }
  
  Serial.println("\n=== CONNECTING TO MQTT ===");
  Serial.print("Broker: ");
  Serial.print(MQTT_BROKER_IP);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  
  // Test TCP connection first
  if (!testBrokerConnection()) {
    Serial.println("✗ Cannot reach MQTT broker");
    Serial.println("===========================");
    return;
  }
  
  Serial.print("✓ Broker reachable, connecting... ");
  
  // Attempt MQTT connection
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("SUCCESS!");
    Serial.print("✓ Client ID: ");
    Serial.println(clientId);
    
    // Subscribe to command topic
    if (mqttClient.subscribe(mqtt_topic_command)) {
      Serial.print("✓ Subscribed to: ");
      Serial.println(mqtt_topic_command);
    } else {
      Serial.println("✗ Failed to subscribe");
    }
    
    Serial.println("✓ MQTT ready!");
  } else {
    Serial.print("FAILED (rc=");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
  Serial.println("===========================");
}

// Test if we can reach the MQTT broker via TCP
bool testBrokerConnection() {
  WiFiClient testClient;
  if (testClient.connect(MQTT_BROKER_IP, MQTT_PORT)) {
    testClient.stop();
    return true;
  }
  return false;
}

// ==================== MQTT MESSAGE HANDLER ====================
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT ← ");
  Serial.println(topic);
  
  // Convert payload to string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  // Parse JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("✗ JSON error: ");
    Serial.println(error.c_str());
    return;
  }
  
  handleCommand(doc);
}

// ==================== COMMAND HANDLER ====================
void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  Serial.print("Command: ");
  Serial.println(action);
  
  if (action == "set_leds") {
    // Set individual LED positions
    JsonArray positions = data["positions"];
    
    for (JsonObject pos : positions) {
      uint8_t position = pos["position"];
      String colorStr = pos["color"];
      uint8_t brightness = pos["brightness"];
      
      if (position >= 1 && position <= 9) {
        // Convert hex color string to uint32_t
        uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
        setPositionLED(position, color, brightness);
        
        Serial.print("LED ");
        Serial.print(position);
        Serial.print(" → ");
        Serial.print(colorStr);
        Serial.print(" @ ");
        Serial.print(brightness);
        Serial.println("%");
      }
    }
    
  } else if (action == "expect_bottle") {
    // Highlight position for bottle placement
    uint8_t position = data["position"];
    String barcode = data["barcode"];
    String name = data["name"];
    
    Serial.print("Expecting bottle at position ");
    Serial.print(position);
    Serial.print(": ");
    Serial.println(name);
    
    // Light up the position
    setPositionLED(position, COLOR_AVAILABLE, 100);
    
  } else if (action == "read_sensors") {
    // Send immediate sensor reading
    Serial.println("Sending immediate heartbeat with current weights");
    
    // Update current weights before sending
    for (int i = 0; i < 9; i++) {
      if (drawerState.positions[i] && drawerState.weightSensorReady) {
        drawerState.weights[i] = readCurrentWeight();
      }
    }
    
    sendHeartbeat();
    
  } else if (action == "test_leds") {
    // Test all LEDs
    Serial.println("Testing all LEDs");
    testAllLEDs();
    
  } else {
    Serial.print("Unknown command: ");
    Serial.println(action);
  }
}

// ==================== POSITION SWITCH READING ====================
void readAllPositions() {
  Serial.println("Reading all positions with weights...");
  
  for (int i = 0; i < 9; i++) {
    // LOW = bottle present (switch closed), HIGH = empty (switch open)
    bool occupied = (digitalRead(SWITCH_PINS[i]) == LOW);
    drawerState.positions[i] = occupied;
    
    // Read current weight if position is occupied
    if (occupied && drawerState.weightSensorReady) {
      float weight = readCurrentWeight();
      drawerState.weights[i] = weight;
      Serial.print("Position ");
      Serial.print(i + 1);
      Serial.print(": OCCUPIED (");
      Serial.print(weight);
      Serial.println("g)");
    } else {
      drawerState.weights[i] = 0.0;
      Serial.print("Position ");
      Serial.print(i + 1);
      Serial.println(": EMPTY");
    }
  }
}

void checkPositionChanges() {
  static bool lastPositions[9] = {false};
  
  for (int i = 0; i < 9; i++) {
    // Read current state
    bool currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
    
    // Check if state changed
    if (currentState != lastPositions[i]) {
      // Debounce
      delay(DEBOUNCE_TIME);
      
      // Read again to confirm
      currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
      
      if (currentState != lastPositions[i]) {
        // Confirmed change
        Serial.print("Position ");
        Serial.print(i + 1);
        Serial.print(" → ");
        Serial.println(currentState ? "OCCUPIED" : "EMPTY");
        
        // Update state
        drawerState.positions[i] = currentState;
        lastPositions[i] = currentState;
        
        // Read weight when bottle is placed or removed
        float weight = 0.0;
        if (currentState) {
          // Bottle placed - read stabilized weight
          weight = readStabilizedWeight();
          drawerState.weights[i] = weight;
          Serial.print("✓ Weight recorded: ");
          Serial.print(weight);
          Serial.println("g");
        } else {
          // Bottle removed - zero the weight
          drawerState.weights[i] = 0.0;
          Serial.println("✓ Weight cleared");
        }
        
        // Send bottle event to RPI with weight
        sendBottleEvent(i + 1, currentState, weight);
        
        // Update LED to show status
        if (currentState) {
          setPositionLED(i + 1, COLOR_OCCUPIED, 50); // Dim white for occupied
        } else {
          setPositionLED(i + 1, COLOR_OFF, 0); // Turn off for empty
        }
      }
    }
  }
}

// ==================== LED CONTROL ====================
void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness) {
  if (position < 1 || position > 9) return;
  
  // Use zigzag mapping: position → LED index
  uint8_t ledIndex = bottleToLed[position - 1];
  
  // Apply brightness scaling
  uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
  uint8_t b = (color & 0xFF) * brightness / 255;
  
  strip.setPixelColor(ledIndex, r, g, b);
  strip.show();
}

void testLEDsOnStartup() {
  Serial.print("Testing position LEDs (zigzag mapping)");
  
  // Light up each position LED briefly to show mapping
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
    Serial.print(".");
  }
  
  delay(500);
  
  // Turn all off (clears entire strip)
  strip.clear();
  strip.show();
  Serial.println(" done");
}

void testAllLEDs() {
  // Cycle through colors for debugging (only position LEDs)
  Serial.println("LED test: Red (positions only)");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_ERROR, 100);
    delay(100); // Brief delay to see sequence
  }
  delay(1000);
  
  Serial.println("LED test: Green (positions only)");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_SUCCESS, 100);
    delay(100);
  }
  delay(1000);
  
  Serial.println("LED test: Blue (positions only)");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
  }
  delay(1000);
  
  Serial.println("LED test: Off (clearing all)");
  strip.clear(); // This clears all 33 LEDs
  strip.show();
  
  Serial.println("LED test complete - only position LEDs used");
}

// ==================== MQTT PUBLISHING ====================
void sendHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = drawerState.uptime;
  data["wifi_rssi"] = WiFi.RSSI();
  data["weight_sensor_ready"] = drawerState.weightSensorReady;
  
  // Add position data with weights
  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    JsonObject pos = positions.createNestedObject();
    pos["position"] = i + 1;
    pos["occupied"] = drawerState.positions[i];
    pos["weight"] = drawerState.weights[i];  // Include weight data
  }
  
  String output;
  serializeJson(doc, output);
  
  if (mqttClient.publish(mqtt_topic_status, output.c_str())) {
    Serial.print("Heartbeat sent (uptime: ");
    Serial.print(drawerState.uptime / 1000);
    Serial.println("s)");
  } else {
    Serial.println("✗ Failed to send heartbeat");
  }
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
  data["weight"] = weight;  // Include actual weight measurement
  
  String output;
  serializeJson(doc, output);
  
  if (mqttClient.publish(mqtt_topic_status, output.c_str())) {
    Serial.print("Event sent: Position ");
    Serial.print(position);
    Serial.print(" ");
    Serial.print(occupied ? "PLACED" : "REMOVED");
    Serial.print(" (");
    Serial.print(weight);
    Serial.println("g)");
  } else {
    Serial.println("✗ Failed to send bottle event");
  }
}

// ==================== SERIAL DEBUG COMMANDS ====================
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "STATUS") {
      printStatus();
    }
    else if (cmd == "HEARTBEAT") {
      sendHeartbeat();
    }
    else if (cmd == "TEST") {
      testAllLEDs();
    }
    else if (cmd == "POSITIONS") {
      readAllPositions();
    }
    else if (cmd == "MAPPING") {
      printLEDMapping();
    }
    else if (cmd == "PING") {
      connectMQTT();
    }
    else if (cmd == "WIFI") {
      connectWiFi();
    }
    else if (cmd == "WEIGHT") {
      testWeight();
    }
    else if (cmd == "CALIBRATE") {
      calibrateWeightSensor();
    }
    else if (cmd == "TARE") {
      scale.tare();
      Serial.println("✓ Weight sensor tared (zeroed)");
    }
    else if (cmd.startsWith("LED")) {
      handleLEDCommand(cmd);
    }
    else if (cmd == "HELP") {
      printHelp();
    }
    else if (cmd != "") {
      Serial.print("Unknown command: ");
      Serial.print(cmd);
      Serial.println(" (type HELP for commands)");
    }
  }
}

void handleLEDCommand(String cmd) {
  // Parse command like "LED 3 BLUE" or "LED 5 OFF"
  int firstSpace = cmd.indexOf(' ');
  int secondSpace = cmd.indexOf(' ', firstSpace + 1);
  
  if (firstSpace != -1 && secondSpace != -1) {
    int position = cmd.substring(firstSpace + 1, secondSpace).toInt();
    String color = cmd.substring(secondSpace + 1);
    
    if (position >= 1 && position <= 9) {
      if (color == "BLUE") {
        setPositionLED(position, COLOR_AVAILABLE, 100);
      } else if (color == "RED") {
        setPositionLED(position, COLOR_ERROR, 100);
      } else if (color == "GREEN") {
        setPositionLED(position, COLOR_SUCCESS, 100);
      } else if (color == "WHITE") {
        setPositionLED(position, COLOR_OCCUPIED, 100);
      } else if (color == "OFF") {
        setPositionLED(position, COLOR_OFF, 0);
      }
      
      Serial.print("Set position ");
      Serial.print(position);
      Serial.print(" to ");
      Serial.println(color);
    }
  }
}

void printLEDMapping() {
  Serial.println("\n=== LED POSITION MAPPING ===");
  Serial.println("Position → LED Index (Zigzag)");
  for (int i = 0; i < 9; i++) {
    Serial.print("Position ");
    Serial.print(i + 1);
    Serial.print(" → LED ");
    Serial.println(bottleToLed[i]);
  }
  Serial.println("Total LEDs: 33 (24 unused)");
  Serial.println("============================\n");
}

void testWeight() {
  Serial.println("\n=== WEIGHT SENSOR TEST ===");
  Serial.print("Sensor ready: ");
  Serial.println(drawerState.weightSensorReady ? "YES" : "NO");
  Serial.print("Calibration factor: ");
  Serial.println(WEIGHT_CALIBRATION_FACTOR);
  
  if (drawerState.weightSensorReady) {
    Serial.println("Taking 5 readings...");
    for (int i = 0; i < 5; i++) {
      float weight = readCurrentWeight();
      Serial.print("Reading ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(weight);
      Serial.println("g");
      delay(1000);
    }
    
    Serial.println("Stabilized reading:");
    float stableWeight = readStabilizedWeight();
    Serial.print("Final weight: ");
    Serial.print(stableWeight);
    Serial.println("g");
  } else {
    Serial.println("Weight sensor not ready!");
  }
  Serial.println("=========================\n");
}

void printStatus() {
  Serial.println("\n=== DRAWER STATUS ===");
  Serial.print("ID: ");
  Serial.println(DRAWER_ID);
  Serial.print("WiFi: ");
  if (WiFi.isConnected()) {
    Serial.print("Connected (");
    Serial.print(WiFi.localIP());
    Serial.print(", RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm)");
  } else {
    Serial.println("Disconnected");
  }
  Serial.print("MQTT: ");
  Serial.println(mqttClient.connected() ? "Connected" : "Disconnected");
  Serial.print("Weight sensor: ");
  Serial.println(drawerState.weightSensorReady ? "Ready" : "Not ready");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  // Show current weight reading
  if (drawerState.weightSensorReady) {
    Serial.print("Current weight reading: ");
    Serial.print(readCurrentWeight());
    Serial.println("g");
  }
  
  Serial.println("\nPositions:");
  for (int i = 0; i < 9; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(": ");
    if (drawerState.positions[i]) {
      Serial.print("OCCUPIED (");
      Serial.print(drawerState.weights[i]);
      Serial.println("g)");
    } else {
      Serial.println("EMPTY");
    }
  }
  Serial.println("====================\n");
}

void printHelp() {
  Serial.println("\n=== SERIAL COMMANDS ===");
  Serial.println("STATUS     - Show system status");
  Serial.println("HEARTBEAT  - Send heartbeat now");
  Serial.println("TEST       - Test all LEDs");
  Serial.println("POSITIONS  - Read all positions");
  Serial.println("MAPPING    - Show LED position mapping");
  Serial.println("WEIGHT     - Test weight sensor");
  Serial.println("CALIBRATE  - Calibrate weight sensor");
  Serial.println("TARE       - Zero weight sensor");
  Serial.println("PING       - Reconnect MQTT");
  Serial.println("WIFI       - Reconnect WiFi");
  Serial.println("LED 3 BLUE - Set position 3 blue");
  Serial.println("LED 5 OFF  - Turn off position 5");
  Serial.println("HELP       - Show this help");
  Serial.println("========================\n");
}
