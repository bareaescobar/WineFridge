/*
 * WineFridge Drawer ESP32 - SIMPLIFIED & ROBUST
 * ESP32 v33 - 15.10.2025 11.00h
 * - Heartbeat every 90s with minimal data
 * - Reduced debug (critical + events only)
 * - New position mapping (rear: 1-4, front: 5-9)
 * - Faster stabilization (1000ms instead of 1500ms)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <HX711.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"

// ==================== CONFIGURATION ====================
#define DRAWER_ID "drawer_5"  // CHANGE TO: "drawer_3" or "drawer_5" or "drawer_7"

// Network
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing
#define HEARTBEAT_INTERVAL 90000      // 90 seconds
#define SENSOR_CHECK_INTERVAL 2000    // 2 seconds
#define CONNECTION_CHECK_INTERVAL 10000

// Hardware - NEW MAPPING (rear: 1-4, front: 5-9)
const uint8_t SWITCH_PINS[9] = {5, 32, 26, 14, 4, 23, 33, 27, 12}; 
const uint8_t bottleToLed[9] = {2, 6, 10, 14, 0, 4, 8, 12, 16};

#define WS2812_DATA_PIN 13
#define NUM_LEDS 17
#define HX711_DT_PIN 19
#define HX711_SCK_PIN 18
#define COB_WARM_PIN 2
#define COB_COOL_PIN 25

// Constants
#define WEIGHT_CALIBRATION_FACTOR -94.302184
#define WEIGHT_THRESHOLD 50.0
#define DEBOUNCE_TIME 150
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
} state;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("[STARTUP] WineFridge Drawer - " + String(DRAWER_ID));

  memset(&state.positions, 0, sizeof(state.positions));
  memset(&state.individualWeights, 0, sizeof(state.individualWeights));
  
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  // Hardware init
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
  }
  
  strip.begin();
  strip.clear();
  strip.show();
  
  // COB LEDs - simplified init
  pinMode(COB_WARM_PIN, OUTPUT);
  pinMode(COB_COOL_PIN, OUTPUT);
  digitalWrite(COB_WARM_PIN, LOW);
  digitalWrite(COB_COOL_PIN, LOW);
  ledcDetach(COB_WARM_PIN);
  ledcDetach(COB_COOL_PIN);
  delay(100);
  ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcWrite(COB_WARM_PIN, 0);
  ledcWrite(COB_COOL_PIN, 0);
  Serial.println("[OK] COB LEDs");
  
  // Weight sensor
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  delay(1000);
  
  if (scale.is_ready()) {
    scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
    delay(500);
    
    bool anyBottlesPresent = false;
    for (int i = 0; i < 9; i++) {
      if (digitalRead(SWITCH_PINS[i]) == LOW) {
        anyBottlesPresent = true;
        break;
      }
    }
    
    if (!anyBottlesPresent) {
      scale.tare();
      state.lastTotalWeight = 0.0;
      Serial.println("[OK] Weight sensor (tared)");
    } else {
      state.lastTotalWeight = scale.get_units(3);
      Serial.println("[OK] Weight sensor (bottles present)");
    }
    
    state.weightSensorReady = true;
  } else {
    Serial.println("[ERROR] Weight sensor FAILED");
  }
  
  // Temperature sensor
  Wire.begin(21, 22);
  if (sht31.begin(0x44)) {
    state.tempSensorReady = true;
    Serial.println("[OK] Temperature sensor");
  } else {
    Serial.println("[ERROR] Temperature sensor FAILED");
  }
  
  setupNetwork();
  readAllPositions();
  updateTemperatureHumidity();
  
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
  
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
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
  
  static unsigned long lastSensorCheck = 0;
  if (millis() - lastSensorCheck > SENSOR_CHECK_INTERVAL) {
    checkPositionChanges();
    lastSensorCheck = millis();
  }
  
  static unsigned long lastTempRead = 0;
  if (millis() - lastTempRead > 15000) {
    updateTemperatureHumidity();
    lastTempRead = millis();
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

// ==================== SENSORS ====================
void readAllPositions() {
  state.lastTotalWeight = readTotalWeight();
  for (int i = 0; i < 9; i++) {
    state.positions[i] = (digitalRead(SWITCH_PINS[i]) == LOW);
  }
}

float readTotalWeight() {
  if (!state.weightSensorReady || !scale.is_ready()) {
    return state.lastTotalWeight;
  }
  
  float totalWeight = 0;
  int validReadings = 0;
  
  for (int i = 0; i < 3; i++) {
    yield();
    
    if (scale.is_ready()) {
      float reading = scale.get_units(1);
      if (reading >= -100 && reading <= 5000 && !isnan(reading) && !isinf(reading)) {
        totalWeight += reading;
        validReadings++;
      }
    }
    delay(50);
  }
  
  if (validReadings > 0) {
    return max(0.0f, totalWeight / validReadings);
  }
  
  return state.lastTotalWeight;
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

bool readSwitchStable(uint8_t pin, int samples = 5) {
  int lowCount = 0;
  for (int i = 0; i < samples; i++) {
    if (digitalRead(pin) == LOW) lowCount++;
    delay(10);
  }
  return (lowCount > samples/2);
}

void checkPositionChanges() {
  static bool lastPositions[9] = {false};
  static unsigned long lastDebounce[9] = {0};
  static bool processingChange = false;
  
  if (processingChange) return;
  
  for (int i = 0; i < 9; i++) {
    bool currentState = readSwitchStable(SWITCH_PINS[i]);
    
    if (millis() - lastDebounce[i] < DEBOUNCE_TIME) continue;
    
    if (currentState != lastPositions[i]) {
      lastDebounce[i] = millis();
      processingChange = true;
      
      delay(DEBOUNCE_TIME);
      currentState = readSwitchStable(SWITCH_PINS[i]);
      
      if (currentState != lastPositions[i]) {
        lastPositions[i] = currentState;
        state.positions[i] = currentState;
        
        float weightChange = 0.0;
        
        if (currentState) {
          // Bottle placed
          delay(1000);  // Reduced from 1500ms
          float newWeight = readTotalWeight();
          weightChange = newWeight - state.lastTotalWeight;
          
          if (weightChange > 10.0) {
            state.individualWeights[i] = weightChange;
            state.lastTotalWeight = newWeight;
            setPositionLED(i + 1, COLOR_OCCUPIED, 50);
            Serial.println("[EVENT] Bottle PLACED at position " + String(i + 1) + " (" + String(weightChange) + "g)");
            sendBottleEvent(i + 1, currentState, weightChange);
          } else {
            lastPositions[i] = !currentState;
            state.positions[i] = !currentState;
          }
        } else {
          // Bottle removed
          weightChange = state.individualWeights[i];
          state.individualWeights[i] = 0.0;
          delay(1000);
          state.lastTotalWeight = readTotalWeight();
          setPositionLED(i + 1, COLOR_OFF, 0);
          Serial.println("[EVENT] Bottle REMOVED from position " + String(i + 1) + " (was " + String(weightChange) + "g)");
          sendBottleEvent(i + 1, currentState, weightChange);
        }
        
        processingChange = false;
        return;
      }
      
      processingChange = false;
    }
  }
}

// ==================== LED CONTROL ====================
void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness) {
  if (position < 1 || position > 9) return;
  
  uint8_t ledIndex = bottleToLed[position - 1];
  if (ledIndex >= NUM_LEDS) return;
  
  uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;  
  uint8_t b = (color & 0xFF) * brightness / 255;
  
  strip.setPixelColor(ledIndex, strip.Color(r, g, b));
  strip.show();
}

void setCOBLighting(uint16_t temperature, uint8_t brightness) {
  temperature = constrain(temperature, TEMP_MIN, TEMP_MAX);
  brightness = constrain(brightness, 0, 100);
  
  if (brightness == 0) {
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
  } 
  else if (action == "set_general_light") {
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setCOBLighting(temperature, brightness);
  }
  else if (action == "expect_bottle") {
    uint8_t position = data["position"];
    setPositionLED(position, COLOR_AVAILABLE, 100);
    Serial.println("[CMD] Expecting bottle at position " + String(position));
  } 
  else if (action == "read_sensors") {
    sendHeartbeat();
  }
}

void sendHeartbeat() {
  if (!mqttClient.connected()) return;
  
  state.lastTotalWeight = readTotalWeight();

  StaticJsonDocument<512> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  
  JsonObject data = doc.createNestedObject("data");
  data["weight"] = round(state.lastTotalWeight * 10.0) / 10.0;
  data["temp"] = state.ambientTemperature;
  data["humid"] = state.ambientHumidity;
  
  // Occupied array: [0,0,1,0,0,0,0,0,0]
  JsonArray occupied = data.createNestedArray("occupied");
  for (int i = 0; i < 9; i++) {
    occupied.add(state.positions[i] ? 1 : 0);
  }
  
  // Weights array: [0, 0, 102.5, 0, ...]
  JsonArray weights = data.createNestedArray("weights");
  for (int i = 0; i < 9; i++) {
    weights.add(round(state.individualWeights[i] * 10.0) / 10.0);
  }
  
  // Light status: "4000K 0%"
  String lightStatus = String(state.currentTemperature) + "K " + String(state.currentBrightness) + "%";
  data["light"] = lightStatus;
  
  data["wifi"] = WiFi.RSSI();
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(mqtt_topic_status, output.c_str());
  Serial.println("[HB] " + output);
}

void sendBottleEvent(uint8_t position, bool occupied, float weight) {
  if (!mqttClient.connected()) return;
  
  StaticJsonDocument<256> doc;
  doc["action"] = "bottle_event";
  doc["source"] = DRAWER_ID;
  
  JsonObject data = doc.createNestedObject("data");
  data["event"] = occupied ? "placed" : "removed";
  data["position"] = position;
  data["weight"] = weight;
  data["total_weight"] = state.lastTotalWeight;
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(mqtt_topic_status, output.c_str());
}