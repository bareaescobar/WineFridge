/*
 * WineFridge Drawer ESP32_DRAWER - SIMPLIFIED v3.0.3
 * SIMPLE WEIGHT SYSTEM: Tare after every change
 *
 * LOGIC:
 * 1. Bottle placed → Measure weight → Save
 * 2. TARE immediately → Reset to 0
 * 3. Next bottle → Measure from 0 → No error accumulation
 *
 * Version: 3.0.3
 * Date: 28.10.2025 18:15h
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

// ==================== CONFIGURATION ====================
#define DRAWER_ID "drawer_7"
#define FIRMWARE_VERSION "3.0.3"

// Network
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing
#define HEARTBEAT_INTERVAL 60000
#define DEBOUNCE_TIME 50
#define WEIGHT_STABILIZE_TIME 3000  // Increased from 300ms to 2000ms for accurate readings
#define SENSOR_UPDATE_INTERVAL 15000
#define WATCHDOG_TIMEOUT 30  // Increased from 10 to 30 seconds to accommodate weight stabilization

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
#define WEIGHT_CALIBRATION_FACTOR 94.302184 //positive only for drawer7
#define WEIGHT_THRESHOLD 100.0
#define WEIGHT_DEFAULT_ERROR 100.0
#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
#define TEMP_MIN 2700
#define TEMP_MAX 6500

// LED Colors
#define COLOR_OFF       0x000000
#define COLOR_AVAILABLE 0x00FF00
#define COLOR_OCCUPIED  0x444444
#define COLOR_ERROR     0xFF0000
#define COLOR_PROCESSING 0x0000FF

// ==================== STATE MACHINE ====================
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

// ==================== MQTT QUEUE ====================
#define MQTT_QUEUE_SIZE 20

struct MQTTEvent {
  char topic[64];
  char payload[512];
  uint8_t retries;
};

class MQTTQueue {
private:
  MQTTEvent queue[MQTT_QUEUE_SIZE];
  uint8_t head = 0;
  uint8_t tail = 0;
  uint8_t count = 0;

public:
  bool push(const char* topic, const char* payload) {
    if (count >= MQTT_QUEUE_SIZE) return false;
    
    strncpy(queue[tail].topic, topic, 63);
    strncpy(queue[tail].payload, payload, 511);
    queue[tail].retries = 0;
    
    tail = (tail + 1) % MQTT_QUEUE_SIZE;
    count++;
    return true;
  }

  MQTTEvent* peek() {
    if (count == 0) return nullptr;
    return &queue[head];
  }

  void pop() {
    if (count == 0) return;
    head = (head + 1) % MQTT_QUEUE_SIZE;
    count--;
  }

  uint8_t size() { return count; }
};

// ==================== GLOBALS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

BottlePosition positions[9];
MQTTQueue eventQueue;
volatile bool otaInProgress = false;
int expectedPosition = -1;  // Track expected bottle position (0-8), -1 = no expectation

struct SystemState {
  float individualWeights[9];      // Only store individual weights
  uint16_t currentTemperature;
  uint8_t currentBrightness;
  float ambientTemperature;
  float ambientHumidity;
  bool weightSensorReady;
  bool tempSensorReady;
  bool debugMode;
  unsigned long uptime;
} state;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

unsigned long lastHeartbeat = 0;
unsigned long lastSensorUpdate = 0;
unsigned long lastMQTTProcess = 0;

// ==================== LED ANIMATION ====================
struct LEDAnimation {
  uint32_t targetColor;
  uint32_t currentColor;
  uint8_t targetBrightness;
  uint8_t currentBrightness;
  bool blinking;
  unsigned long blinkTimer;
  bool blinkState;
};

LEDAnimation ledAnimations[NUM_LEDS];

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========================================");
  Serial.println("WineFridge Drawer - " + String(DRAWER_ID));
  Serial.println("Firmware: v" + String(FIRMWARE_VERSION));
  Serial.println("SIMPLE WEIGHT SYSTEM");
  Serial.println("========================================\n");

  // Watchdog
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  // Initialize state
  memset(&state, 0, sizeof(state));
  memset(&positions, 0, sizeof(positions));
  state.debugMode = false;
  
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  setupPins();
  setupLEDs();
  setupCOBLEDs();
  setupSensors();
  setupNetwork();
  setupOTA();
  
  // Initialize positions
  for (int i = 0; i < 9; i++) {
    positions[i].state = STATE_EMPTY;
    positions[i].weight = 0.0;
  }
  
  updateAllSensors();
  
  Serial.println("\n[READY] System initialized\n");
  esp_task_wdt_reset();
}

// ==================== HARDWARE SETUP ====================
void setupPins() {
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
  }
  Serial.println("[OK] GPIO pins configured");
}

void setupLEDs() {
  strip.begin();
  strip.clear();
  strip.show();
  
  for (int i = 0; i < NUM_LEDS; i++) {
    ledAnimations[i].currentColor = COLOR_OFF;
    ledAnimations[i].targetColor = COLOR_OFF;
    ledAnimations[i].currentBrightness = 0;
    ledAnimations[i].targetBrightness = 0;
    ledAnimations[i].blinking = false;
  }
  
  Serial.println("[OK] WS2812B LEDs initialized");
}

void setupCOBLEDs() {
  pinMode(COB_WARM_PIN, OUTPUT);
  pinMode(COB_COOL_PIN, OUTPUT);
  digitalWrite(COB_WARM_PIN, LOW);
  digitalWrite(COB_COOL_PIN, LOW);
  delay(50);
  
  ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcWrite(COB_WARM_PIN, 0);
  ledcWrite(COB_COOL_PIN, 0);
  
  state.currentTemperature = 4000;
  state.currentBrightness = 0;
  
  Serial.println("[OK] COB LEDs initialized");
}

void setupSensors() {
  // Weight sensor
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  delay(500);
  
  if (scale.is_ready()) {
    scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
    delay(200);
    scale.tare();  // Start with clean baseline
    state.weightSensorReady = true;
    Serial.println("[OK] HX711 weight sensor ready & tared");
  } else {
    state.weightSensorReady = false;
    Serial.println("[ERROR] HX711 weight sensor FAILED");
  }
  
  // Temperature sensor
  Wire.begin(21, 22);
  if (sht31.begin(0x44)) {
    state.tempSensorReady = true;
    Serial.println("[OK] SHT31 temperature/humidity sensor ready");
  } else {
    state.tempSensorReady = false;
    Serial.println("[ERROR] SHT31 sensor FAILED");
  }
}

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
    Serial.println("[OK] Signal: " + String(WiFi.RSSI()) + " dBm");
    
    mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
    mqttClient.setCallback(onMQTTMessage);
    mqttClient.setBufferSize(1024);
    
    connectMQTT();
  } else {
    Serial.println("\n[ERROR] WiFi connection failed");
  }
}

void setupOTA() {
  ArduinoOTA.setHostname("WineFridge-Drawer7");  // Friendly name for OTA discovery
  ArduinoOTA.setPassword("winefridge2025");
  ArduinoOTA.setPort(3232);
  
  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    Serial.println("\n[OTA] Update starting...");
    
    esp_task_wdt_delete(NULL);
    
    if (mqttClient.connected()) {
      mqttClient.disconnect();
    }
    
    strip.clear();
    strip.show();
    ledcWrite(COB_WARM_PIN, 0);
    ledcWrite(COB_COOL_PIN, 0);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 500) {
      Serial.printf("[OTA] Progress: %u%%\n", (progress / (total / 100)));
      lastPrint = millis();
      yield();
    }
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("\n[OTA] Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
    
    otaInProgress = false;
    esp_task_wdt_add(NULL);
  });
  
  ArduinoOTA.begin();
  Serial.println("[OK] OTA enabled on port 3232");
}

void connectMQTT() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
  
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("[OK] MQTT connected to " + String(MQTT_BROKER_IP));
    mqttClient.subscribe(mqtt_topic_command);
    sendStartupMessage();
  } else {
    Serial.println("[ERROR] MQTT connection failed: " + String(mqttClient.state()));
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  // OTA priority
  if (otaInProgress) {
    ArduinoOTA.handle();
    delay(10);
    return;
  }
  
  esp_task_wdt_reset();
  state.uptime = millis();
  
  ArduinoOTA.handle();
  
  // Network maintenance
  static unsigned long lastConnCheck = 0;
  if (millis() - lastConnCheck > 10000) {
    maintainConnections();
    lastConnCheck = millis();
  }
  
  // MQTT
  if (mqttClient.connected()) {
    mqttClient.loop();
    
    if (millis() - lastMQTTProcess > 100) {
      processMQTTQueue();
      lastMQTTProcess = millis();
    }
  }
  
  // Position state machines
  for (int i = 0; i < 9; i++) {
    updatePositionStateMachine(i);
  }
  
  updateLEDAnimations();
  
  // Sensors
  if (millis() - lastSensorUpdate > SENSOR_UPDATE_INTERVAL) {
    updateAllSensors();
    lastSensorUpdate = millis();
  }
  
  // Heartbeat
  if (millis() - lastHeartbeat > HEARTBEAT_INTERVAL) {
    queueHeartbeat();
    lastHeartbeat = millis();
  }
  
  yield();
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
        
        if (state.debugMode) {
          Serial.printf("[DEBUG] Pos %d: EMPTY → DEBOUNCING\n", posIndex + 1);
        }
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
          // Check if this is the wrong position
          if (expectedPosition != -1 && expectedPosition != posIndex) {
            // Wrong position detected - skip weighing
            pos->weight = WEIGHT_DEFAULT_ERROR;
            state.individualWeights[posIndex] = pos->weight;
            pos->state = STATE_OCCUPIED;

            setLEDAnimation(posIndex + 1, COLOR_ERROR, 100, false);

            Serial.printf("[EVENT] ⚠️  Wrong position! Expected %d, got %d (%.1fg)\n",
                         expectedPosition + 1, posIndex + 1, pos->weight);

            queueBottleEvent(posIndex + 1, true, pos->weight);

            if (state.debugMode) {
              Serial.printf("[DEBUG] Pos %d: DEBOUNCING → OCCUPIED (wrong position)\n", posIndex + 1);
            }
          } else {
            // Correct position or no expectation - proceed to weighing
            pos->state = STATE_WEIGHING;
            pos->stateTimer = millis();
            setLEDAnimation(posIndex + 1, COLOR_PROCESSING, 100, true);

            if (state.debugMode) {
              Serial.printf("[DEBUG] Pos %d: DEBOUNCING → WEIGHING\n", posIndex + 1);
            }
          }
        }
      }
      break;
    
    case STATE_WEIGHING:
      if (millis() - pos->stateTimer > WEIGHT_STABILIZE_TIME) {
        // Read weight change from 0 baseline
        float weightChange = readCurrentWeight();
        
        if (state.debugMode) {
          Serial.printf("[DEBUG] Pos %d: Weight reading = %.1fg\n", posIndex + 1, weightChange);
        }
        
        // Switch is authority - accept bottle
        if (weightChange >= WEIGHT_THRESHOLD) {
          pos->weight = weightChange;
        } else {
          // Low weight detected but switch is active - use default
          pos->weight = WEIGHT_DEFAULT_ERROR;
          Serial.printf("[WEIGHT] ⚠️  Pos %d: Low weight (%.1fg), using default %dg\n",
                       posIndex + 1, weightChange, (int)WEIGHT_DEFAULT_ERROR);
        }
        
        state.individualWeights[posIndex] = pos->weight;
        pos->state = STATE_OCCUPIED;
        
        setLEDAnimation(posIndex + 1, COLOR_OCCUPIED, 50, false);
        
        Serial.printf("[EVENT] ✓ Bottle PLACED at position %d (%.1fg)\n", 
                     posIndex + 1, 
                     pos->weight);
        
        // Send event
        queueBottleEvent(posIndex + 1, true, pos->weight);

        // Clear expected position after successful placement
        expectedPosition = -1;

        // CRITICAL: TARE IMMEDIATELY after measuring
        delay(200);  // Small delay for stability
        if (state.weightSensorReady && scale.is_ready()) {
          scale.tare();
          Serial.println("[WEIGHT] ✓ Tared - baseline reset to 0");
        }
      }
      break;
    
    case STATE_OCCUPIED:
      if (!switchPressed) {
        pos->state = STATE_REMOVING;
        pos->stateTimer = millis();
        pos->debounceCount = 1;
        
        if (state.debugMode) {
          Serial.printf("[DEBUG] Pos %d: OCCUPIED → REMOVING\n", posIndex + 1);
        }
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
          float removedWeight = pos->weight;
          
          pos->weight = 0.0;
          state.individualWeights[posIndex] = 0.0;
          pos->state = STATE_EMPTY;
          
          setLEDAnimation(posIndex + 1, COLOR_OFF, 0, false);
          
          Serial.printf("[EVENT] ✓ Bottle REMOVED from position %d (was %.1fg)\n", 
                       posIndex + 1, 
                       removedWeight);
          
          // Send event
          queueBottleEvent(posIndex + 1, false, removedWeight);
          
          // CRITICAL: TARE IMMEDIATELY after removal
          delay(200);
          if (state.weightSensorReady && scale.is_ready()) {
            scale.tare();
            Serial.println("[WEIGHT] ✓ Tared - baseline reset to 0");
          }
        }
      }
      break;
  }
}

// ==================== SENSOR FUNCTIONS ====================
float readCurrentWeight() {
  if (!state.weightSensorReady || !scale.is_ready()) {
    return 0.0;
  }
  
  // Single fast reading
  float reading = scale.get_units(1);
  
  if (reading >= -100 && reading <= 5000 && !isnan(reading) && !isinf(reading)) {
    return max(0.0f, reading);
  }
  
  return 0.0;
}

float getTotalWeight() {
  // Total weight = sum of individual weights stored in memory
  float total = 0.0;
  for (int i = 0; i < 9; i++) {
    total += state.individualWeights[i];
  }
  return total;
}

void updateAllSensors() {
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
void setLEDAnimation(uint8_t position, uint32_t color, uint8_t brightness, bool blink) {
  if (position < 1 || position > 9) return;
  
  uint8_t ledIndex = bottleToLed[position - 1];
  if (ledIndex >= NUM_LEDS) return;
  
  ledAnimations[ledIndex].targetColor = color;
  ledAnimations[ledIndex].targetBrightness = brightness;
  ledAnimations[ledIndex].blinking = blink;
  ledAnimations[ledIndex].blinkTimer = millis();
  ledAnimations[ledIndex].blinkState = true;
}

void updateLEDAnimations() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate < 50) return;
  lastUpdate = millis();
  
  bool needsShow = false;
  
  for (int i = 0; i < NUM_LEDS; i++) {
    LEDAnimation* anim = &ledAnimations[i];
    
    if (anim->blinking) {
      if (millis() - anim->blinkTimer > 500) {
        anim->blinkState = !anim->blinkState;
        anim->blinkTimer = millis();
        needsShow = true;
      }
      
      if (anim->blinkState) {
        strip.setPixelColor(i, anim->targetColor);
      } else {
        strip.setPixelColor(i, COLOR_OFF);
      }
    } else {
      if (anim->currentColor != anim->targetColor || 
          anim->currentBrightness != anim->targetBrightness) {
        
        anim->currentColor = anim->targetColor;
        anim->currentBrightness = anim->targetBrightness;
        
        uint8_t r = ((anim->currentColor >> 16) & 0xFF) * anim->currentBrightness / 100;
        uint8_t g = ((anim->currentColor >> 8) & 0xFF) * anim->currentBrightness / 100;
        uint8_t b = (anim->currentColor & 0xFF) * anim->currentBrightness / 100;
        
        strip.setPixelColor(i, strip.Color(r, g, b));
        needsShow = true;
      }
    }
  }
  
  if (needsShow) {
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

// ==================== MQTT FUNCTIONS ====================
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
      bool blink = pos["blink"] | false;  // Read blink field, default to false if not present

      if (position >= 1 && position <= 9) {
        uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
        setLEDAnimation(position, color, brightness, blink);
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
    expectedPosition = position - 1;  // Store expected position index (0-8)
    setLEDAnimation(position, COLOR_AVAILABLE, 100, false);
    Serial.printf("[CMD] Expecting bottle at position %d\n", position);
  }
  else if (action == "read_sensors") {
    queueHeartbeat();
  }
  else if (action == "debug_mode") {
    state.debugMode = data["enabled"];
    Serial.printf("[CMD] Debug mode: %s\n", state.debugMode ? "ON" : "OFF");
  }
  else if (action == "manual_tare") {
    Serial.println("[CMD] Manual tare...");
    if (scale.is_ready()) {
      scale.tare();
      Serial.println("[CMD] ✓ Tare complete");
    }
  }
  else if (action == "reboot") {
    Serial.println("[CMD] Rebooting in 2 seconds...");
    delay(2000);
    ESP.restart();
  }
}

void processMQTTQueue() {
  if (!mqttClient.connected()) return;
  
  MQTTEvent* event = eventQueue.peek();
  if (event == nullptr) return;
  
  if (mqttClient.publish(event->topic, event->payload)) {
    if (state.debugMode) {
      Serial.printf("[MQTT] ✓ Sent\n");
    }
    eventQueue.pop();
  } else {
    event->retries++;
    if (event->retries > 3) {
      Serial.printf("[MQTT] ✗ Failed after retries\n");
      eventQueue.pop();
    }
  }
}

void queueHeartbeat() {
  StaticJsonDocument<640> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  
  JsonObject data = doc.createNestedObject("data");
  
  // Total weight = sum of individual weights
  data["weight"] = round(getTotalWeight() * 10.0) / 10.0;
  data["temp"] = state.ambientTemperature;
  data["humid"] = state.ambientHumidity;
  
  JsonArray occupied = data.createNestedArray("occupied");
  for (int i = 0; i < 9; i++) {
    occupied.add((positions[i].state == STATE_OCCUPIED) ? 1 : 0);
  }
  
  JsonArray weights = data.createNestedArray("weights");
  for (int i = 0; i < 9; i++) {
    weights.add(round(state.individualWeights[i] * 10.0) / 10.0);
  }
  
  String lightStatus = String(state.currentTemperature) + "K " + 
                      String(state.currentBrightness) + "%";
  data["light"] = lightStatus;
  data["wifi"] = WiFi.RSSI();
  data["uptime"] = state.uptime / 1000;
  data["firmware"] = FIRMWARE_VERSION;
  
  String output;
  serializeJson(doc, output);
  
  eventQueue.push(mqtt_topic_status, output.c_str());
}

void queueBottleEvent(uint8_t position, bool occupied, float weight) {
  StaticJsonDocument<320> doc;
  doc["action"] = "bottle_event";
  doc["source"] = DRAWER_ID;
  
  JsonObject data = doc.createNestedObject("data");
  data["event"] = occupied ? "placed" : "removed";
  data["position"] = position;
  data["weight"] = round(weight * 10.0) / 10.0;
  data["total_weight"] = round(getTotalWeight() * 10.0) / 10.0;
  
  String output;
  serializeJson(doc, output);
  
  eventQueue.push(mqtt_topic_status, output.c_str());
}

void sendStartupMessage() {
  StaticJsonDocument<256> doc;
  doc["action"] = "startup";
  doc["source"] = DRAWER_ID;
  
  JsonObject data = doc.createNestedObject("data");
  data["firmware"] = FIRMWARE_VERSION;
  data["uptime"] = 0;
  data["ip"] = WiFi.localIP().toString();
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(mqtt_topic_status, output.c_str());
  Serial.println("[MQTT] Startup message sent");
}

// ==================== NETWORK ====================
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