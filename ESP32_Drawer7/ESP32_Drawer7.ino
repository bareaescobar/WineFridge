/*
 * WineFridge Drawer ESP32_DRAWER7 v5.5.0
 * 
 * CHANGELOG v5.5.0:
 * NEW VARIABLE 
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include "Adafruit_SHT31.h"
#include "esp_task_wdt.h"

#define DRAWER_ID "drawer_7"
#define FIRMWARE_VERSION "5.5.0"

// WiFi
#define WIFI_SSID_1 "Solo Spirits"
#define WIFI_PASSWORD_1 "Haveadrink"
#define WIFI_SSID_2 "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD_2 "sA77n4kXrss9k9fn377i"

#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

#define HEARTBEAT_INTERVAL 60000
#define DEBOUNCE_TIME 50
#define WEIGHT_STABILIZE_TIME 2000  // 2 seconds total for weighing
#define WATCHDOG_TIMEOUT 75000 // 75 seconds
#define NETWORK_TIMECHECK 30000 // 30 seconds

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
#define WEIGHT_FALLBACK 1000.0  // Use 1000g if sensor fails
#define NUM_WEIGHT_READINGS 5   // Take 5 readings
#define WEIGHT_OUTLIER_THRESHOLD 200.0  // Discard readings >200g from median

#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
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
  String connectedSSID = "";
} state;

// ==================== GLOBALS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

BottlePosition positions[9];
LEDBlinkState ledBlinkStates[9];
volatile bool otaInProgress = false;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== SETUP ====================
void setup() {
  pinMode(COB_WARM_PIN, OUTPUT);
  digitalWrite(COB_WARM_PIN, LOW);
  pinMode(COB_COOL_PIN, OUTPUT);
  digitalWrite(COB_COOL_PIN, LOW);
  delay(100);
  
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("WineFridge Drawer - " + String(DRAWER_ID));
  Serial.println("Firmware: v" + String(FIRMWARE_VERSION));
  Serial.println("========================================");

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  memset(&state, 0, sizeof(state));
  memset(&positions, 0, sizeof(positions));
  memset(&ledBlinkStates, 0, sizeof(ledBlinkStates));
  
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  setupPins();
  setupLEDs();
  setupCOBLEDs();
  setupSensors();
  setupNetwork();
  setupOTA();
  
  for (int i = 0; i < 9; i++) {
    positions[i].state = STATE_EMPTY;
    positions[i].weight = 0.0;
  }
  
  Serial.println("\n[READY]\n");
  esp_task_wdt_reset();
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
  ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcWrite(COB_WARM_PIN, 0);
  ledcWrite(COB_COOL_PIN, 0);
  Serial.println("[OK] COB LEDs");
}

void setupSensors() {
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  delay(500);
  
  if (scale.is_ready()) {
    scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
    delay(200);
    scale.tare();
    state.weightSensorReady = true;
    Serial.println("[OK] HX711 ready & tared");
  } else {
    state.weightSensorReady = false;
    Serial.println("[WARN] HX711 FAILED - will use fallback 1000g");
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
    esp_task_wdt_reset();
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(" failed");
    WiFi.disconnect(true);
    delay(1000);
    WiFi.mode(WIFI_OFF);
    delay(500);
    WiFi.mode(WIFI_STA);
    delay(500);
    
    Serial.print("[WIFI] Trying ");
    Serial.print(WIFI_SSID_2);
    Serial.print("...");
    
    WiFi.begin(WIFI_SSID_2, WIFI_PASSWORD_2);
    attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
      esp_task_wdt_reset();
    }
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    state.ipAddress = WiFi.localIP().toString();
    state.connectedSSID = WiFi.SSID();
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

void setupOTA() {
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setPassword("winefridge2025");
  
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    Serial.println("\n[OTA] Starting...");
    esp_task_wdt_deinit();
    if (mqttClient.connected()) mqttClient.disconnect();
    strip.clear();
    strip.show();
    ledcWrite(COB_WARM_PIN, 0);
    ledcWrite(COB_COOL_PIN, 0);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastReport = 0;
    if (millis() - lastReport > 500) {
      Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
      lastReport = millis();
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\n[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    delay(1000);
    ESP.restart();
  });
  
  ArduinoOTA.begin();
  Serial.println("[OK] OTA ready");
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
  Serial.print("[MQTT] Connecting...");
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println(" ✓");
    mqttClient.subscribe(mqtt_topic_command);
    sendStartupMessage();
  } else {
    Serial.print(" ✗ (");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
}

// ==================== LOOP ====================
void loop() {
  ArduinoOTA.handle();
  esp_task_wdt_reset();
  
  if (otaInProgress) return;
  
  updateBlinkingLEDs();
  
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > NETWORK_TIMECHECK) {
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
void updatePositionStateMachine(uint8_t posIndex) {
  BottlePosition* pos = &positions[posIndex];
  bool switchPressed = (digitalRead(SWITCH_PINS[posIndex]) == LOW);
  
  switch (pos->state) {
    
    case STATE_EMPTY:
      if (switchPressed) {
        pos->state = STATE_DEBOUNCING;
        pos->stateTimer = millis();
        pos->debounceCount = 1;
      }
      break;
    
    case STATE_DEBOUNCING:
      if (millis() - pos->stateTimer > 10) {
        if (switchPressed) {
          pos->debounceCount++;
        } else {
          pos->state = STATE_EMPTY;
          pos->debounceCount = 0;
        }
        
        pos->stateTimer = millis();
        
        if (pos->debounceCount >= 5) {
          // Check wrong position
          if (state.expectedPosition != 0 && (posIndex + 1) != state.expectedPosition) {
            Serial.printf("[ERROR] Wrong position! Expected %d, got %d\n", state.expectedPosition, posIndex + 1);
            
            pos->weight = WEIGHT_FALLBACK;
            state.individualWeights[posIndex] = pos->weight;
            pos->state = STATE_OCCUPIED;
            
            setLED(posIndex + 1, 0xFF0000, 100, false);  // RED solid
            
            sendWrongPlacement(posIndex + 1);
          } else {
            // Correct position - weigh
            pos->state = STATE_WEIGHING;
            pos->stateTimer = millis();
            setLED(posIndex + 1, 0x00FF00, 100, true);  // GREEN blinking
          }
        }
      }
      break;
    
    case STATE_WEIGHING:
      if (millis() - pos->stateTimer > WEIGHT_STABILIZE_TIME) {
        esp_task_wdt_reset();
        
        float weight = readWeightRobust();
        
        if (weight >= WEIGHT_THRESHOLD) {
          pos->weight = weight;
          Serial.printf("[WEIGHT] ✓ Pos %d: %.1fg\n", posIndex + 1, weight);
        } else {
          pos->weight = WEIGHT_FALLBACK;
          Serial.printf("[WEIGHT] ⚠ Pos %d: Low (%.1fg), using %dg\n", posIndex + 1, weight, (int)WEIGHT_FALLBACK);
        }
        
        state.individualWeights[posIndex] = pos->weight;
        state.positions[posIndex] = true;
        pos->state = STATE_OCCUPIED;
        
        if (state.expectedPosition == (posIndex + 1)) {
          state.expectedPosition = 0;
        }
        
        setLED(posIndex + 1, 0x444444, 50, false);  // GRAY solid
        
        Serial.printf("[EVENT] ✓ PLACED at %d (%.1fg)\n", posIndex + 1, pos->weight);
        sendBottleEvent(posIndex + 1, true, pos->weight);
        
        // TARE immediately
        yield();
        if (state.weightSensorReady && scale.is_ready()) {
          scale.tare();
          Serial.println("[WEIGHT] ✓ Tared");
        }
        esp_task_wdt_reset();
      }
      break;
    
    case STATE_OCCUPIED:
      if (!switchPressed) {
        pos->state = STATE_REMOVING;
        pos->stateTimer = millis();
        pos->debounceCount = 1;
      }
      break;
    
    case STATE_REMOVING:
      if (millis() - pos->stateTimer > 10) {
        if (!switchPressed) {
          pos->debounceCount++;
        } else {
          pos->state = STATE_OCCUPIED;
          pos->debounceCount = 0;
        }
        
        pos->stateTimer = millis();
        
        if (pos->debounceCount >= 5) {
          esp_task_wdt_reset();
          
          float removedWeight = pos->weight;
          pos->weight = 0.0;
          state.individualWeights[posIndex] = 0.0;
          state.positions[posIndex] = false;
          pos->state = STATE_EMPTY;
          
          setLED(posIndex + 1, 0x000000, 0, false);  // OFF
          
          Serial.printf("[EVENT] ✓ REMOVED from %d (was %.1fg)\n", posIndex + 1, removedWeight);
          sendBottleEvent(posIndex + 1, false, removedWeight);
          
          // TARE immediately
          yield();
          if (state.weightSensorReady && scale.is_ready()) {
            scale.tare();
            Serial.println("[WEIGHT] ✓ Tared");
          }
          esp_task_wdt_reset();
        }
      }
      break;
  }
}

// ==================== WEIGHT FUNCTIONS ====================
float readWeightRobust() {
  if (!state.weightSensorReady || !scale.is_ready()) {
    return 0.0;
  }
  
  // Take 5 readings over 2 seconds
  float readings[NUM_WEIGHT_READINGS];
  unsigned long interval = WEIGHT_STABILIZE_TIME / NUM_WEIGHT_READINGS;
  
  for (int i = 0; i < NUM_WEIGHT_READINGS; i++) {
    if (i > 0) delay(interval);
    readings[i] = scale.get_units(1);
    esp_task_wdt_reset();
  }
  
  // Sort for median
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
  
  // Average readings within threshold of median
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
  
  if (brightness == 0) {
    ledcWrite(COB_WARM_PIN, 0);
    ledcWrite(COB_COOL_PIN, 0);
  } else {
    float warmRatio = map(temperature, TEMP_MIN, TEMP_MAX, 100, 0) / 100.0;
    float coolRatio = 1.0 - warmRatio;
    uint8_t maxPWM = map(brightness, 0, 100, 0, 255);
    uint8_t warmPWM = maxPWM * warmRatio;
    uint8_t coolPWM = maxPWM * coolRatio;
    
    ledcWrite(COB_WARM_PIN, warmPWM);
    ledcWrite(COB_COOL_PIN, coolPWM);
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
  
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, message) == DeserializationError::Ok) {
    handleCommand(doc);
  }
}

void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  
  if (action == "set_leds") {
    JsonArray positionsArray = data["positions"];
    for (JsonObject pos : positionsArray) {
      uint8_t position = pos["position"];
      String colorStr = pos["color"];
      uint8_t brightness = pos["brightness"];
      bool blink = pos.containsKey("blink") ? pos["blink"].as<bool>() : false;
      
      if (position >= 1 && position <= 9) {
        uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
        setLED(position, color, brightness, blink);
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
  
  // Total weight
  float total = 0.0;
  for (int i = 0; i < 9; i++) total += state.individualWeights[i];
  data["total_weight"] = round(total * 10.0) / 10.0;
  
  data["temperature"] = state.ambientTemperature;
  data["humidity"] = state.ambientHumidity;
  
  // Compact arrays
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
