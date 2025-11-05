/*
 * WineFridge Drawer ESP32_DRAWER7 - COMPLETE v5.0.0
 * SIMPLIFIED WEIGHT SYSTEM: Tare after every change
 *
 * LOGIC:
 * 1. Bottle placed → Measure weight → Save
 * 2. TARE immediately → Reset to 0
 * 3. Next bottle → Measure from 0 → No error accumulation
 *
 * Version: 5.0.0
 * Date: November 4, 2025
 *
 * CHANGELOG v5.0.0:
 * - DUAL WiFi support with automatic fallback
 * - Fixed watchdog API for ESP32 v3.x
 * - OTA stable (watchdog disabled during upload)
 * - No mDNS (IP-only for stability)
 * - IP address in heartbeat
 * - Wrong position detection WITHOUT weighing
 * - Green blinking LED during weighing
 * - expectedPosition tracking for LOAD operations
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
#define FIRMWARE_VERSION "5.0.0"

// Dual WiFi Configuration - Fallback automático
#define WIFI_SSID_1 "Winefridge"
#define WIFI_PASSWORD_1 "aabbccdd"

#define WIFI_SSID_2 "MOVISTAR-WIFI6-65F8"          // Cambiar por tu segunda red
#define WIFI_PASSWORD_2 "sA77n4kXrss9k9fn377i"     // Cambiar por tu segunda clave

// MQTT Configuration
#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

// Timing
#define HEARTBEAT_INTERVAL 60000
#define DEBOUNCE_TIME 50
#define WEIGHT_STABILIZE_TIME 3000  // 3s for reliable weight readings
#define SENSOR_UPDATE_INTERVAL 15000
#define WATCHDOG_TIMEOUT 30  // 30 seconds to accommodate weight stabilization

// Hardware - FIXED PINOUT (evita GPIO2 reservado, usa GPIOs con PWM)
const uint8_t SWITCH_PINS[9] = {5, 32, 26, 14, 4, 23, 33, 27, 12};
const uint8_t bottleToLed[9] = {2, 6, 10, 14, 0, 4, 8, 12, 16};

#define WS2812_DATA_PIN 13
#define NUM_LEDS 17
#define HX711_DT_PIN 19
#define HX711_SCK_PIN 18

#define COB_WARM_PIN 2   
#define COB_COOL_PIN 25   

// Weight - Calibration POSITIVE for drawer 7
#define WEIGHT_CALIBRATION_FACTOR 94.302184
#define WEIGHT_THRESHOLD 100.0
#define WEIGHT_DEFAULT_ERROR 100.0

// COB LED
#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
#define TEMP_MIN 2700
#define TEMP_MAX 6500

// LED Colors
#define COLOR_OFF       0x000000
#define COLOR_AVAILABLE 0x00FF00  // Green
#define COLOR_OCCUPIED  0x444444  // White/Gray
#define COLOR_ERROR     0xFF0000  // Red
#define COLOR_PROCESSING 0x0000FF // Blue

// ==================== STATE MACHINE ====================
enum BottleState {
  STATE_EMPTY,
  STATE_DEBOUNCING,
  STATE_WEIGHING,
  STATE_OCCUPIED
};

struct BottlePosition {
  BottleState state;
  float weight;
  unsigned long stateTimer;
  uint8_t debounceCount;
};

// ==================== GLOBAL OBJECTS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// ==================== STATE ====================
struct {
  BottlePosition positions[9];
  float individualWeights[9];
  bool debugMode;
  float ambientTemperature;
  float ambientHumidity;
  uint16_t currentTemperature;
  uint8_t currentBrightness;
  unsigned long uptime;
  uint8_t expectedPosition;  // 0 = no expectation, 1-9 = expected position for LOAD
  String ipAddress;          // Store IP address
} state;

BottlePosition positions[9];

char mqtt_topic_status[64];
char mqtt_topic_command[64];

unsigned long lastHeartbeat = 0;
unsigned long lastSensorUpdate = 0;
bool otaInProgress = false;  // Flag para pausar loop durante OTA

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

// ==================== MQTT MESSAGE QUEUE ====================
struct MQTTMessage {
  char topic[64];
  char payload[512];
};

class SimpleQueue {
private:
  MQTTMessage queue[10];
  int front, rear, count;
public:
  SimpleQueue() : front(0), rear(0), count(0) {}
  
  bool push(const char* topic, const char* payload) {
    if (count >= 10) return false;
    strncpy(queue[rear].topic, topic, 63);
    strncpy(queue[rear].payload, payload, 511);
    rear = (rear + 1) % 10;
    count++;
    return true;
  }
  
  bool pop(char* topic, char* payload) {
    if (count == 0) return false;
    strcpy(topic, queue[front].topic);
    strcpy(payload, queue[front].payload);
    front = (front + 1) % 10;
    count--;
    return true;
  }
  
  bool isEmpty() { return count == 0; }
};

SimpleQueue eventQueue;

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n\n========================================");
  Serial.println("WineFridge Drawer - " + String(DRAWER_ID));
  Serial.println("Firmware: v" + String(FIRMWARE_VERSION));
  Serial.println("SIMPLE WEIGHT SYSTEM");
  Serial.println("========================================\n");

  // Watchdog - Nueva API ESP32 v3.x
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
  state.expectedPosition = 0;  // No position expected initially
  
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
  
  sendStartupMessage();
}

// ==================== HARDWARE SETUP ====================
void setupPins() {
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
  }
  Serial.println("[OK] Switches configured");
}

void setupLEDs() {
  strip.begin();
  strip.clear();
  strip.show();
  
  for (int i = 0; i < NUM_LEDS; i++) {
    ledAnimations[i].targetColor = COLOR_OFF;
    ledAnimations[i].currentColor = COLOR_OFF;
    ledAnimations[i].targetBrightness = 0;
    ledAnimations[i].currentBrightness = 0;
    ledAnimations[i].blinking = false;
    ledAnimations[i].blinkState = false;
  }
  
  Serial.println("[OK] WS2812B LEDs initialized");
}

void setupCOBLEDs() {
  // Setup PWM for COB LEDs - FIXED pins with PWM capability
  ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  
  ledcWrite(COB_WARM_PIN, 0);
  ledcWrite(COB_COOL_PIN, 0);
  
  state.currentTemperature = 2700;
  state.currentBrightness = 0;
  
  Serial.println("[OK] COB LEDs initialized");
}

void setupSensors() {
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(WEIGHT_CALIBRATION_FACTOR);
  scale.tare();
  
  Serial.println("[OK] Weight sensor initialized");
  
  if (sht31.begin(0x44)) {
    Serial.println("[OK] Temperature sensor initialized");
  } else {
    Serial.println("[WARN] Temperature sensor not found");
  }
}

void setupNetwork() {
  Serial.println("\n[WIFI] Starting dual WiFi connection...");
  
  // Try primary WiFi first
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID_1, WIFI_PASSWORD_1);
  
  Serial.print("[WIFI] Connecting to ");
  Serial.print(WIFI_SSID_1);
  Serial.print("...");
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
    esp_task_wdt_reset();
  }
  
  // If primary fails, try secondary
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\n[WIFI] Primary failed, trying secondary...");
    WiFi.begin(WIFI_SSID_2, WIFI_PASSWORD_2);
    
    Serial.print("[WIFI] Connecting to ");
    Serial.print(WIFI_SSID_2);
    Serial.print("...");
    
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
    Serial.println("\n[WIFI] ✓ Connected!");
    Serial.println("[WIFI] IP: " + state.ipAddress);
    Serial.println("[WIFI] SSID: " + WiFi.SSID());
  } else {
    Serial.println("\n[WIFI] ✗ Connection failed");
    state.ipAddress = "0.0.0.0";
  }
  
  // Setup MQTT
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqttClient.setCallback(onMQTTMessage);
  mqttClient.setBufferSize(1024);
  
  connectMQTT();
}

void setupOTA() {
  // OTA Configuration - SIN hostname (usar IP directa para evitar problemas mDNS)
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setPassword("winefridge2025");
  
  ArduinoOTA.onStart([]() {
    otaInProgress = true;  // Pausar loop principal
    Serial.println("\n[OTA] Update starting...");
    
    // Disable watchdog durante OTA
    esp_task_wdt_deinit();
    
    // Desconectar MQTT
    mqttClient.disconnect();
    
    // Apagar LEDs y sensores
    strip.clear();
    strip.show();
    ledcWrite(COB_WARM_PIN, 0);
    ledcWrite(COB_COOL_PIN, 0);
  });
  
  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Update complete!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastReport = 0;
    if (millis() - lastReport > 500) {  // Report every 500ms
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
    
    // Reiniciar ESP32 después de error OTA
    delay(1000);
    ESP.restart();
  });
  
  ArduinoOTA.begin();
  Serial.println("[OK] OTA ready (use IP: " + state.ipAddress + ")");
}

// ==================== MQTT ====================
void connectMQTT() {
  if (otaInProgress) return;  // No conectar durante OTA
  
  int attempts = 0;
  while (!mqttClient.connected() && attempts < 3) {
    Serial.print("[MQTT] Connecting...");
    if (mqttClient.connect(DRAWER_ID)) {
      Serial.println(" ✓");
      mqttClient.subscribe(mqtt_topic_command);
    } else {
      Serial.print(" ✗ (");
      Serial.print(mqttClient.state());
      Serial.println(")");
      delay(2000);
      attempts++;
    }
    esp_task_wdt_reset();
  }
}

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
        setLEDAnimation(position, color, brightness, blink);
      }
    }
    
  } else if (action == "set_general_light") {
    uint16_t temperature = data["temperature"];
    uint8_t brightness = data["brightness"];
    setCOBLighting(temperature, brightness);
    
  } else if (action == "expect_bottle") {
    uint8_t position = data["position"];
    state.expectedPosition = position;  // 0 = no expectation, 1-9 = expected position
    
    if (state.debugMode) {
      Serial.printf("[DEBUG] Expecting bottle at position %d\n", position);
    }
    
  } else if (action == "debug_mode") {
    state.debugMode = data["enabled"];
    Serial.printf("[DEBUG] Debug mode: %s\n", state.debugMode ? "ON" : "OFF");
  }
}

// ==================== MAIN LOOP ====================
void loop() {
  if (otaInProgress) {
    ArduinoOTA.handle();  // Solo manejar OTA cuando está en progreso
    return;
  }
  
  esp_task_wdt_reset();
  
  // Network
  if (WiFi.status() != WL_CONNECTED) {
    setupNetwork();
  }
  
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  // OTA
  ArduinoOTA.handle();
  
  // State machines
  for (int i = 0; i < 9; i++) {
    updatePositionStateMachine(i);
  }
  
  // LED animations
  updateLEDAnimations();
  
  // MQTT queue
  processEventQueue();
  
  // Sensors
  if (millis() - lastSensorUpdate > SENSOR_UPDATE_INTERVAL) {
    updateAllSensors();
    lastSensorUpdate = millis();
  }
  
  // Heartbeat with IP
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
          // Check if this is wrong position during load operation
          if (state.expectedPosition != 0 && (posIndex + 1) != state.expectedPosition) {
            // Wrong position detected - don't weigh, just show red LED and send error
            Serial.printf("[ERROR] Wrong position! Expected %d, got %d\n", state.expectedPosition, posIndex + 1);

            // Set weight to default error value
            pos->weight = WEIGHT_DEFAULT_ERROR;
            state.individualWeights[posIndex] = pos->weight;

            // Go to OCCUPIED state (not EMPTY) so LED turns off when bottle is removed
            pos->state = STATE_OCCUPIED;

            setLEDAnimation(posIndex + 1, COLOR_ERROR, 100, false);  // Red LED

            // Send wrong placement event
            StaticJsonDocument<256> wrongDoc;
            wrongDoc["action"] = "wrong_placement";
            wrongDoc["source"] = DRAWER_ID;
            wrongDoc["data"]["position"] = posIndex + 1;
            wrongDoc["data"]["expected_position"] = state.expectedPosition;
            wrongDoc["timestamp"] = millis();

            char wrongPayload[256];
            serializeJson(wrongDoc, wrongPayload);
            eventQueue.push(mqtt_topic_status, wrongPayload);
          } else {
            // Correct position or no expected position - proceed with weighing
            pos->state = STATE_WEIGHING;
            pos->stateTimer = millis();
            setLEDAnimation(posIndex + 1, COLOR_AVAILABLE, 100, true);  // Green blinking during weighing

            if (state.debugMode) {
              Serial.printf("[DEBUG] Pos %d: DEBOUNCING → WEIGHING\n", posIndex + 1);
            }
          }
        }
      }
      break;
    
    case STATE_WEIGHING:
      if (millis() - pos->stateTimer > WEIGHT_STABILIZE_TIME) {
        esp_task_wdt_reset();  // Reset watchdog before heavy operations

        // Read weight change from 0 baseline
        float weightChange = readCurrentWeight();

        Serial.printf("[WEIGHT] Pos %d: Raw reading = %.1fg\n", posIndex + 1, weightChange);

        // Switch is authority - accept bottle
        if (weightChange >= WEIGHT_THRESHOLD) {
          pos->weight = weightChange;
          Serial.printf("[WEIGHT] ✓ Pos %d: Accepted weight %.1fg\n", posIndex + 1, weightChange);
        } else {
          // Low weight detected but switch is active - use default
          pos->weight = WEIGHT_DEFAULT_ERROR;
          Serial.printf("[WEIGHT] ⚠️  Pos %d: Low weight (%.1fg), using default %dg\n",
                       posIndex + 1, weightChange, WEIGHT_DEFAULT_ERROR);
        }

        state.individualWeights[posIndex] = pos->weight;
        pos->state = STATE_OCCUPIED;
        
        setLEDAnimation(posIndex + 1, COLOR_AVAILABLE, 100, false);  // Green solid after weighing

        // TARE immediately after measurement
        scale.tare();
        Serial.printf("[TARE] Position %d measured, baseline reset\n", posIndex + 1);

        queueBottleEvent(posIndex + 1, true, pos->weight);

        if (state.debugMode) {
          Serial.printf("[DEBUG] Pos %d: WEIGHING → OCCUPIED (%.1fg)\n", posIndex + 1, pos->weight);
        }
      }
      break;
    
    case STATE_OCCUPIED:
      if (!switchPressed) {
        pos->state = STATE_EMPTY;
        pos->weight = 0.0;
        state.individualWeights[posIndex] = 0.0;
        
        setLEDAnimation(posIndex + 1, COLOR_OFF, 0, false);

        // TARE after bottle removed
        scale.tare();
        Serial.printf("[TARE] Position %d cleared, baseline reset\n", posIndex + 1);

        queueBottleEvent(posIndex + 1, false, 0.0);

        if (state.debugMode) {
          Serial.printf("[DEBUG] Pos %d: OCCUPIED → EMPTY\n", posIndex + 1);
        }
      }
      break;
  }
}

float readCurrentWeight() {
  if (!scale.wait_ready_timeout(1000)) {
    Serial.println("[WEIGHT] Sensor timeout");
    return 0.0;
  }
  
  float reading = scale.get_units(5);  // 5 samples average
  return reading;
}

float getTotalWeight() {
  float total = 0.0;
  for (int i = 0; i < 9; i++) {
    total += state.individualWeights[i];
  }
  return total;
}

// ==================== LED CONTROL ====================
void setLEDAnimation(uint8_t position, uint32_t color, uint8_t brightness, bool blink) {
  if (position < 1 || position > 9) return;
  
  uint8_t ledIndex = bottleToLed[position - 1];
  
  ledAnimations[ledIndex].targetColor = color;
  ledAnimations[ledIndex].targetBrightness = brightness;
  ledAnimations[ledIndex].blinking = blink;
  
  if (blink) {
    ledAnimations[ledIndex].blinkTimer = millis();
    ledAnimations[ledIndex].blinkState = true;
  }
}

void updateLEDAnimations() {
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
        uint8_t r = ((anim->targetColor >> 16) & 0xFF) * anim->targetBrightness / 100;
        uint8_t g = ((anim->targetColor >> 8) & 0xFF) * anim->targetBrightness / 100;
        uint8_t b = (anim->targetColor & 0xFF) * anim->targetBrightness / 100;
        strip.setPixelColor(i, strip.Color(r, g, b));
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

// ==================== MQTT REPORTING ====================
void updateAllSensors() {
  if (sht31.begin(0x44)) {
    state.ambientTemperature = sht31.readTemperature();
    state.ambientHumidity = sht31.readHumidity();
  }
  state.uptime = millis();
}

void sendHeartbeat() {
  StaticJsonDocument<768> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["ip"] = state.ipAddress;  // Include IP in heartbeat
  
  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = state.uptime / 1000;
  data["wifi_rssi"] = WiFi.RSSI();
  data["free_heap"] = ESP.getFreeHeap();
  data["temperature"] = round(state.ambientTemperature * 10.0) / 10.0;
  data["humidity"] = round(state.ambientHumidity * 10.0) / 10.0;
  data["total_weight"] = round(getTotalWeight() * 10.0) / 10.0;
  
  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    positions.add(state.individualWeights[i] > 50.0 ? 1 : 0);
  }
  
  JsonArray weights = data.createNestedArray("weights");
  for (int i = 0; i < 9; i++) {
    weights.add(round(state.individualWeights[i] * 10.0) / 10.0);
  }
  
  String lightStatus = String(state.currentTemperature) + "K " + 
                      String(state.currentBrightness) + "%";
  data["light"] = lightStatus;
  
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
  data["ip"] = state.ipAddress;
  
  String output;
  serializeJson(doc, output);
  
  mqttClient.publish(mqtt_topic_status, output.c_str());
  Serial.println("[MQTT] Startup message sent");
}

void processEventQueue() {
  if (eventQueue.isEmpty() || !mqttClient.connected()) return;
  
  char topic[64];
  char payload[512];
  
  if (eventQueue.pop(topic, payload)) {
    mqttClient.publish(topic, payload);
  }
}
