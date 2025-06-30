/*
 * WineFridge Drawer ESP32 - Combined Best Version
 * Combines simplicity of v2.1 with robustness of v10 ---> 13.36h - 30.06.2025
 * Simple, efficient, but with good debugging capabilities
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

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
#define WS2812_DATA_PIN 13
#define NUM_LEDS 9

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
    Serial.println("Sending immediate heartbeat");
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
        Serial.print(" → ");
        Serial.println(currentState ? "OCCUPIED" : "EMPTY");
        
        // Update state
        drawerState.positions[i] = currentState;
        lastPositions[i] = currentState;
        
        // Send bottle event to RPI
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
  Serial.print("Testing LEDs");
  
  // Light up each position briefly
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
    Serial.print(".");
  }
  
  delay(500);
  
  // Turn all off
  strip.clear();
  strip.show();
  Serial.println(" done");
}

void testAllLEDs() {
  // Cycle through colors for debugging
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
    // Weight will be added in future phases
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

void sendBottleEvent(uint8_t position, bool occupied) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["action"] = "bottle_event";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["event"] = occupied ? "placed" : "removed";
  data["position"] = position;
  // Weight will be added in future phases
  
  String output;
  serializeJson(doc, output);
  
  if (mqttClient.publish(mqtt_topic_status, output.c_str())) {
    Serial.print("Event sent: Position ");
    Serial.print(position);
    Serial.print(" ");
    Serial.println(occupied ? "PLACED" : "REMOVED");
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
    else if (cmd == "PING") {
      connectMQTT();
    }
    else if (cmd == "WIFI") {
      connectWiFi();
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
  Serial.println("PING       - Reconnect MQTT");
  Serial.println("WIFI       - Reconnect WiFi");
  Serial.println("LED 3 BLUE - Set position 3 blue");
  Serial.println("LED 5 OFF  - Turn off position 5");
  Serial.println("HELP       - Show this help");
  Serial.println("========================\n");
}
