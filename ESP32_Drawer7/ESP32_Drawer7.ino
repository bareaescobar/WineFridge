/*
 * WineFridge Drawer ESP32_DRAWER7 v6.3.0
 *
 * BASED ON WORKING v33 PATTERN
 * - NO OTA (conflicts with GPIO 2)
 * - COB lights fully working
 * - Uses pinMode + ledcDetach + ledcAttach pattern
 * - Detects existing bottles on startup
 * - Tares scale before each LOAD operation
 * - Smart weight measurement: TARE for LOAD, differential for manual
 * - Fixed: COB initialization simulates MQTT brightness=0 command after startup
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

#define DRAWER_ID "drawer_7"
#define FIRMWARE_VERSION "6.3.0"

// WiFi
#define WIFI_SSID_1 "Solo Spirits"
#define WIFI_PASSWORD_1 "Haveadrink"
#define WIFI_SSID_2 "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD_2 "sA77n4kXrss9k9fn377i"

#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

#define HEARTBEAT_INTERVAL 60000
#define DEBOUNCE_TIME 50
#define WEIGHT_STABILIZE_TIME 2000

// Hardware
const uint8_t SWITCH_PINS[9] = {5, 32, 26, 14, 4, 23, 33, 27, 12};
const uint8_t bottleToLed[9] = {2, 6, 10, 14, 0, 4, 8, 12, 16};

#define WS2812_DATA_PIN 13
#define NUM_LEDS 17
#define HX711_DT_PIN 19
#define HX711_SCK_PIN 18
#define COB_WARM_PIN 2
#define COB_COOL_PIN 25

// Weight
#define WEIGHT_CALIBRATION_FACTOR 94.302184
#define WEIGHT_THRESHOLD 100.0
#define NUM_WEIGHT_READINGS 5
#define WEIGHT_OUTLIER_THRESHOLD 200.0

#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
#define PWM_MAX 255
#define TEMP_MIN 2700
#define TEMP_MAX 6500
#define BLINK_INTERVAL 500

// ==================== STATE ====================
enum PositionState {
  STATE_EMPTY,
  STATE_DEBOUNCING,
  STATE_WEIGHING,
  STATE_OCCUPIED,
  STATE_REMOVING
};

struct BottlePosition {
  PositionState state;
  unsigned long stateTimer;
  float weight;
  uint8_t debounceCount;
  float weightBeforePlacement;  // Track total weight before this bottle was placed
};

struct LEDBlinkState {
  bool enabled;
  uint32_t color;
  uint8_t brightness;
  unsigned long lastToggle;
  bool currentState;
};

struct {
  bool positions[9];
  float individualWeights[9];
  uint16_t currentTemperature = 4000;
  uint8_t currentBrightness = 0;
  float ambientTemperature = 0.0;
  float ambientHumidity = 0.0;
  bool weightSensorReady = false;
  bool tempSensorReady = false;
  uint8_t expectedPosition = 0;
  String ipAddress = "0.0.0.0";
} state;

// ==================== GLOBALS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

BottlePosition positions[9];
LEDBlinkState ledBlinkStates[9];

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("WineFridge Drawer - " + String(DRAWER_ID));
  Serial.println("Firmware: v" + String(FIRMWARE_VERSION));
  Serial.println("========================================");
  
  memset(&state, 0, sizeof(state));
  memset(&positions, 0, sizeof(positions));
  memset(&ledBlinkStates, 0, sizeof(ledBlinkStates));
  
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  setupPins();
  setupLEDs();
  setupCOBLEDs();
  setupSensors();

  // Initialize all positions as empty first
  for (int i = 0; i < 9; i++) {
    positions[i].state = STATE_EMPTY;
    positions[i].weight = 0.0;
  }

  setupNetwork();

  // Detect existing bottles after network is ready
  detectExistingBottles();

  Serial.println("\n[READY]\n");
}

void setupPins() {
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
  }
  Serial.println("[OK] GPIO pins");
}

void setupLEDs() {
  strip.begin();
  strip.clear();
  strip.show();
  Serial.println("[OK] WS2812B LEDs");
}

void setupCOBLEDs() {
  Serial.println("[COB] Initializing COB LEDs...");
  
  // CRITICAL PATTERN for GPIO 2:
  // 1. pinMode + digitalWrite LOW
  pinMode(COB_WARM_PIN, OUTPUT);
  pinMode(COB_COOL_PIN, OUTPUT);
  digitalWrite(COB_WARM_PIN, LOW);
  digitalWrite(COB_COOL_PIN, LOW);
  
  // 2. ledcDetach (clear any previous config)
  ledcDetach(COB_WARM_PIN);
  ledcDetach(COB_COOL_PIN);
  delay(100);
  
  // 3. ledcAttach (setup PWM)
  ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  
  // 4. Set to off
  ledcWrite(COB_WARM_PIN, 0);
  ledcWrite(COB_COOL_PIN, 0);
  
  Serial.println("[COB] Testing - turning ON for 1 second...");

  // Test: Turn on warm at 50%
  ledcWrite(COB_WARM_PIN, 128);
  delay(500);
  ledcWrite(COB_WARM_PIN, 0);

  // Test: Turn on cool at 50%
  ledcWrite(COB_COOL_PIN, 128);
  delay(500);
  ledcWrite(COB_COOL_PIN, 0);

  Serial.println("[OK] COB LEDs ready (will reset via MQTT after connection)");
}

void setupSensors() {
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  delay(500);
  
  if (scale.is_ready()) {
    scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
    delay(200);
    scale.tare();
    state.weightSensorReady = true;
    Serial.println("[OK] HX711 ready");
  } else {
    state.weightSensorReady = false;
    Serial.println("[WARN] HX711 FAILED");
  }
  
  Wire.begin(21, 22);
  if (sht31.begin(0x44)) {
    state.tempSensorReady = true;
    Serial.println("[OK] SHT31 sensor");
  } else {
    state.tempSensorReady = false;
    Serial.println("[WARN] SHT31 FAILED");
  }
}

void setupNetwork() {
  Serial.println("\n[WIFI] Starting...");
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  
  WiFi.begin(WIFI_SSID_1, WIFI_PASSWORD_1);
  Serial.print("[WIFI] Trying ");
  Serial.print(WIFI_SSID_1);
  Serial.print("...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" failed");
    WiFi.disconnect(true);
    delay(1000);
    
    Serial.print("[WIFI] Trying ");
    Serial.print(WIFI_SSID_2);
    Serial.print("...");
    
    WiFi.begin(WIFI_SSID_2, WIFI_PASSWORD_2);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    state.ipAddress = WiFi.localIP().toString();
    Serial.println(" ✓");
    Serial.println("[WIFI] IP: " + state.ipAddress);
    
    mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
    mqttClient.setCallback(onMQTTMessage);
    mqttClient.setBufferSize(2048);
    
    connectMQTT();
  } else {
    Serial.println(" ✗");
    state.ipAddress = "0.0.0.0";
  }
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
  Serial.print("[MQTT] Connecting...");
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println(" ✓");
    mqttClient.subscribe(mqtt_topic_command);
    Serial.println("[MQTT] Subscribed: " + String(mqtt_topic_command));
    sendStartupMessage();
  } else {
    Serial.print(" ✗ (");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  yield();
  
  updateBlinkingLEDs();
  
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > 30000) {
    maintainConnections();
    lastConnectionCheck = millis();
  }
  
  if (mqttClient.connected()) {
    mqttClient.loop();
  }
  
  for (int i = 0; i < 9; i++) {
    updatePositionStateMachine(i);
  }
  
  static unsigned long lastSensorUpdate = 0;
  if (millis() - lastSensorUpdate > 15000) {
    updateSensors();
    lastSensorUpdate = millis();
  }
  
  static unsigned long lastHeartbeat = 0;
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    lastHeartbeat = millis();
  }
  
  delay(10);
}

// ==================== STATE MACHINE ====================
void updatePositionStateMachine(uint8_t position) {
  bool currentSwitch = (digitalRead(SWITCH_PINS[position]) == LOW);
  BottlePosition* pos = &positions[position];
  
  switch (pos->state) {
    case STATE_EMPTY:
      if (currentSwitch) {
        pos->state = STATE_DEBOUNCING;
        pos->stateTimer = millis();
        pos->debounceCount = 1;
      }
      break;
      
    case STATE_DEBOUNCING:
      if (!currentSwitch) {
        pos->state = STATE_EMPTY;
        pos->debounceCount = 0;
      } else if (millis() - pos->stateTimer > DEBOUNCE_TIME) {
        pos->debounceCount++;
        pos->stateTimer = millis();
        
        if (pos->debounceCount >= 3) {
          if (state.expectedPosition != 0 && (position + 1) != state.expectedPosition) {
            Serial.printf("[WARN] Wrong position! Expected %d, got %d\n", 
                         state.expectedPosition, position + 1);
            sendWrongPlacement(position + 1);
            setLED(position + 1, 0xFF0000, 100, true);
            pos->state = STATE_EMPTY;
            return;
          }
          
          pos->state = STATE_WEIGHING;
          pos->stateTimer = millis();

          // Save current total weight before placing bottle (for differential calculation)
          if (scale.is_ready()) {
            pos->weightBeforePlacement = scale.get_units(3);
            Serial.printf("[POS %d] Weight before: %.1fg\n", position + 1, pos->weightBeforePlacement);
          } else {
            pos->weightBeforePlacement = 0.0;
          }

          setLED(position + 1, 0x00FF00, 50, true);
          Serial.printf("[POS %d] Weighing...\n", position + 1);
        }
      }
      break;
      
    case STATE_WEIGHING:
      if (!currentSwitch) {
        pos->state = STATE_EMPTY;
        setLED(position + 1, 0x000000, 0, false);
      } else if (millis() - pos->stateTimer > WEIGHT_STABILIZE_TIME) {
        float totalWeightNow = getStableWeight();
        float individualWeight;

        // If this position is expected (LOAD process), scale was TARED - read total weight directly
        if (state.expectedPosition == (position + 1)) {
          individualWeight = totalWeightNow;
          Serial.printf("[POS %d] LOAD process - Total weight: %.1fg (scale was tared)\n",
                       position + 1, totalWeightNow);
        } else {
          // Manual placement - calculate differential weight
          individualWeight = totalWeightNow - pos->weightBeforePlacement;
          Serial.printf("[POS %d] Manual placement - Total: %.1fg, Before: %.1fg, Individual: %.1fg\n",
                       position + 1, totalWeightNow, pos->weightBeforePlacement, individualWeight);
        }

        if (individualWeight > WEIGHT_THRESHOLD) {
          pos->weight = individualWeight;
          state.individualWeights[position] = individualWeight;
          state.positions[position] = true;
          pos->state = STATE_OCCUPIED;

          setLED(position + 1, 0x808080, 30, false);
          sendBottleEvent(position + 1, true, individualWeight);

          Serial.printf("[POS %d] ✓ Placed: %.1fg\n", position + 1, individualWeight);

          if (state.expectedPosition == (position + 1)) {
            state.expectedPosition = 0;
          }
        } else {
          Serial.printf("[POS %d] ✗ Weight too low: %.1fg\n", position + 1, individualWeight);
          pos->state = STATE_EMPTY;
          setLED(position + 1, 0xFF0000, 100, true);
          delay(2000);
          setLED(position + 1, 0x000000, 0, false);
        }
      }
      break;
      
    case STATE_OCCUPIED:
      if (!currentSwitch) {
        pos->state = STATE_REMOVING;
        pos->stateTimer = millis();
      }
      break;
      
    case STATE_REMOVING:
      if (currentSwitch) {
        pos->state = STATE_OCCUPIED;
      } else if (millis() - pos->stateTimer > DEBOUNCE_TIME * 3) {
        float oldWeight = pos->weight;
        pos->weight = 0.0;
        state.individualWeights[position] = 0.0;
        state.positions[position] = false;
        pos->state = STATE_EMPTY;
        
        setLED(position + 1, 0x000000, 0, false);
        sendBottleEvent(position + 1, false, oldWeight);
        
        Serial.printf("[POS %d] ✓ Removed: %.1fg\n", position + 1, oldWeight);
      }
      break;
  }
}

float getStableWeight() {
  if (!state.weightSensorReady || !scale.is_ready()) {
    return 0.0;
  }
  
  float readings[NUM_WEIGHT_READINGS];
  unsigned long interval = WEIGHT_STABILIZE_TIME / NUM_WEIGHT_READINGS;
  
  for (int i = 0; i < NUM_WEIGHT_READINGS; i++) {
    if (i > 0) delay(interval);
    readings[i] = scale.get_units(1);
  }
  
  for (int i = 0; i < NUM_WEIGHT_READINGS - 1; i++) {
    for (int j = i + 1; j < NUM_WEIGHT_READINGS; j++) {
      if (readings[i] > readings[j]) {
        float temp = readings[i];
        readings[i] = readings[j];
        readings[j] = temp;
      }
    }
  }
  
  float median = readings[NUM_WEIGHT_READINGS / 2];
  
  float sum = 0.0;
  int count = 0;
  
  for (int i = 0; i < NUM_WEIGHT_READINGS; i++) {
    if (abs(readings[i] - median) < WEIGHT_OUTLIER_THRESHOLD) {
      sum += readings[i];
      count++;
    }
  }
  
  if (count == 0) return 0.0;
  
  float average = sum / count;
  return max(0.0f, average);
}

void updateSensors() {
  if (state.tempSensorReady) {
    float t = sht31.readTemperature();
    float h = sht31.readHumidity();
    
    if (!isnan(t) && !isnan(h)) {
      state.ambientTemperature = round(t * 10.0) / 10.0;
      state.ambientHumidity = round(h * 10.0) / 10.0;
    }
  }
}

// ==================== LED FUNCTIONS ====================
void setLED(uint8_t position, uint32_t color, uint8_t brightness, bool blink) {
  if (position < 1 || position > 9) return;
  
  uint8_t ledIndex = bottleToLed[position - 1];
  if (ledIndex >= NUM_LEDS) return;
  
  ledBlinkStates[position - 1].enabled = blink;
  ledBlinkStates[position - 1].color = color;
  ledBlinkStates[position - 1].brightness = brightness;
  ledBlinkStates[position - 1].lastToggle = millis();
  ledBlinkStates[position - 1].currentState = true;
  
  if (!blink) {
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 100;
    uint8_t g = ((color >> 8) & 0xFF) * brightness / 100;
    uint8_t b = (color & 0xFF) * brightness / 100;
    strip.setPixelColor(ledIndex, strip.Color(r, g, b));
    strip.show();
  }
}

void updateBlinkingLEDs() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 50) return;
  lastUpdate = millis();
  
  bool needsUpdate = false;
  
  for (int i = 0; i < 9; i++) {
    if (ledBlinkStates[i].enabled) {
      if (millis() - ledBlinkStates[i].lastToggle >= BLINK_INTERVAL) {
        ledBlinkStates[i].currentState = !ledBlinkStates[i].currentState;
        ledBlinkStates[i].lastToggle = millis();
        
        uint8_t ledIndex = bottleToLed[i];
        if (ledBlinkStates[i].currentState) {
          uint32_t color = ledBlinkStates[i].color;
          uint8_t brightness = ledBlinkStates[i].brightness;
          uint8_t r = ((color >> 16) & 0xFF) * brightness / 100;
          uint8_t g = ((color >> 8) & 0xFF) * brightness / 100;
          uint8_t b = (color & 0xFF) * brightness / 100;
          strip.setPixelColor(ledIndex, strip.Color(r, g, b));
        } else {
          strip.setPixelColor(ledIndex, 0);
        }
        needsUpdate = true;
      }
    }
  }
  
  if (needsUpdate) {
    strip.show();
  }
}

void setCOBLighting(uint16_t temperature, uint8_t brightness) {
  temperature = constrain(temperature, TEMP_MIN, TEMP_MAX);
  brightness = constrain(brightness, 0, 100);

  Serial.printf("[COB] Setting T=%dK B=%d%%\n", temperature, brightness);

  if (brightness == 0) {
    // Turn off: detach, reset, reattach
    ledcDetach(COB_WARM_PIN);
    ledcDetach(COB_COOL_PIN);
    delay(50);
    pinMode(COB_WARM_PIN, OUTPUT);
    pinMode(COB_COOL_PIN, OUTPUT);
    digitalWrite(COB_WARM_PIN, LOW);
    digitalWrite(COB_COOL_PIN, LOW);
    delay(50);
    ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
    ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
    ledcWrite(COB_WARM_PIN, 0);
    ledcWrite(COB_COOL_PIN, 0);
    Serial.println("[COB] OFF");
  } else {
    float warmRatio = map(temperature, TEMP_MIN, TEMP_MAX, 100, 0) / 100.0;
    float coolRatio = 1.0 - warmRatio;

    uint8_t maxPWM = map(brightness, 0, 100, 0, PWM_MAX);
    uint8_t warmPWM = maxPWM * warmRatio;
    uint8_t coolPWM = maxPWM * coolRatio;

    ledcWrite(COB_WARM_PIN, warmPWM);
    ledcWrite(COB_COOL_PIN, coolPWM);
    
    Serial.printf("[COB] Warm=%d Cool=%d (%.2f/%.2f)\n", warmPWM, coolPWM, warmRatio, coolRatio);
  }

  state.currentTemperature = temperature;
  state.currentBrightness = brightness;
}

// ==================== MQTT ====================
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  if (length > 1500) return;
  
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  
  Serial.printf("[MQTT] RX: %s\n", message);
  
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, message) == DeserializationError::Ok) {
    handleCommand(doc);
  } else {
    Serial.println("[MQTT] JSON parse error");
  }
}

void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  Serial.println("[MQTT] Action: " + action);
  
  if (action == "set_leds") {
    JsonArray positionsArray = data["positions"];

    // If empty array, turn off ALL LEDs and disable all blink states
    if (positionsArray.size() == 0) {
      Serial.println("[LED] Clearing ALL LEDs");
      for (int i = 0; i < 9; i++) {
        ledBlinkStates[i].enabled = false;
        uint8_t ledIndex = bottleToLed[i];
        strip.setPixelColor(ledIndex, 0);
      }
      strip.show();
    } else {
      // Clear all LEDs first before setting new ones
      for (int i = 0; i < 9; i++) {
        ledBlinkStates[i].enabled = false;
        uint8_t ledIndex = bottleToLed[i];
        strip.setPixelColor(ledIndex, 0);
      }

      // Set the specified LEDs
      for (JsonObject pos : positionsArray) {
        uint8_t position = pos["position"];
        String colorStr = pos["color"];
        uint8_t brightness = pos["brightness"];
        bool blink = pos.containsKey("blink") ? pos["blink"].as<bool>() : false;

        if (position >= 1 && position <= 9) {
          uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
          setLED(position, color, brightness, blink);
          Serial.printf("[LED] Pos %d: %s @%d%% Blink:%d\n",
                       position, colorStr.c_str(), brightness, blink);
        }
      }
    }
  }
  else if (action == "set_general_light") {
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setCOBLighting(temperature, brightness);
  }
  else if (action == "expect_bottle") {
    uint8_t position = data["position"];
    state.expectedPosition = position;
    Serial.printf("[CMD] Expecting position %d\n", position);

    // Tare the scale so we only measure the new bottle's weight
    if (scale.is_ready()) {
      scale.tare();
      Serial.println("[CMD] ✓ Scale tared - ready to measure new bottle");
    }
  }
  else if (action == "manual_tare") {
    if (scale.is_ready()) {
      scale.tare();
      Serial.println("[CMD] ✓ Tare complete");
    }
  }
  else if (action == "reboot") {
    Serial.println("[CMD] Rebooting in 2s...");
    delay(2000);
    ESP.restart();
  }
}

void sendHeartbeat() {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<640> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["ip"] = state.ipAddress;
  
  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = millis() / 1000;
  
  float total = 0.0;
  for (int i = 0; i < 9; i++) total += state.individualWeights[i];
  data["total_weight"] = round(total * 10.0) / 10.0;
  
  data["temperature"] = state.ambientTemperature;
  data["humidity"] = state.ambientHumidity;
  
  JsonArray occupied = data.createNestedArray("occupied");
  for (int i = 0; i < 9; i++) occupied.add(state.positions[i] ? 1 : 0);
  
  JsonArray weights = data.createNestedArray("weights");
  for (int i = 0; i < 9; i++) weights.add(round(state.individualWeights[i] * 10.0) / 10.0);
  
  char light[32];
  snprintf(light, sizeof(light), "%dK %d%%", state.currentTemperature, state.currentBrightness);
  data["cob_light"] = light;
  data["wifi_rssi"] = WiFi.RSSI();
  
  char buffer[640];
  serializeJson(doc, buffer);
  mqttClient.publish(mqtt_topic_status, buffer);
}

void sendBottleEvent(uint8_t position, bool placed, float weight) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<320> doc;
  doc["action"] = "bottle_event";
  doc["source"] = DRAWER_ID;
  
  JsonObject data = doc.createNestedObject("data");
  data["event"] = placed ? "placed" : "removed";
  data["position"] = position;
  data["weight"] = round(weight * 10.0) / 10.0;
  
  char buffer[320];
  serializeJson(doc, buffer);
  mqttClient.publish(mqtt_topic_status, buffer);
}

void sendWrongPlacement(uint8_t position) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["action"] = "wrong_placement";
  doc["source"] = DRAWER_ID;
  
  JsonObject data = doc.createNestedObject("data");
  data["position"] = position;
  data["expected_position"] = state.expectedPosition;
  
  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(mqtt_topic_status, buffer);
}

void sendStartupMessage() {
  StaticJsonDocument<256> doc;
  doc["action"] = "startup";
  doc["source"] = DRAWER_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["ip"] = state.ipAddress;

  char buffer[256];
  serializeJson(doc, buffer);
  mqttClient.publish(mqtt_topic_status, buffer);
  Serial.println("[MQTT] Startup sent");

  // Simulate receiving brightness=0 message to reset COB lights to clean state
  // This is necessary after the test sequence in setupCOBLEDs()
  Serial.println("[INIT] Simulating brightness=0 command to reset COB lights...");
  StaticJsonDocument<256> cobResetDoc;
  cobResetDoc["action"] = "set_general_light";
  JsonObject data = cobResetDoc.createNestedObject("data");
  data["temperature"] = 4000;
  data["brightness"] = 0;
  handleCommand(cobResetDoc);
}

void detectExistingBottles() {
  Serial.println("[INIT] Detecting existing bottles...");

  if (!state.weightSensorReady) {
    Serial.println("[INIT] ✗ Weight sensor not ready, skipping detection");
    return;
  }

  delay(1000); // Wait for weight sensor to stabilize after tare

  // Count pressed switches (bottles present)
  int bottlesPresent = 0;
  bool switchStates[9];

  for (int i = 0; i < 9; i++) {
    switchStates[i] = (digitalRead(SWITCH_PINS[i]) == LOW);
    if (switchStates[i]) {
      bottlesPresent++;
    }
  }

  if (bottlesPresent == 0) {
    Serial.println("[INIT] No bottles detected");
    return;
  }

  // Read total weight
  float totalWeight = getStableWeight();

  if (totalWeight < (WEIGHT_THRESHOLD * bottlesPresent * 0.5)) {
    // Weight too low, probably false detection
    Serial.printf("[INIT] ✗ Weight too low: %.1fg for %d bottles\n", totalWeight, bottlesPresent);
    return;
  }

  // Estimate individual weight (simple average)
  float estimatedWeight = totalWeight / bottlesPresent;

  Serial.printf("[INIT] Found %d bottles, total weight: %.1fg\n", bottlesPresent, totalWeight);

  // Mark positions as occupied
  for (int i = 0; i < 9; i++) {
    if (switchStates[i]) {
      positions[i].state = STATE_OCCUPIED;
      positions[i].weight = estimatedWeight;
      state.positions[i] = true;
      state.individualWeights[i] = estimatedWeight;

      Serial.printf("[INIT] ✓ Position %d occupied (est. %.1fg)\n", i + 1, estimatedWeight);
    }
  }

  Serial.println("[INIT] ✓ Detection complete\n");
}

void maintainConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Reconnecting...");
    WiFi.reconnect();
    return;
  }

  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Reconnecting...");
    connectMQTT();
  }
}
