/*
 * WineFridge Lighting ESP32 - SIMPLIFIED & ROBUST
 * ESP32_LIGHTING v23 - 15.10.2025 11.30h
 * - Heartbeat every 90s with minimal data
 * - Reduced debug (critical + events only)
 * - Consistent with ESP_DRAWER v33 style
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>

// ==================== CONFIGURATION ====================
// CHANGE THESE FOR EACH PCB:
#define LIGHTING_ID "lighting_1"    // lighting_1, lighting_2, lighting_3
#define FIRST_DRAWER 1             // PCB #1: 1, PCB #2: 4, PCB #3: 7
#define SECOND_DRAWER 2             // PCB #1: 2, PCB #2: 6, PCB #3: 8
#define ZONE_NUMBER 1               // PCB #1: 1, PCB #2: 2, PCB #3: 3

// Network
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing
#define HEARTBEAT_INTERVAL 90000
#define CONNECTION_CHECK_INTERVAL 10000

// Hardware - SAME PINS FOR ALL PCBs
#define DRAWER1_LED_PIN    18
#define DRAWER2_LED_PIN    12
#define DRAWER1_WARM_PIN   19
#define DRAWER1_COOL_PIN   21
#define DRAWER2_WARM_PIN   32
#define DRAWER2_COOL_PIN   33
#define ZONE_WARM_PIN      14
#define ZONE_COOL_PIN      13

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

CRGB drawer1_leds[LEDS_PER_DRAWER];
CRGB drawer2_leds[LEDS_PER_DRAWER];

struct {
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
  } drawer1;
  
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
  } drawer2;
  
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
  } zone;
  
  bool lightingReady = false;
} state;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[STARTUP] WineFridge Lighting - " + String(LIGHTING_ID));
  Serial.println("[CONFIG] Drawers: " + String(FIRST_DRAWER) + "," + String(SECOND_DRAWER) + " | Zone: " + String(ZONE_NUMBER));

  // Configure BOOT pins immediately
  pinMode(ZONE_WARM_PIN, OUTPUT);
  digitalWrite(ZONE_WARM_PIN, LOW);
  pinMode(ZONE_COOL_PIN, OUTPUT); 
  digitalWrite(ZONE_COOL_PIN, LOW);
  delay(100);
  
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", LIGHTING_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", LIGHTING_ID);
  
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
  
  ledcWrite(DRAWER1_WARM_PIN, 0);
  ledcWrite(DRAWER1_COOL_PIN, 0);
  ledcWrite(DRAWER2_WARM_PIN, 0);
  ledcWrite(DRAWER2_COOL_PIN, 0);
  ledcWrite(ZONE_WARM_PIN, 0);
  ledcWrite(ZONE_COOL_PIN, 0);
  
  state.lightingReady = true;
  Serial.println("[OK] Lighting hardware");
  
  setupNetwork();
  
  Serial.println("[READY] System initialized");
}

// ==================== NETWORK ====================
void setupNetwork() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  Serial.print("[WIFI] Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[OK] WiFi connected: " + WiFi.localIP().toString());
    
    mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
    mqttClient.setCallback(onMQTTMessage);
    mqttClient.setBufferSize(2048);
    
    connectMQTT();
  } else {
    Serial.println("\n[ERROR] WiFi connection failed");
  }
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String clientId = String(LIGHTING_ID) + "-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("[OK] MQTT connected");
    mqttClient.subscribe(mqtt_topic_command);
  } else {
    Serial.println("[ERROR] MQTT connection failed: " + String(mqttClient.state()));
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  yield();
  
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > CONNECTION_CHECK_INTERVAL) {
    maintainConnections();
    lastConnectionCheck = millis();
  }
  
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }
  
  delay(10);
}

void maintainConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[ERROR] WiFi lost, reconnecting");
    WiFi.reconnect();
    return;
  }
  
  if (!mqttClient.connected()) {
    Serial.println("[ERROR] MQTT lost, reconnecting");
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
    return;
  }
  
  for (int i = 0; i < LEDS_PER_DRAWER; i++) {
    led_array[i] = CRGB::Black;
  }
  
  bool any_active = false;
  
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
  } else if (drawer == SECOND_DRAWER) {
    setCOBLighting(DRAWER2_WARM_PIN, DRAWER2_COOL_PIN, temperature, brightness);
    state.drawer2.temperature = temperature;
    state.drawer2.brightness = brightness;
  }
}

void setZoneLight(uint8_t zone, uint16_t temperature, uint8_t brightness) {
  if (zone != ZONE_NUMBER) return;
  
  setCOBLighting(ZONE_WARM_PIN, ZONE_COOL_PIN, temperature, brightness);
  state.zone.temperature = temperature;
  state.zone.brightness = brightness;
}

void allLightsOff() {
  FastLED.clear();
  FastLED.show();
  
  ledcWrite(DRAWER1_WARM_PIN, 0);
  ledcWrite(DRAWER1_COOL_PIN, 0);
  ledcWrite(DRAWER2_WARM_PIN, 0);
  ledcWrite(DRAWER2_COOL_PIN, 0);
  ledcWrite(ZONE_WARM_PIN, 0);
  ledcWrite(ZONE_COOL_PIN, 0);
  
  state.drawer1.brightness = 0;
  state.drawer1.leds_active = false;
  state.drawer2.brightness = 0;
  state.drawer2.leds_active = false;
  state.zone.brightness = 0;
  
  Serial.println("[CMD] All lights OFF");
}

// ==================== MQTT ====================
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  if (length > 1500) return;
  
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
    
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, message) == DeserializationError::Ok) {
    handleCommand(doc);
  }
}

void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  if (action == "set_leds") {
    uint8_t drawer = data["drawer"];
    JsonArray positions = data["positions"];
    setDrawerLEDs(drawer, positions);
    Serial.println("[CMD] Set LEDs drawer " + String(drawer));
  }
  else if (action == "set_general_light") {
    uint8_t drawer = data["drawer"];
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setDrawerGeneral(drawer, temperature, brightness);
    Serial.println("[CMD] Set light drawer " + String(drawer) + ": " + String(temperature) + "K " + String(brightness) + "%");
  }
  else if (action == "set_zone_light") {
    uint8_t zone = data["zone"];
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setZoneLight(zone, temperature, brightness);
    Serial.println("[CMD] Set zone " + String(zone) + ": " + String(temperature) + "K " + String(brightness) + "%");
  }
  else if (action == "all_lights_off") {
    allLightsOff();
  }
  else if (action == "read_sensors") {
    sendHeartbeat();
  }
}

void sendHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<384> doc;
  doc["action"] = "heartbeat";
  doc["source"] = LIGHTING_ID;
  
  JsonObject data = doc.createNestedObject("data");
  
  // Drawer 1: {"id":1,"light":"4000K 0%","leds":0}
  JsonObject d1 = data.createNestedObject("d1");
  d1["id"] = FIRST_DRAWER;
  String light1 = String(state.drawer1.temperature) + "K " + String(state.drawer1.brightness) + "%";
  d1["light"] = light1;
  d1["leds"] = state.drawer1.leds_active ? 1 : 0;
  
  // Drawer 2
  JsonObject d2 = data.createNestedObject("d2");
  d2["id"] = SECOND_DRAWER;
  String light2 = String(state.drawer2.temperature) + "K " + String(state.drawer2.brightness) + "%";
  d2["light"] = light2;
  d2["leds"] = state.drawer2.leds_active ? 1 : 0;
  
  // Zone
  JsonObject zone = data.createNestedObject("zone");
  zone["id"] = ZONE_NUMBER;
  String lightZone = String(state.zone.temperature) + "K " + String(state.zone.brightness) + "%";
  zone["light"] = lightZone;
  
  data["wifi"] = WiFi.RSSI();
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(mqtt_topic_status, output.c_str());
  Serial.println("[HB] " + output);
}