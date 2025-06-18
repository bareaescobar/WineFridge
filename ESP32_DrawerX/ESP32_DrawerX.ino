/*
 * WineFridge Drawer ESP32 - Phase 1 Basic Version
 * Simple connectivity + LED control + position switches
 * 
 * Features included:
 * - WiFi connection
 * - MQTT connection & heartbeat
 * - LED position control
 * - Position switch reading
 * 
 * TODO for later phases:
 * - Weight sensor (HX711)
 * - Temperature sensor (SHT30)
 * - Advanced animations
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// ==================== CONFIGURATION ====================
// CHANGE THESE VALUES FOR YOUR SETUP!

// Drawer Identity - CHANGE FOR EACH DRAWER!
#define DRAWER_ID "drawer_1"  // drawer_1, drawer_2, or drawer_3

// WiFi Configuration - UPDATE THESE!
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"

// MQTT Configuration - UPDATE THE IP!
#define MQTT_BROKER_IP "192.168.1.84"  // Your RPI's IP address
#define MQTT_PORT 1883

// Timing Configuration
#define HEARTBEAT_INTERVAL 30000        // 30 seconds
#define SENSOR_READ_INTERVAL 1000       // 1 second
#define DEBOUNCE_TIME 50               // 50ms debounce

// ==================== PIN DEFINITIONS ====================

// Position Detection Switches (Normally Open, using internal pullup)
const uint8_t SWITCH_PINS[9] = {
    4,   // Position 1 - GPIO4
    5,   // Position 2 - GPIO5  
    23,  // Position 3 - GPIO23
    14,  // Position 4 - GPIO14
    27,  // Position 5 - GPIO27
    26,  // Position 6 - GPIO26
    12,  // Position 7 - GPIO12
    33,  // Position 8 - GPIO33
    32   // Position 9 - GPIO32
};

// LED Control
#define WS2812_DATA_PIN 13  // Front position LEDs
#define NUM_LEDS 9

// General Lighting (for Phase 2)
#define GENERAL_WARM_PIN 25 // Warm white COB (2700K)
#define GENERAL_COOL_PIN 26 // Cool white COB (6500K)

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

// ==================== STATE VARIABLES ====================
struct DrawerState {
  bool positions[9];              // Track occupied positions (true = occupied)
  unsigned long lastHeartbeat;    // Last heartbeat time
  unsigned long uptime;           // System uptime
  bool systemReady;               // System initialization complete
} drawerState;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n========================================");
  Serial.println("    WineFridge Drawer Starting Up");
  Serial.println("========================================");
  Serial.print("Drawer ID: ");
  Serial.println(DRAWER_ID);
  Serial.println();
  
  // Initialize state
  memset(&drawerState, 0, sizeof(drawerState));
  
  // Initialize pins for position switches
  Serial.println("→ Initializing position switches...");
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    drawerState.positions[i] = false; // Start assuming all empty
  }
  Serial.println("✓ Position switches initialized");
  
  // Initialize LED strip
  Serial.println("→ Initializing LED strip...");
  strip.begin();
  strip.clear();
  strip.show();
  Serial.println("✓ LED strip initialized");
  
  // Test LEDs briefly
  Serial.println("→ Testing LEDs...");
  testLEDsOnStartup();
  Serial.println("✓ LED test complete");
  
  // Connect to WiFi
  connectWiFi();

  WiFi.setSleep(false);
  
  // Only try MQTT if WiFi is connected
  if (WiFi.status() == WL_CONNECTED) {
    // Setup MQTT
    mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
    mqttClient.setCallback(onMQTTMessage);

    // --- PATCH para routers WiFi6 / evitar rc=-2 ---
    mqttClient.setSocketTimeout(10);  // 10 segundos timeout socket
    mqttClient.setKeepAlive(60);      // 60 segundos keepalive
    // --- FIN PATCH ---
    
    connectMQTT();
  }
  
  // Read initial position states
  Serial.println("→ Reading initial positions...");
  readAllPositions();
  Serial.println("✓ Position states read");
  
  drawerState.systemReady = true;
  drawerState.lastHeartbeat = millis();
  
  Serial.println("\n========================================");
  Serial.println("    Drawer Ready for Operation!");
  Serial.println("    Type 'HELP' for commands");
  Serial.println("========================================\n");
  
  // Send initial heartbeat if MQTT is connected
  if (mqttClient.connected()) {
    sendHeartbeat();
    Serial.println("✓ Initial heartbeat sent");
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  // Maintain connections (but not too frequently)
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > 10000) { // Check every 10 seconds
    
    if (!WiFi.isConnected()) {
      Serial.println("WiFi disconnected, attempting reconnection...");
      connectWiFi();
    }
    
    if (WiFi.isConnected() && !mqttClient.connected()) {
      Serial.println("MQTT disconnected, attempting reconnection...");
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
  
  Serial.println("\n=== CONNECTING TO WIFI ===");
  Serial.print("SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("Attempting connection");
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
    
    if (attempts % 10 == 0) {
      Serial.print(" (");
      Serial.print(attempts);
      Serial.print("/30)");
    }
  }
  
  Serial.println(); // New line after dots
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✓ WiFi connected successfully!");
    Serial.print("✓ IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("✓ Gateway: ");
    Serial.println(WiFi.gatewayIP());
    Serial.print("✓ Signal strength: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
    Serial.println("============================");
  } else {
    Serial.println("✗ WiFi connection FAILED!");
    Serial.println("✗ Please check SSID and password");
    Serial.println("============================");
  }
}

// ==================== MQTT CONNECTION ====================
void connectMQTT() {
  if (mqttClient.connected()) {
    return; // Already connected
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("✗ Cannot connect to MQTT: WiFi not connected");
    return;
  }
  
  Serial.println("\n=== CONNECTING TO MQTT ===");
  Serial.print("Broker: ");
  Serial.print(MQTT_BROKER_IP);
  Serial.print(":");
  Serial.println(MQTT_PORT);
  
  // First test if we can reach the broker
  if (!testBrokerConnection()) {
    Serial.println("✗ Cannot reach MQTT broker!");
    Serial.println("✗ Check if mosquitto is running on RPI");
    Serial.println("===========================");
    return;
  }
  
  Serial.println("✓ Broker is reachable, attempting MQTT connection...");
  
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 3) {
    Serial.print("Attempt ");
    Serial.print(attempts + 1);
    Serial.print("/3... ");
    
    // Create unique client ID
    String clientId = String(DRAWER_ID) + "_" + String(random(0xffff), HEX);
    
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("SUCCESS!");
      Serial.print("✓ Client ID: ");
      Serial.println(clientId);
      
      // Subscribe to command topic
      String cmdTopic = String("winefridge/") + DRAWER_ID + "/command";
      if (mqttClient.subscribe(cmdTopic.c_str())) {
        Serial.print("✓ Subscribed to: ");
        Serial.println(cmdTopic);
      } else {
        Serial.println("✗ Failed to subscribe to command topic");
      }
      
      Serial.println("✓ MQTT connection established!");
      Serial.println("=============================");
      return; // Success!
      
    } else {
      int rc = mqttClient.state();
      Serial.print("FAILED (rc=");
      Serial.print(rc);
      Serial.print(" - ");
      Serial.print(getMqttErrorDescription(rc));
      Serial.println(")");
      
      if (attempts < 2) {
        Serial.println("Waiting 3 seconds before retry...");
        delay(3000);
      }
    }
    attempts++;
  }
  
  Serial.println("✗ MQTT connection failed after 3 attempts");
  Serial.println("=============================");
}

// Test if we can reach the MQTT broker
bool testBrokerConnection() {
  WiFiClient testClient;
  Serial.print("Testing TCP connection... ");
  
  if (testClient.connect(MQTT_BROKER_IP, MQTT_PORT)) {
    Serial.println("SUCCESS!");
    testClient.stop();
    return true;
  } else {
    Serial.println("FAILED!");
    return false;
  }
}

// Get human-readable MQTT error description
String getMqttErrorDescription(int rc) {
  switch(rc) {
    case -4: return "Connection timeout";
    case -3: return "Connection lost";
    case -2: return "Connect failed";
    case -1: return "Disconnected";
    case 0: return "Connected";
    case 1: return "Bad protocol version";
    case 2: return "Bad client ID";
    case 3: return "Unavailable";
    case 4: return "Bad credentials";
    case 5: return "Unauthorized";
    default: return "Unknown error";
  }
}

// ==================== MQTT MESSAGE HANDLER ====================
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  Serial.print("MQTT message received on topic: ");
  Serial.println(topic);
  
  // Convert payload to string
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.print("Message: ");
  Serial.println(message);
  
  // Parse JSON
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }
  
  handleCommand(doc);
}

// ==================== COMMAND HANDLER ====================
void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  Serial.print("Handling command: ");
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
        
        Serial.print("Set position ");
        Serial.print(position);
        Serial.print(" to color ");
        Serial.print(colorStr);
        Serial.print(" brightness ");
        Serial.println(brightness);
      }
    }
    
  } else if (action == "read_sensors") {
    // Send immediate sensor reading
    Serial.println("Sending immediate sensor reading");
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
  Serial.println("Reading all position switches...");
  for (int i = 0; i < 9; i++) {
    // LOW = bottle present (switch closed), HIGH = empty (switch open)
    bool occupied = (digitalRead(SWITCH_PINS[i]) == LOW);
    drawerState.positions[i] = occupied;
    
    Serial.print("Position ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(occupied ? "OCCUPIED" : "EMPTY");
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
        Serial.print(" changed to: ");
        Serial.println(currentState ? "OCCUPIED" : "EMPTY");
        
        // Update state
        drawerState.positions[i] = currentState;
        lastPositions[i] = currentState;
        
        // Send bottle event
        sendBottleEvent(i + 1, currentState);
        
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
  
  uint8_t index = position - 1; // Convert to 0-based index
  
  // Apply brightness scaling
  uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
  uint8_t b = (color & 0xFF) * brightness / 255;
  
  strip.setPixelColor(index, r, g, b);
  strip.show();
}

void testLEDsOnStartup() {
  Serial.println("Testing LEDs...");
  
  // Light up each position briefly
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
  }
  
  delay(500);
  
  // Turn all off
  strip.clear();
  strip.show();
  
  Serial.println("LED test complete");
}

void testAllLEDs() {
  // Cycle through colors
  Serial.println("LED test: Red");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_ERROR, 100);
  }
  delay(1000);
  
  Serial.println("LED test: Green");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_SUCCESS, 100);
  }
  delay(1000);
  
  Serial.println("LED test: Blue");
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
  }
  delay(1000);
  
  Serial.println("LED test: Off");
  strip.clear();
  strip.show();
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
  
  // Add position data
  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    JsonObject pos = positions.createNestedObject();
    pos["position"] = i + 1;
    pos["occupied"] = drawerState.positions[i];
    // Weight will be added in Phase 2
  }
  
  publishStatus(doc);
  
  Serial.print("Heartbeat sent - ");
  Serial.print("Uptime: ");
  Serial.print(drawerState.uptime / 1000);
  Serial.println(" seconds");
}

void sendBottleEvent(uint8_t position, bool occupied) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["action"] = "bottle_event";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["event"] = occupied ? "placed" : "removed";
  data["position"] = position;
  // Weight will be added in Phase 2
  
  publishStatus(doc);
  
  Serial.print("Bottle event sent: Position ");
  Serial.print(position);
  Serial.print(" ");
  Serial.println(occupied ? "PLACED" : "REMOVED");
}

void publishStatus(JsonDocument& doc) {
  String output;
  serializeJson(doc, output);
  
  String topic = String("winefridge/") + DRAWER_ID + "/status";
  
  if (mqttClient.publish(topic.c_str(), output.c_str())) {
    Serial.println("Status published successfully");
  } else {
    Serial.println("Failed to publish status");
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
    else if (cmd == "PING") {
      testBrokerConnection();
    }
    else if (cmd == "MQTT") {
      connectMQTT();
    }
    else if (cmd.startsWith("LED")) {
      // Example: LED 3 BLUE
      handleLEDCommand(cmd);
    }
    else if (cmd == "HELP") {
      printHelp();
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

void printStatus() {
  Serial.println("\n=== DRAWER STATUS ===");
  Serial.print("ID: ");
  Serial.println(DRAWER_ID);
  Serial.print("WiFi: ");
  Serial.print(WiFi.isConnected() ? "Connected" : "Disconnected");
  if (WiFi.isConnected()) {
    Serial.print(" (RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.print(" dBm, IP: ");
    Serial.print(WiFi.localIP());
    Serial.println(")");
  } else {
    Serial.println();
  }
  Serial.print("MQTT: ");
  Serial.println(mqttClient.connected() ? "Connected" : "Disconnected");
  Serial.print("Uptime: ");
  Serial.print(millis() / 1000);
  Serial.println(" seconds");
  
  Serial.println("\nPositions:");
  for (int i = 0; i < 9; i++) {
    Serial.print("  ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.println(drawerState.positions[i] ? "OCCUPIED" : "EMPTY");
  }
  Serial.println("====================\n");
}

void printHelp() {
  Serial.println("\n=== SERIAL COMMANDS ===");
  Serial.println("STATUS     - Show system status");
  Serial.println("HEARTBEAT  - Send heartbeat now");
  Serial.println("TEST       - Test all LEDs");
  Serial.println("POSITIONS  - Read all positions");
  Serial.println("PING       - Test connection to broker");
  Serial.println("MQTT       - Reconnect to MQTT");
  Serial.println("LED 3 BLUE - Set position 3 blue");
  Serial.println("LED 5 OFF  - Turn off position 5");
  Serial.println("HELP       - Show this help");
  Serial.println("========================\n");
}