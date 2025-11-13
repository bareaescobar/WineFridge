/*
 * WineFridge Drawer ESP32_DRAWER5 v5.3.0
 * FULL firmware corregido para OTA estable
 *
 * Cambios clave respecto v5.3.0:
 *  - Watchdog aumentado a 75s
 *  - Protecciones OTA: no ejecutar tareas bloqueantes durante OTA
 *  - Llamada única a ArduinoOTA.handle() en loop
 *  - Buffer MQTT reducido a 1024
 *  - WiFi sleep activado durante OTA start, restaurado en end
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

#define DRAWER_ID "drawer_5"
#define FIRMWARE_VERSION "5.3.0"

// Dual WiFi
#define WIFI_SSID_1 "Solo Spirits"
#define WIFI_PASSWORD_1 "Haveadrink"
#define WIFI_SSID_2 "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD_2 "sA77n4kXrss9k9fn377i"

#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

#define HEARTBEAT_INTERVAL 60000
#define DEBOUNCE_TIME 50
#define WEIGHT_STABILIZE_TIME 1500
#define WATCHDOG_TIMEOUT 75   // segundos (modificado a 75s)

// Hardware
const uint8_t SWITCH_PINS[9] = {5, 32, 26, 14, 4, 23, 33, 27, 12};
const uint8_t bottleToLed[9] = {2, 6, 10, 14, 0, 4, 8, 12, 16};

#define WS2812_DATA_PIN 13
#define NUM_LEDS 17
#define HX711_DT_PIN 19
#define HX711_SCK_PIN 18
#define COB_WARM_PIN 2
#define COB_COOL_PIN 25

#define WEIGHT_CALIBRATION_FACTOR -94.302184
#define WEIGHT_THRESHOLD 50.0
#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
#define TEMP_MIN 2700
#define TEMP_MAX 6500

// Blink settings
#define BLINK_INTERVAL 500  // 500ms on/off

// ==================== GLOBALS ====================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);
HX711 scale;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

volatile bool otaInProgress = false;

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
  uint8_t expectedPosition = 0;
  String ipAddress = "0.0.0.0";
  String connectedSSID = "";
} state;

// LED blink state
struct LEDBlinkState {
  bool enabled;
  uint32_t color;
  uint8_t brightness;
  unsigned long lastToggle;
  bool currentState;
};

LEDBlinkState ledBlinkStates[9];

char mqtt_topic_status[64];
char mqtt_topic_command[64];

// Forward declarations
void setupNetwork();
void connectMQTT();
void setupOTA();
void maintainConnections();
void sendHeartbeat();
void sendStartupMessage();
void readAllPositions();
float readTotalWeight();
void updateTemperatureHumidity();
void checkPositionChanges();
void updateBlinkingLEDs();
void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness, bool blink);
void setCOBLighting(uint16_t temperature, uint8_t brightness);
void allLightsOff();
void handleCommand(JsonDocument& doc);

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

  // Configurar WDT con 75s
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  // Reinit for safety
  esp_task_wdt_deinit();
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);

  memset(&state.positions, 0, sizeof(state.positions));
  memset(&state.individualWeights, 0, sizeof(state.individualWeights));
  memset(&ledBlinkStates, 0, sizeof(ledBlinkStates));
  
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", DRAWER_ID);
  
  Serial.println("[INIT] Hardware...");
  
  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
  }
  
  strip.begin();
  strip.clear();
  strip.show();
  Serial.println("[OK] WS2812B LEDs");
  
  // Keep original ledcAttach usage to match your hardware setup
  // But in this drawer sketch pins are COB_WARM_PIN/COB_COOL_PIN
  // We'll use ledcAttach as per your base code:
  ledcAttach(COB_WARM_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcAttach(COB_COOL_PIN, COB_PWM_FREQUENCY, COB_PWM_RESOLUTION);
  ledcWrite(COB_WARM_PIN, 0);
  ledcWrite(COB_COOL_PIN, 0);
  Serial.println("[OK] COB LEDs");
  
  Serial.println("[INIT] Weight sensor...");
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
      Serial.println("[TARE] No bottles");
      scale.tare();
      state.lastTotalWeight = 0.0;
    } else {
      Serial.println("[SKIP] Bottles present");
      state.lastTotalWeight = scale.get_units(3);
    }
    
    state.weightSensorReady = true;
    Serial.println("[OK] Weight sensor");
  } else {
    Serial.println("[ERROR] Weight sensor");
  }
  
  Wire.begin(21, 22);
  if (sht31.begin(0x44)) {
    state.tempSensorReady = true;
    Serial.println("[OK] Temperature sensor");
  } else {
    Serial.println("[ERROR] Temperature sensor");
  }
  
  setupNetwork();
  
  if (WiFi.status() == WL_CONNECTED) {
    setupOTA();
  }
  
  readAllPositions();
  updateTemperatureHumidity();
  
  Serial.println("\n[READY] System initialized\n");
  esp_task_wdt_reset();
}

// ==================== NETWORK / MQTT ====================
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
    Serial.println("[WIFI] Resetting WiFi module...");
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
    Serial.println("[WIFI] SSID: " + state.connectedSSID);
    
    mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
    mqttClient.setCallback(onMQTTMessage);
    mqttClient.setBufferSize(1024); // reducir buffer para liberar heap
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
    sendStartupMessage();
  } else {
    Serial.print(" ✗ (");
    Serial.print(mqttClient.state());
    Serial.println(")");
  }
}

// ==================== OTA ====================
void setupOTA() {
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setPassword("winefridge2025");
  ArduinoOTA.setHostname(DRAWER_ID);

  ArduinoOTA.onStart([]() {
    otaInProgress = true;
    Serial.println("\n[OTA] Starting...");
    // Desactivar WDT para que no reinicie durante la programación
    esp_task_wdt_deinit();
    // desconectar MQTT y apagar luces - minimizamos interferencias
    if (mqttClient.connected()) mqttClient.disconnect();
    strip.clear();
    strip.show();
    // apagar COB
    ledcWrite(COB_WARM_PIN, 0);
    ledcWrite(COB_COOL_PIN, 0);
    // reducir actividad WiFi/CPU durante OTA
    WiFi.setSleep(true);
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\n[OTA] Complete!");
    // Restaurar WiFi sleep off para operación normal
    WiFi.setSleep(false);
    otaInProgress = false;
    // Re-inicializamos WDT para operación normal
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = WATCHDOG_TIMEOUT * 1000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
    esp_task_wdt_add(NULL);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    static unsigned long lastReport = 0;
    if (millis() - lastReport > 500) {
      uint8_t p = (uint8_t)((progress * 100UL) / total);
      Serial.printf("[OTA] Progress: %u%%\r", p);
      lastReport = millis();
    }
    // si perdemos WiFi, abortar proactivamente
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("\n[OTA] WiFi perdido, abortando...");
      delay(500);
      // Forzamos reboot para intentar recovery
      ESP.restart();
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

// ==================== MAIN LOOP ====================
void loop() {
  // Llamada única a ArduinoOTA.handle - lo primero en loop
  ArduinoOTA.handle();
  if (otaInProgress) {
    // No ejecutar nada que pueda bloquear (WDT está deinit)
    return;
  }

  // Solo resetear WDT cuando no estamos en OTA
  esp_task_wdt_reset();

  // Actualizaciones normales de LEDs sólo si no hay OTA
  updateBlinkingLEDs();

  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > 10000) {
    maintainConnections();
    lastConnectionCheck = millis();
  }

  if (mqttClient.connected()) mqttClient.loop();

  static unsigned long lastSensorCheck = 0;
  if (millis() - lastSensorCheck > 2000) {
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

// ==================== LED BLINKING ====================
void updateBlinkingLEDs() {
  // No ejecutar si OTA en progreso
  if (otaInProgress) return;

  unsigned long currentMillis = millis();
  for (int i = 0; i < 9; i++) {
    if (ledBlinkStates[i].enabled) {
      if (currentMillis - ledBlinkStates[i].lastToggle >= BLINK_INTERVAL) {
        ledBlinkStates[i].currentState = !ledBlinkStates[i].currentState;
        ledBlinkStates[i].lastToggle = currentMillis;

        uint8_t ledIndex = bottleToLed[i];
        if (ledIndex < NUM_LEDS) {
          if (ledBlinkStates[i].currentState) {
            uint32_t color = ledBlinkStates[i].color;
            uint8_t brightness = ledBlinkStates[i].brightness;
            uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
            uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
            uint8_t b = (color & 0xFF) * brightness / 255;
            strip.setPixelColor(ledIndex, strip.Color(r, g, b));
          } else {
            strip.setPixelColor(ledIndex, strip.Color(0, 0, 0));
          }
        }
      }
    }
  }
  // Mostrar todos los cambios de una sola vez
  strip.show();
}

// ==================== MAINTAIN CONNECTIONS ====================
void maintainConnections() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WIFI] Lost, reconnecting...");
    setupNetwork();
    if (WiFi.status() == WL_CONNECTED && !otaInProgress) {
      setupOTA();
    }
    return;
  }

  if (!mqttClient.connected()) {
    Serial.println("[MQTT] Lost, reconnecting...");
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

void checkPositionChanges() {
  static bool lastPositions[9] = {false};
  static unsigned long lastDebounce[9] = {0};
  static bool processingChange = false;

  if (processingChange) return;

  for (int i = 0; i < 9; i++) {
    bool currentState = (digitalRead(SWITCH_PINS[i]) == LOW);

    if (millis() - lastDebounce[i] < DEBOUNCE_TIME) continue;

    if (currentState != lastPositions[i]) {
      lastDebounce[i] = millis();
      processingChange = true;

      delay(DEBOUNCE_TIME);
      currentState = (digitalRead(SWITCH_PINS[i]) == LOW);

      if (currentState != lastPositions[i]) {
        lastPositions[i] = currentState;
        state.positions[i] = currentState;

        float weightChange = 0.0;

        if (currentState) {
          delay(WEIGHT_STABILIZE_TIME);
          float newWeight = readTotalWeight();
          weightChange = newWeight - state.lastTotalWeight;

          if (weightChange > 10.0) {
            state.individualWeights[i] = weightChange;
            state.lastTotalWeight = newWeight;

            if (state.expectedPosition > 0 && (i + 1) != state.expectedPosition) {
              Serial.printf("[ERROR] Wrong position! Expected %d, got %d\n", state.expectedPosition, i + 1);
              setPositionLED(i + 1, 0xFF0000, 100, false);
              sendWrongPlacement(i + 1, state.expectedPosition);
            } else {
              setPositionLED(i + 1, 0x444444, 50, false);
              Serial.printf("[PLACED] Position %d (%.1fg)\n", i + 1, weightChange);
              sendBottleEvent(i + 1, currentState, weightChange);
            }
          }
        } else {
          weightChange = state.individualWeights[i];
          state.individualWeights[i] = 0.0;
          delay(1000);
          state.lastTotalWeight = readTotalWeight();
          setPositionLED(i + 1, 0x000000, 0, false);
          Serial.printf("[REMOVED] Position %d (%.1fg)\n", i + 1, weightChange);
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
void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness, bool blink) {
  if (position < 1 || position > 9) return;

  uint8_t ledIndex = bottleToLed[position - 1];
  if (ledIndex >= NUM_LEDS) return;

  // Update blink state
  ledBlinkStates[position - 1].enabled = blink;
  ledBlinkStates[position - 1].color = color;
  ledBlinkStates[position - 1].brightness = brightness;
  ledBlinkStates[position - 1].lastToggle = millis();
  ledBlinkStates[position - 1].currentState = true;

  if (!blink) {
    // Solid color
    uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
    uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
    uint8_t b = (color & 0xFF) * brightness / 255;

    strip.setPixelColor(ledIndex, strip.Color(r, g, b));
    strip.show();
  }
  // If blink, updateBlinkingLEDs() will handle it
}

void setCOBLighting(uint16_t temperature, uint8_t brightness) {
  temperature = constrain(temperature, TEMP_MIN, TEMP_MAX);
  brightness = constrain(brightness, 0, 100);

  if (brightness == 0) {
    ledcWrite(COB_WARM_PIN, 0);
    ledcWrite(COB_COOL_PIN, 0);
    state.currentBrightness = 0;
    return;
  }

  float warmRatio = map(temperature, TEMP_MIN, TEMP_MAX, 100, 0) / 100.0;
  float coolRatio = 1.0 - warmRatio;

  uint8_t maxPWM = map(brightness, 0, 100, 0, 255);
  uint8_t warmPWM = maxPWM * warmRatio;
  uint8_t coolPWM = maxPWM * coolRatio;

  ledcWrite(COB_WARM_PIN, warmPWM);
  ledcWrite(COB_COOL_PIN, coolPWM);

  state.currentTemperature = temperature;
  state.currentBrightness = brightness;

  Serial.printf("[COB] %dK @ %d%%\n", temperature, brightness);
}

void allLightsOff() {
  // Apaga LEDs y COB
  strip.clear();
  strip.show();
  ledcWrite(COB_WARM_PIN, 0);
  ledcWrite(COB_COOL_PIN, 0);
  // Reset state flags
  for (int i = 0; i < 9; i++) {
    ledBlinkStates[i].enabled = false;
  }
  state.currentBrightness = 0;
}

// ==================== MQTT / JSON ====================
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

    // If empty array, turn off ALL LEDs and disable all blink states
    if (positions.size() == 0) {
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
      for (JsonObject pos : positions) {
        uint8_t position = pos["position"];
        String colorStr = pos["color"];
        uint8_t brightness = pos["brightness"];
        bool blink = pos["blink"] | false;  // Default false

        if (position >= 1 && position <= 9) {
          uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
          setPositionLED(position, color, brightness, blink);
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

  StaticJsonDocument<1024> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["ip"] = state.ipAddress;

  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = millis();
  data["wifi_rssi"] = WiFi.RSSI();
  data["ssid"] = state.connectedSSID;
  data["total_weight"] = round(state.lastTotalWeight * 10.0) / 10.0;
  data["temperature"] = state.ambientTemperature;
  data["humidity"] = state.ambientHumidity;
  data["cob_temp"] = state.currentTemperature;
  data["cob_brightness"] = state.currentBrightness;

  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    JsonObject pos = positions.createNestedObject();
    pos["position"] = i + 1;
    pos["occupied"] = state.positions[i];
    pos["weight"] = round(state.individualWeights[i] * 10.0) / 10.0;
  }

  String output;
  serializeJson(doc, output);
  mqttClient.publish(mqtt_topic_status, output.c_str());
}

void sendStartupMessage() {
  StaticJsonDocument<256> doc;
  doc["action"] = "startup";
  doc["source"] = DRAWER_ID;

  JsonObject data = doc.createNestedObject("data");
  data["firmware"] = FIRMWARE_VERSION;
  data["ip"] = state.ipAddress;
  data["ssid"] = state.connectedSSID;

  String output;
  serializeJson(doc, output);
  mqttClient.publish(mqtt_topic_status, output.c_str());
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

void sendWrongPlacement(uint8_t wrongPosition, uint8_t expectedPosition) {
  if (!mqttClient.connected()) return;

  StaticJsonDocument<256> doc;
  doc["action"] = "placement_error";
  doc["source"] = DRAWER_ID;

  JsonObject data = doc.createNestedObject("data");
  data["drawer"] = DRAWER_ID;
  data["position"] = wrongPosition;
  data["expected_position"] = expectedPosition;

  String output;
  serializeJson(doc, output);
  mqttClient.publish("winefridge/system/status", output.c_str());
}
