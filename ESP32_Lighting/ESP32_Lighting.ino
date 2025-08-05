/*
 * WineFridge Lighting ESP32 - Production Version
 * Compatible with ESP32_DRAWER v21 and RPI MQTT system
 * ESP32_LIGHTING v22 - 05.08.2025 15.55h
    
    Connections (OPTIMIZED FOR EASY WIRING):
    Drawer 1 LEDs - D18 ✅ (LEFT SIDE)
    Drawer 2 LEDs - D12 ✅ (RIGHT SIDE - grouped with D13,D14)
    
    Drawer 1 Warm - D19 ✅ (LEFT SIDE)
    Drawer 1 Cool - D21 ✅ (LEFT SIDE)
    Drawer 2 Warm - D32 ✅ (RIGHT SIDE)
    Drawer 2 Cool - D33 ✅ (RIGHT SIDE)
    
    Zone Warm - D14 ✅ (RIGHT SIDE - grouped with D12,D13)
    Zone Cool - D13 ✅ (RIGHT SIDE - grouped with D12,D14)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>

// ==================== CONFIGURATION ====================
// CHANGE THESE 3 LINES FOR EACH PCB:
#define LIGHTING_ID "lighting_1"    // lighting_1, lighting_2, lighting_3
#define FIRST_DRAWER 4              // PCB #1: 4, PCB #2: 6, PCB #3: 8
#define SECOND_DRAWER 5             // PCB #1: 5, PCB #2: 7, PCB #3: 9
#define ZONE_NUMBER 1               // PCB #1: 1, PCB #2: 2, PCB #3: 3

// Network - SAME AS DRAWER
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing - SAME AS DRAWER
#define HEARTBEAT_INTERVAL 30000
#define CONNECTION_CHECK_INTERVAL 10000
#define DEBUG_STATUS_INTERVAL 30000

// Hardware - SAME PINS FOR ALL PCBs
#define DRAWER1_LED_PIN    18    // First drawer WS2812B (LEFT SIDE)
#define DRAWER2_LED_PIN    12    // Second drawer WS2812B (RIGHT SIDE - near D13,D14)
#define DRAWER1_WARM_PIN   19    // First drawer warm COB (LEFT SIDE)
#define DRAWER1_COOL_PIN   21    // First drawer cool COB (LEFT SIDE)
#define DRAWER2_WARM_PIN   32    // Second drawer warm COB (RIGHT SIDE)
#define DRAWER2_COOL_PIN   33    // Second drawer cool COB (RIGHT SIDE)
#define ZONE_WARM_PIN      14    // Zone warm COB (RIGHT SIDE - near D12,D13)
#define ZONE_COOL_PIN      13    // Zone cool COB (RIGHT SIDE - near D12,D14)

// Constants
#define LEDS_PER_DRAWER 9
#define MAX_BRIGHTNESS 150
#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
#define TEMP_MIN 2700
#define TEMP_MAX 6500

// ==================== GLOBALS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// LED Arrays
CRGB drawer1_leds[LEDS_PER_DRAWER];
CRGB drawer2_leds[LEDS_PER_DRAWER];

struct {
  // Drawer 1 state
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
  } drawer1;
  
  // Drawer 2 state  
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
  } drawer2;
  
  // Zone state
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
  } zone;
  
  // System
  unsigned long uptime = 0;
  bool lightingReady = false;
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
  Serial.println("\n=== LIGHTING STATUS ===");
  Serial.printf("WiFi: %s (RSSI: %d)\n", WiFi.isConnected() ? "OK" : "FAIL", WiFi.RSSI());
  Serial.printf("MQTT: %s\n", mqttClient.connected() ? "OK" : "FAIL");
  Serial.printf("Lighting: %s\n", state.lightingReady ? "OK" : "FAIL");
  Serial.printf("Drawer %d: %dK, %d%%, LEDs: %s\n", FIRST_DRAWER, state.drawer1.temperature, state.drawer1.brightness, state.drawer1.leds_active ? "ON" : "OFF");
  Serial.printf("Drawer %d: %dK, %d%%, LEDs: %s\n", SECOND_DRAWER, state.drawer2.temperature, state.drawer2.brightness, state.drawer2.leds_active ? "ON" : "OFF");
  Serial.printf("Zone %d: %dK, %d%%\n", ZONE_NUMBER, state.zone.temperature, state.zone.brightness);
  Serial.printf("Uptime: %lu minutes\n", millis() / 60000);
  Serial.println("==================\n");
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  debugPrint("=== WineFridge Lighting Starting ===");
  debugPrint("Controller: " + String(LIGHTING_ID));
  debugPrint("Drawers: " + String(FIRST_DRAWER) + "-" + String(SECOND_DRAWER) + ", Zone: " + String(ZONE_NUMBER));

  // Configure BOOT pins immediately
  pinMode(ZONE_WARM_PIN, OUTPUT);
  digitalWrite(ZONE_WARM_PIN, LOW);
  pinMode(ZONE_COOL_PIN, OUTPUT); 
  digitalWrite(ZONE_COOL_PIN, LOW);
  delay(100);
  
  // MQTT topics
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", LIGHTING_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", LIGHTING_ID);
  
  // Hardware initialization
  debugPrint("Initializing hardware...");
  
  // LED strips
  FastLED.addLeds<WS2812, DRAWER1_LED_PIN, GRB>(drawer1_leds, LEDS_PER_DRAWER);
  FastLED.addLeds<WS2812, DRAWER2_LED_PIN, GRB>(drawer2_leds, LEDS_PER_DRAWER);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  
  // COB LEDs
  ledcAttach(DRAWER1_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(DRAWER1_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(DRAWER2_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(DRAWER2_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(ZONE_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(ZONE_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  
  // Initialize all COB LEDs to OFF
  ledcWrite(DRAWER1_WARM_PIN, 0);
  ledcWrite(DRAWER1_COOL_PIN, 0);
  ledcWrite(DRAWER2_WARM_PIN, 0);
  ledcWrite(DRAWER2_COOL_PIN, 0);
  ledcWrite(ZONE_WARM_PIN, 0);
  ledcWrite(ZONE_COOL_PIN, 0);
  
  state.lightingReady = true;
  debugPrint("Lighting hardware: OK");
  
  // Network setup
  setupNetwork();
  
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
  String clientId = String(LIGHTING_ID) + "-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    debugPrint("MQTT connected!");
    mqttClient.subscribe(mqtt_topic_command);
  } else {
    debugPrint("MQTT connection failed! State: " + String(mqttClient.state()));
  }
}

// ==================== MAIN LOOP ====================
void loop() {
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

// ==================== LIGHTING CONTROL ====================
void setDrawerLEDs(uint8_t drawer, JsonArray positions) {
  CRGB* led_array = nullptr;
  bool* led_state = nullptr;
  
  if (drawer == FIRST_DRAWER) {
    led_array = drawer1_leds;
    led_state = &state.drawer1.leds_active;
  } else if (drawer == SECOND_DRAWER) {
    led_array = drawer2_leds;
    led_state = &state.drawer2.leds_active;
  } else {
    debugPrint("Drawer " + String(drawer) + " not controlled by this PCB");
    return;
  }
  
  // Clear all positions first
  for (int i = 0; i < LEDS_PER_DRAWER; i++) {
    led_array[i] = CRGB::Black;
  }
  
  bool any_active = false;
  
  // Set specified positions
  for (JsonObject pos : positions) {
    uint8_t position = pos["position"];
    String colorStr = pos["color"];
    uint8_t brightness = pos.containsKey("brightness") ? pos["brightness"] : 255;
    
    if (position >= 1 && position <= 9) {
      uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
      uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
      uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
      uint8_t b = (color & 0xFF) * brightness / 255;
      
      led_array[position - 1] = CRGB(r, g, b);
      any_active = true;
      
      debugPrint("Set drawer " + String(drawer) + " position " + String(position) + 
                 " to RGB(" + String(r) + "," + String(g) + "," + String(b) + ")");
    }
  }
  
  *led_state = any_active;
  FastLED.show();
}

void setCOBLighting(uint8_t warmPin, uint8_t coolPin, uint16_t temperature, uint8_t brightness) {
  temperature = constrain(temperature, TEMP_MIN, TEMP_MAX);
  brightness = constrain(brightness, 0, 100);
  
  if (brightness == 0) {
    ledcWrite(warmPin, 0);
    ledcWrite(coolPin, 0);
    return;
  }
  
  // Calculate warm/cool mix
  float warmRatio = map(temperature, TEMP_MIN, TEMP_MAX, 100, 0) / 100.0;
  float coolRatio = 1.0 - warmRatio;
  
  uint8_t maxPWM = map(brightness, 0, 100, 0, 255);
  uint8_t warmPWM = maxPWM * warmRatio;
  uint8_t coolPWM = maxPWM * coolRatio;
  
  ledcWrite(warmPin, warmPWM);
  ledcWrite(coolPin, coolPWM);
}

void setDrawerGeneral(uint8_t drawer, uint16_t temperature, uint8_t brightness) {
  if (drawer == FIRST_DRAWER) {
    setCOBLighting(DRAWER1_WARM_PIN, DRAWER1_COOL_PIN, temperature, brightness);
    state.drawer1.temperature = temperature;
    state.drawer1.brightness = brightness;
    debugPrint("Drawer " + String(drawer) + " general: " + String(temperature) + "K, " + String(brightness) + "%");
  } else if (drawer == SECOND_DRAWER) {
    setCOBLighting(DRAWER2_WARM_PIN, DRAWER2_COOL_PIN, temperature, brightness);
    state.drawer2.temperature = temperature;
    state.drawer2.brightness = brightness;
    debugPrint("Drawer " + String(drawer) + " general: " + String(temperature) + "K, " + String(brightness) + "%");
  }
}

void setZoneLight(uint8_t zone, uint16_t temperature, uint8_t brightness) {
  if (zone != ZONE_NUMBER) {
    debugPrint("Zone " + String(zone) + " not controlled by this PCB");
    return;
  }
  
  setCOBLighting(ZONE_WARM_PIN, ZONE_COOL_PIN, temperature, brightness);
  state.zone.temperature = temperature;
  state.zone.brightness = brightness;
  debugPrint("Zone " + String(zone) + ": " + String(temperature) + "K, " + String(brightness) + "%");
}

void allLightsOff() {
  debugPrint("Turning all lights OFF");
  
  // Turn off all LEDs
  FastLED.clear();
  FastLED.show();
  
  // Turn off all COB LEDs
  ledcWrite(DRAWER1_WARM_PIN, 0);
  ledcWrite(DRAWER1_COOL_PIN, 0);
  ledcWrite(DRAWER2_WARM_PIN, 0);
  ledcWrite(DRAWER2_COOL_PIN, 0);
  ledcWrite(ZONE_WARM_PIN, 0);
  ledcWrite(ZONE_COOL_PIN, 0);
  
  // Update state
  state.drawer1.brightness = 0;
  state.drawer1.leds_active = false;
  state.drawer2.brightness = 0;
  state.drawer2.leds_active = false;
  state.zone.brightness = 0;
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
    uint8_t drawer = data["drawer"];
    JsonArray positions = data["positions"];
    setDrawerLEDs(drawer, positions);
  }
  else if (action == "set_general_light") {
    uint8_t drawer = data["drawer"];
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setDrawerGeneral(drawer, temperature, brightness);
  }
  else if (action == "set_zone_light") {
    uint8_t zone = data["zone"];
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setZoneLight(zone, temperature, brightness);
  }
  else if (action == "all_lights_off") {
    allLightsOff();
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
  doc["source"] = LIGHTING_ID;
  doc["timestamp"] = millis();
  
  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = state.uptime;
  data["wifi_rssi"] = WiFi.RSSI();
  data["lighting_ready"] = state.lightingReady;
  data["free_heap"] = ESP.getFreeHeap();
  
  // Drawer 1 status
  JsonObject drawer1 = data.createNestedObject("drawer1");
  drawer1["id"] = FIRST_DRAWER;
  drawer1["temperature"] = state.drawer1.temperature;
  drawer1["brightness"] = state.drawer1.brightness;
  drawer1["leds_active"] = state.drawer1.leds_active;
  
  // Drawer 2 status
  JsonObject drawer2 = data.createNestedObject("drawer2");
  drawer2["id"] = SECOND_DRAWER;
  drawer2["temperature"] = state.drawer2.temperature;
  drawer2["brightness"] = state.drawer2.brightness;
  drawer2["leds_active"] = state.drawer2.leds_active;
  
  // Zone status
  JsonObject zone = data.createNestedObject("zone");
  zone["id"] = ZONE_NUMBER;
  zone["temperature"] = state.zone.temperature;
  zone["brightness"] = state.zone.brightness;
  
  String output;
  serializeJson(doc, output);
  
  bool sent = mqttClient.publish(mqtt_topic_status, output.c_str());
  debugPrint("Heartbeat " + String(sent ? "SENT" : "FAILED"));
}