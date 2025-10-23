/*
 * WineFridge Lighting ESP32 - LIGHTING_2 Controller
 * Located physically in drawer 2
 * Controls: Drawer 4, Drawer 5, Zone 2
 * Version 2.6 - With OTA Support
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "esp_task_wdt.h"

// ==================== CONFIGURATION ====================
#define LIGHTING_ID "lighting_6"      // Controller ID (physical location: drawer 2)
#define FIRMWARE_VERSION "2.6.0"

// This controller manages:
#define FIRST_DRAWER 4          // Drawer 2
#define SECOND_DRAWER 6         // Drawer 1  
#define ZONE_NUMBER 2           // Zone 1 (upper zone)

// Network
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing
#define HEARTBEAT_INTERVAL 90000
#define CONNECTION_CHECK_INTERVAL 10000
#define WATCHDOG_TIMEOUT 30

// Hardware - CORRECT PINS (matching original working code)
#define DRAWER1_LED_PIN    18      // WS2812B strip drawer 2
#define DRAWER2_LED_PIN    12      // WS2812B strip drawer 1
#define LEDS_PER_DRAWER    17      // 17 LEDs per drawer

#define DRAWER1_WARM_PIN   19      // COB warm for drawer 2
#define DRAWER1_COOL_PIN   21      // COB cool for drawer 2
#define DRAWER2_WARM_PIN   32      // COB warm for drawer 1
#define DRAWER2_COOL_PIN   33      // COB cool for drawer 1
#define ZONE_WARM_PIN      14      // Zone 1 warm
#define ZONE_COOL_PIN      13      // Zone 1 cool

// Constants
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

volatile bool otaInProgress = false;

struct {
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
    uint8_t frontBrightness = 100;  // Front LED brightness 0-100
  } drawer1;
  
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
    uint8_t frontBrightness = 100;  // Front LED brightness 0-100
  } drawer2;
  
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
  } zone;
  
  bool lightingReady = false;
  bool debugMode = false;
} state;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== FUNCTION PROTOTYPES ====================
void setupNetwork();
void connectMQTT();
void setupOTA();
void maintainConnections();
void setDrawerLEDs(uint8_t drawer, JsonArray positions);
void setAllDrawerLEDs(uint8_t drawer, String colorStr, uint8_t brightness);
void setCOBLighting(uint8_t warmPin, uint8_t coolPin, uint16_t temperature, uint8_t brightness);
void setDrawerGeneral(uint8_t drawer, uint16_t temperature, uint8_t brightness);
void setZoneLight(uint8_t zone, uint16_t temperature, uint8_t brightness);
void allLightsOff();
void onMQTTMessage(char* topic, byte* payload, unsigned int length);
void handleCommand(JsonDocument& doc);
void sendHeartbeat();
void sendStartupMessage();


// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n[STARTUP] WineFridge Lighting - " + String(LIGHTING_ID));
  Serial.println("[VERSION] Firmware: " + String(FIRMWARE_VERSION));
  Serial.printf("[CONFIG] Drawers: %d,%d | Zone: %d\n", FIRST_DRAWER, SECOND_DRAWER, ZONE_NUMBER);

  // Watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

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
  setupOTA();
  
  Serial.println("[READY] System initialized");
  esp_task_wdt_reset();
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
    esp_task_wdt_reset();
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
    sendStartupMessage();
  } else {
    Serial.println("[ERROR] MQTT connection failed: " + String(mqttClient.state()));
  }
}

// ==================== OTA SETUP ====================
void setupOTA() {
  ArduinoOTA.setHostname(LIGHTING_ID);
  ArduinoOTA.setPassword("winefridge2025");
  
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    Serial.println("\n[OTA] Update starting...");
    esp_task_wdt_delete(NULL);
    if (mqttClient.connected()) mqttClient.disconnect();
    allLightsOff();
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\n[OTA] Error[%u]: ", error);
    otaInProgress = false;
    esp_task_wdt_add(NULL);
  });
  
  ArduinoOTA.begin();
  Serial.println("[OK] OTA enabled");
}

// ==================== MAIN LOOP ====================
void loop() {
  if (otaInProgress) {
    ArduinoOTA.handle();
    return;
  }
  
  ArduinoOTA.handle();
  esp_task_wdt_reset();
  
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
  uint8_t* front_brightness = nullptr;
  
  if (drawer == FIRST_DRAWER) {
    led_array = drawer1_leds;
    led_state = &state.drawer1.leds_active;
    front_brightness = &state.drawer1.frontBrightness;
  } else if (drawer == SECOND_DRAWER) {
    led_array = drawer2_leds;
    led_state = &state.drawer2.leds_active;
    front_brightness = &state.drawer2.frontBrightness;
  } else {
    return;
  }
  
  // Clear all LEDs first
  for (int i = 0; i < LEDS_PER_DRAWER; i++) {
    led_array[i] = CRGB::Black;
  }
  
  bool any_active = false;
  
  for (JsonObject pos : positions) {
    uint8_t position = pos["position"];
    String colorStr = pos["color"];
    uint8_t brightness = 255;  // Default max brightness
    
    // Check for brightness parameter (0-100 scale)
    if (pos.containsKey("brightness")) {
      uint8_t b = pos["brightness"];
      brightness = map(b, 0, 100, 0, 255);  // Convert 0-100 to 0-255
      *front_brightness = b;  // Store for status
    }
    
    if (position >= 1 && position <= LEDS_PER_DRAWER) {
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
  
  if (state.debugMode) {
    Serial.printf("[LED] Drawer %d updated\n", drawer);
  }
}

void setAllDrawerLEDs(uint8_t drawer, String colorStr, uint8_t brightness) {
  CRGB* led_array = nullptr;
  bool* led_state = nullptr;
  uint8_t* front_brightness = nullptr;
  
  if (drawer == FIRST_DRAWER) {
    led_array = drawer1_leds;
    led_state = &state.drawer1.leds_active;
    front_brightness = &state.drawer1.frontBrightness;
  } else if (drawer == SECOND_DRAWER) {
    led_array = drawer2_leds;
    led_state = &state.drawer2.leds_active;
    front_brightness = &state.drawer2.frontBrightness;
  } else {
    return;
  }
  
  // Convert brightness from 0-100 to 0-255
  uint8_t scaled_brightness = map(brightness, 0, 100, 0, 255);
  *front_brightness = brightness;
  
  // Parse color
  uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
  uint8_t r = ((color >> 16) & 0xFF) * scaled_brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * scaled_brightness / 255;
  uint8_t b = (color & 0xFF) * scaled_brightness / 255;
  
  // Set all LEDs to same color
  for (int i = 0; i < LEDS_PER_DRAWER; i++) {
    led_array[i] = CRGB(r, g, b);
  }
  
  *led_state = (brightness > 0);
  FastLED.show();
  
  if (state.debugMode) {
    Serial.printf("[LED] All drawer %d: %s @ %d%%\n", drawer, colorStr.c_str(), brightness);
  }
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
  
  if (state.debugMode) {
    Serial.printf("[COB] Drawer %d: %dK @ %d%%\n", drawer, temperature, brightness);
  }
}

void setZoneLight(uint8_t zone, uint16_t temperature, uint8_t brightness) {
  if (zone != ZONE_NUMBER) return;
  
  setCOBLighting(ZONE_WARM_PIN, ZONE_COOL_PIN, temperature, brightness);
  state.zone.temperature = temperature;
  state.zone.brightness = brightness;
  
  if (state.debugMode) {
    Serial.printf("[ZONE] Zone %d: %dK @ %d%%\n", zone, temperature, brightness);
  }
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
  else if (action == "set_all_leds") {
    // Control all 17 LEDs at once
    uint8_t drawer = data["drawer"];
    String color = data["color"];
    uint8_t brightness = data.containsKey("brightness") ? data["brightness"] : 100;
    setAllDrawerLEDs(drawer, color, brightness);
    Serial.println("[CMD] All LEDs drawer " + String(drawer) + ": " + color + " @ " + String(brightness) + "%");
  }
  else if (action == "set_general_light") {
    uint8_t drawer = data["drawer"];
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setDrawerGeneral(drawer, temperature, brightness);
    Serial.println("[CMD] General drawer " + String(drawer) + ": " + String(temperature) + "K @ " + String(brightness) + "%");
  }
  else if (action == "set_zone_light") {
    uint8_t zone = data["zone"];
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setZoneLight(zone, temperature, brightness);
    Serial.println("[CMD] Zone " + String(zone) + ": " + String(temperature) + "K @ " + String(brightness) + "%");
  }
  else if (action == "all_lights_off") {
    allLightsOff();
  }
  else if (action == "debug_mode") {
    state.debugMode = data["enabled"];
    Serial.printf("[CMD] Debug mode: %s\n", state.debugMode ? "ON" : "OFF");
  }
  else if (action == "read_sensors") {
    sendHeartbeat();
  }
  else if (action == "reboot") {
    Serial.println("[CMD] Rebooting...");
    delay(1000);
    ESP.restart();
  }
}

void sendHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<512> doc;
  doc["action"] = "heartbeat";
  doc["source"] = LIGHTING_ID;
  
  JsonObject data = doc.createNestedObject("data");
  
  // Drawer 1 (physical drawer 2)
  JsonObject d1 = data.createNestedObject("d1");
  d1["id"] = FIRST_DRAWER;
  String light1 = String(state.drawer1.temperature) + "K " + String(state.drawer1.brightness) + "%";
  d1["light"] = light1;
  d1["leds"] = state.drawer1.leds_active ? 1 : 0;
  d1["front_brightness"] = state.drawer1.frontBrightness;
  
  // Drawer 2 (physical drawer 1)
  JsonObject d2 = data.createNestedObject("d2");
  d2["id"] = SECOND_DRAWER;
  String light2 = String(state.drawer2.temperature) + "K " + String(state.drawer2.brightness) + "%";
  d2["light"] = light2;
  d2["leds"] = state.drawer2.leds_active ? 1 : 0;
  d2["front_brightness"] = state.drawer2.frontBrightness;
  
  // Zone
  JsonObject zone = data.createNestedObject("zone");
  zone["id"] = ZONE_NUMBER;
  String lightZone = String(state.zone.temperature) + "K " + String(state.zone.brightness) + "%";
  zone["light"] = lightZone;
  
  data["wifi"] = WiFi.RSSI();
  data["firmware"] = FIRMWARE_VERSION;
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(mqtt_topic_status, output.c_str());
  
  if (state.debugMode) {
    Serial.println("[HB] Sent");
  }
}

void sendStartupMessage() {
  StaticJsonDocument<256> doc;
  doc["action"] = "startup";
  doc["source"] = LIGHTING_ID;
  
  JsonObject data = doc.createNestedObject("data");
  data["firmware"] = FIRMWARE_VERSION;
  data["drawers"][0] = FIRST_DRAWER;
  data["drawers"][1] = SECOND_DRAWER;
  data["zone"] = ZONE_NUMBER;
  data["ip"] = WiFi.localIP().toString();
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(mqtt_topic_status, output.c_str());
  Serial.println("[MQTT] Startup sent");
}