/*
 * WineFridge Drawer ESP32 - FIXED INDIVIDUAL WEIGHTS
 * Fixed: Calculate individual bottle weights from platform weight differences
 * Claude INDIVIDUAL WEIGHTS FIXED - 13.15 * 01.07.2025
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HX711.h>

// ==================== CONFIGURATION ====================
#define DRAWER_ID "drawer_1"

// WiFi Configuration
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"

// MQTT Configuration
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing
#define HEARTBEAT_INTERVAL 30000
#define SENSOR_READ_INTERVAL 1000
#define DEBOUNCE_TIME 50
#define CONNECTION_CHECK_INTERVAL 10000

// Hardware Pins
const uint8_t SWITCH_PINS[9] = {4, 5, 23, 14, 27, 26, 12, 33, 32};
const uint8_t bottleToLed[9] = {32, 28, 24, 20, 16, 12, 8, 4, 0};

#define WS2812_DATA_PIN 13
#define NUM_LEDS 33
#define HX711_DT_PIN 19
#define HX711_SCK_PIN 18

// Weight Configuration
#define WEIGHT_CALIBRATION_FACTOR 94.302184
#define WEIGHT_THRESHOLD 50.0
#define WEIGHT_STABILIZATION_TIME 2000
// MAX_WEIGHT removed - not needed for platform setup

// LED Colors
#define COLOR_AVAILABLE 0x0000FF
#define COLOR_SUCCESS   0x00FF00
#define COLOR_ERROR     0xFF0000
#define COLOR_OCCUPIED  0x444444
#define COLOR_OFF       0x000000

// ==================== GLOBAL OBJECTS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;

// ==================== STATE ====================
struct DrawerState {
  bool positions[9];
  float individualWeights[9];    // Individual bottle weights
  float lastTotalWeight;         // Last known total platform weight
  unsigned long lastHeartbeat;
  unsigned long uptime;
  bool weightSensorReady;
} drawerState;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== WineFridge Drawer - INDIVIDUAL WEIGHTS FIXED ===");
  Serial.print("Drawer ID: ");
  Serial.println(DRAWER_ID);
  
  // Initialize state
  memset(&drawerState, 0, sizeof(drawerState));
  
  // MQTT topics
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  // Position switches
  Serial.println("→ Position switches...");
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    drawerState.positions[i] = false;
  }
  Serial.println("✓ Switches ready");
  
  // LED strip
  Serial.println("→ LED strip...");
  strip.begin();
  strip.clear();
  strip.show();
  testLEDsStartup();
  Serial.println("✓ LEDs ready");
  
  // Weight sensor with SMART TARE
  Serial.println("→ Weight sensor...");
  initWeightSensor();
  
  // Network
  connectWiFi();
  if (WiFi.status() == WL_CONNECTED) {
    setupMQTT();
    connectMQTT();
  }
  
  // Initial reading
  readAllPositions();
  drawerState.lastHeartbeat = millis();
  
  Serial.println("✓ System Ready!");
  if (mqttClient.connected()) sendHeartbeat();
}

// ==================== MAIN LOOP ====================
void loop() {
  // Connection maintenance (every 10s)
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    if (!WiFi.isConnected()) connectWiFi();
    if (WiFi.isConnected() && !mqttClient.connected()) connectMQTT();
    lastConnectionCheck = millis();
  }
  
  if (mqttClient.connected()) mqttClient.loop();
  
  drawerState.uptime = millis();
  
  // Position monitoring
  static unsigned long lastPositionCheck = 0;
  if (millis() - lastPositionCheck > SENSOR_READ_INTERVAL) {
    checkPositionChanges();
    lastPositionCheck = millis();
  }
  
  // Heartbeat
  if (mqttClient.connected() && millis() - drawerState.lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    drawerState.lastHeartbeat = millis();
  }
  
  // Serial commands
  handleSerialCommands();
}

// ==================== WEIGHT SENSOR - SIMPLIFIED & FIXED ====================
void initWeightSensor() {
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  
  if (scale.is_ready()) {
    scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
    
    // SMART TARE: Only tare if no bottles detected
    bool anyBottlesPresent = false;
    for (int i = 0; i < 9; i++) {
      if (digitalRead(SWITCH_PINS[i]) == LOW) {
        anyBottlesPresent = true;
        break;
      }
    }
    
    if (!anyBottlesPresent) {
      scale.tare();
      Serial.println("✓ Weight sensor ready (auto-tared)");
      drawerState.lastTotalWeight = 0.0;
    } else {
      Serial.println("✓ Weight sensor ready (bottles detected, no tare)");
      Serial.println("  Use TARE command manually if needed");
      drawerState.lastTotalWeight = readTotalWeight();
    }
    
    drawerState.weightSensorReady = true;
    Serial.print("✓ Calibration factor: ");
    Serial.println(WEIGHT_CALIBRATION_FACTOR);
  } else {
    Serial.println("⚠ Weight sensor not responding");
    drawerState.weightSensorReady = false;
  }
}

float readTotalWeight() {
  if (!drawerState.weightSensorReady) return 0.0;
  
  // Read total platform weight
  float totalWeight = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 5; i++) {
    if (scale.is_ready()) {
      float reading = scale.get_units(1);
      
      // Only basic validation: reject extreme negatives and invalid numbers
      if (reading >= -100 && !isnan(reading) && !isinf(reading)) {
        totalWeight += reading;
        validReadings++;
      }
    }
    delay(20);
  }
  
  if (validReadings > 0) {
    float average = totalWeight / validReadings;
    return max(0.0f, average);  // Never return negative
  }
  
  return 0.0;
}

float readStabilizedTotalWeight() {
  Serial.print("Weighing platform");
  
  // 2-second delay before weighing (as requested)
  delay(WEIGHT_STABILIZATION_TIME);
  
  float weight1 = readTotalWeight();
  Serial.print(".");
  delay(500);
  float weight2 = readTotalWeight();
  Serial.print(".");
  
  Serial.print(" (");
  Serial.print(weight1);
  Serial.print("g → ");
  Serial.print(weight2);
  Serial.print("g)");
  
  // Use second reading (more stable)
  if (abs(weight2 - weight1) < WEIGHT_THRESHOLD) {
    Serial.println(" - Stable");
    return weight2;
  } else {
    Serial.println(" - Average");
    return (weight1 + weight2) / 2.0;
  }
}

// ==================== POSITION MONITORING ====================
void readAllPositions() {
  // Read current total weight
  float currentTotalWeight = readTotalWeight();
  drawerState.lastTotalWeight = currentTotalWeight;
  
  Serial.print("Initial platform weight: ");
  Serial.print(currentTotalWeight);
  Serial.println("g");
  
  for (int i = 0; i < 9; i++) {
    bool occupied = (digitalRead(SWITCH_PINS[i]) == LOW);
    drawerState.positions[i] = occupied;
    
    if (occupied) {
      // For initial setup, we can't determine individual weights
      // We'll need manual intervention or existing data
      drawerState.individualWeights[i] = 0.0;  // Unknown individual weight
      Serial.print("Position ");
      Serial.print(i + 1);
      Serial.println(": OCCUPIED (weight unknown at startup)");
    } else {
      drawerState.individualWeights[i] = 0.0;
      Serial.print("Position ");
      Serial.print(i + 1);
      Serial.println(": EMPTY");
    }
  }
}

void checkPositionChanges() {
  static bool lastPositions[9] = {false};
  
  for (int i = 0; i < 9; i++) {
    bool currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
    
    if (currentState != lastPositions[i]) {
      delay(DEBOUNCE_TIME);
      currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
      
      if (currentState != lastPositions[i]) {
        Serial.print("Position ");
        Serial.print(i + 1);
        Serial.print(" → ");
        Serial.println(currentState ? "OCCUPIED" : "EMPTY");
        
        drawerState.positions[i] = currentState;
        lastPositions[i] = currentState;
        
        float individualWeight = 0.0;
        
        if (currentState) {
          // Bottle PLACED - calculate individual weight
          float newTotalWeight = readStabilizedTotalWeight();
          individualWeight = newTotalWeight - drawerState.lastTotalWeight;
          
          // Store individual weight and update total
          drawerState.individualWeights[i] = individualWeight;
          drawerState.lastTotalWeight = newTotalWeight;
          
          Serial.print("✓ Bottle placed: ");
          Serial.print(individualWeight);
          Serial.print("g (total: ");
          Serial.print(newTotalWeight);
          Serial.println("g)");
          
          setPositionLED(i + 1, COLOR_OCCUPIED, 50);
        } else {
          // Bottle REMOVED - use stored individual weight
          individualWeight = drawerState.individualWeights[i];
          
          // Update total weight (should decrease)
          float newTotalWeight = readStabilizedTotalWeight();
          drawerState.lastTotalWeight = newTotalWeight;
          drawerState.individualWeights[i] = 0.0;  // Clear individual weight
          
          Serial.print("✓ Bottle removed: ");
          Serial.print(individualWeight);
          Serial.print("g (total: ");
          Serial.print(newTotalWeight);
          Serial.println("g)");
          
          setPositionLED(i + 1, COLOR_OFF, 0);
        }
        
        sendBottleEvent(i + 1, currentState, individualWeight);
      }
    }
  }
}

// ==================== LED CONTROL ====================
void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness) {
  if (position < 1 || position > 9) return;
  
  uint8_t ledIndex = bottleToLed[position - 1];
  uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
  uint8_t b = (color & 0xFF) * brightness / 255;
  
  strip.setPixelColor(ledIndex, r, g, b);
  strip.show();
}

void testLEDsStartup() {
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
  }
  delay(500);
  strip.clear();
  strip.show();
}

// ==================== NETWORK ====================
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  Serial.println("→ Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi connected");
    Serial.print("✓ IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n✗ WiFi failed");
  }
}

void setupMQTT() {
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqttClient.setCallback(onMQTTMessage);
  mqttClient.setBufferSize(1024);
}

void connectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;
  
  Serial.println("→ Connecting MQTT...");
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("✓ MQTT connected");
    mqttClient.subscribe(mqtt_topic_command);
  } else {
    Serial.print("✗ MQTT failed (rc=");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
}

// ==================== MQTT MESSAGES ====================
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, message) == DeserializationError::Ok) {
    handleCommand(doc);
  }
}

void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  if (action == "set_leds") {
    JsonArray positions = data["positions"];
    for (JsonObject pos : positions) {
      uint8_t position = pos["position"];
      String colorStr = pos["color"];
      uint8_t brightness = pos["brightness"];
      
      if (position >= 1 && position <= 9) {
        uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
        setPositionLED(position, color, brightness);
      }
    }
  } else if (action == "expect_bottle") {
    uint8_t position = data["position"];
    setPositionLED(position, COLOR_AVAILABLE, 100);
  } else if (action == "read_sensors") {
    sendHeartbeat();
  }
}

void sendHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<1024> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = drawerState.uptime;
  data["wifi_rssi"] = WiFi.RSSI();
  data["weight_sensor_ready"] = drawerState.weightSensorReady;
  data["total_weight"] = drawerState.lastTotalWeight;  // Include total platform weight
  
  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    JsonObject pos = positions.createNestedObject();
    pos["position"] = i + 1;
    pos["occupied"] = drawerState.positions[i];
    pos["weight"] = round(drawerState.individualWeights[i] * 10.0) / 10.0;  // Individual weight
  }
  
  String output;
  serializeJson(doc, output);
  mqttClient.publish(mqtt_topic_status, output.c_str());
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
  data["weight"] = weight;  // Individual bottle weight
  data["total_weight"] = drawerState.lastTotalWeight;  // Total platform weight
  
  String output;
  serializeJson(doc, output);
  mqttClient.publish(mqtt_topic_status, output.c_str());
  
  Serial.print("Event: Position ");
  Serial.print(position);
  Serial.print(" ");
  Serial.print(occupied ? "PLACED" : "REMOVED");
  Serial.print(" (");
  Serial.print(weight);
  Serial.println("g)");
}

// ==================== SERIAL COMMANDS - SIMPLIFIED ====================
void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();
    
    if (cmd == "STATUS") {
      printStatus();
    } else if (cmd == "WEIGHT") {
      testWeight();
    } else if (cmd == "TARE") {
      scale.tare();
      drawerState.lastTotalWeight = 0.0;  // Reset total weight tracking
      Serial.println("✓ Manual tare done - total weight reset");
    } else if (cmd == "CALIBRATE") {
      calibrateWeight();
    } else if (cmd == "HEARTBEAT") {
      sendHeartbeat();
    } else if (cmd.startsWith("LED")) {
      handleLEDCommand(cmd);
    } else if (cmd == "HELP") {
      printHelp();
    }
  }
}

void testWeight() {
  Serial.println("\n=== WEIGHT TEST ===");
  Serial.print("Sensor ready: ");
  Serial.println(drawerState.weightSensorReady ? "YES" : "NO");
  
  if (drawerState.weightSensorReady) {
    Serial.print("Current total weight: ");
    Serial.print(drawerState.lastTotalWeight);
    Serial.println("g");
    
    for (int i = 0; i < 3; i++) {
      Serial.print("Reading ");
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(readTotalWeight());
      Serial.println("g");
      delay(1000);
    }
    
    Serial.println("Stabilized reading:");
    float weight = readStabilizedTotalWeight();
    Serial.print("Final total: ");
    Serial.print(weight);
    Serial.println("g");
  }
  Serial.println("==================\n");
}

void calibrateWeight() {
  Serial.println("\n=== CALIBRATION ===");
  Serial.println("1. Remove all weight");
  Serial.println("2. Press ENTER to tare");
  
  while (!Serial.available()) delay(100);
  Serial.readString();
  
  scale.tare();
  drawerState.lastTotalWeight = 0.0;
  Serial.println("✓ Tared - total weight reset");
  
  Serial.println("3. Place known weight");
  Serial.println("4. Enter weight in grams:");
  
  while (!Serial.available()) delay(100);
  float knownWeight = Serial.parseFloat();
  Serial.readString();
  
  if (knownWeight > 0) {
    float reading = scale.get_units(10);
    
    // CORRECT CALIBRATION FORMULA
    float newFactor = WEIGHT_CALIBRATION_FACTOR * (knownWeight / reading);
    
    Serial.print("Current reading: ");
    Serial.println(reading);
    Serial.print("Known weight: ");
    Serial.println(knownWeight);
    Serial.print("NEW calibration factor: ");
    Serial.println(newFactor);
    Serial.println("Update #define WEIGHT_CALIBRATION_FACTOR in code");
    
    scale.set_scale(newFactor);
    Serial.println("✓ Applied for this session");
  }
  Serial.println("==================\n");
}

void handleLEDCommand(String cmd) {
  int s1 = cmd.indexOf(' ');
  int s2 = cmd.indexOf(' ', s1 + 1);
  
  if (s1 != -1 && s2 != -1) {
    int pos = cmd.substring(s1 + 1, s2).toInt();
    String color = cmd.substring(s2 + 1);
    
    if (pos >= 1 && pos <= 9) {
      if (color == "BLUE") setPositionLED(pos, COLOR_AVAILABLE, 100);
      else if (color == "RED") setPositionLED(pos, COLOR_ERROR, 100);
      else if (color == "GREEN") setPositionLED(pos, COLOR_SUCCESS, 100);
      else if (color == "OFF") setPositionLED(pos, COLOR_OFF, 0);
    }
  }
}

void printStatus() {
  Serial.println("\n=== STATUS ===");
  Serial.print("WiFi: ");
  Serial.println(WiFi.isConnected() ? "OK" : "FAIL");
  Serial.print("MQTT: ");
  Serial.println(mqttClient.connected() ? "OK" : "FAIL");
  Serial.print("Weight: ");
  Serial.println(drawerState.weightSensorReady ? "OK" : "FAIL");
  
  if (drawerState.weightSensorReady) {
    Serial.print("Total platform weight: ");
    Serial.print(drawerState.lastTotalWeight);
    Serial.println("g");
  }
  
  for (int i = 0; i < 9; i++) {
    Serial.print("Pos ");
    Serial.print(i + 1);
    Serial.print(": ");
    if (drawerState.positions[i]) {
      Serial.print("OCCUPIED (");
      Serial.print(drawerState.individualWeights[i]);
      Serial.println("g)");
    } else {
      Serial.println("EMPTY");
    }
  }
  Serial.println("==============\n");
}

void printHelp() {
  Serial.println("\n=== COMMANDS ===");
  Serial.println("STATUS     - System status");
  Serial.println("WEIGHT     - Test total weight");
  Serial.println("TARE       - Zero sensor");
  Serial.println("CALIBRATE  - Calibrate");
  Serial.println("HEARTBEAT  - Send heartbeat");
  Serial.println("LED 3 BLUE - Control LED");
  Serial.println("HELP       - This help");
  Serial.println("================\n");
}
