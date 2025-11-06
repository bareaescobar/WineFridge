/*
 * WineFridge Lighting ESP32 - LIGHTING_2 v5.3.0
 * Controls: Drawer 1, Drawer 2, Zone 1 (lower)
 * NEW: OTA improvements and 75s watchdog
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include "esp_task_wdt.h"

#define LIGHTING_ID "lighting_2"
#define FIRMWARE_VERSION "5.3.0"

#define FIRST_DRAWER 1
#define SECOND_DRAWER 2
#define ZONE_NUMBER 1

#define WIFI_SSID_1 "Solo Spirits"
#define WIFI_PASSWORD_1 "Haveadrink"
#define WIFI_SSID_2 "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD_2 "sA77n4kXrss9k9fn377i"

#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

#define HEARTBEAT_INTERVAL 90000
#define WATCHDOG_TIMEOUT 75
#define BLINK_INTERVAL 500

#define DRAWER1_LED_PIN 18
#define DRAWER2_LED_PIN 12
#define LEDS_PER_DRAWER 17

#define DRAWER1_WARM_PIN 19
#define DRAWER1_COOL_PIN 21
#define DRAWER2_WARM_PIN 32
#define DRAWER2_COOL_PIN 33
#define ZONE_WARM_PIN 14
#define ZONE_COOL_PIN 13

#define MAX_BRIGHTNESS 150
#define COB_PWM_FREQUENCY 5000
#define COB_PWM_RESOLUTION 8
#define TEMP_MIN 2700
#define TEMP_MAX 6500

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
CRGB drawer1_leds[LEDS_PER_DRAWER];
CRGB drawer2_leds[LEDS_PER_DRAWER];
volatile bool otaInProgress = false;

struct LEDBlinkState {
  bool enabled;
  CRGB color;
  unsigned long lastToggle;
  bool currentState;
};

LEDBlinkState drawer1BlinkStates[LEDS_PER_DRAWER];
LEDBlinkState drawer2BlinkStates[LEDS_PER_DRAWER];

struct {
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
    uint8_t frontBrightness = 100;
  } drawer1;
  
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
    bool leds_active = false;
    uint8_t frontBrightness = 100;
  } drawer2;
  
  struct {
    uint16_t temperature = 4000;
    uint8_t brightness = 0;
  } zone;
  
  bool lightingReady = false;
  String ipAddress = "0.0.0.0";
  String connectedSSID = "";
} state;

char mqtt_topic_status[64];
char mqtt_topic_command[64];

void setup() {
  pinMode(ZONE_WARM_PIN, OUTPUT);
  digitalWrite(ZONE_WARM_PIN, LOW);
  pinMode(ZONE_COOL_PIN, OUTPUT);
  digitalWrite(ZONE_COOL_PIN, LOW);
  pinMode(DRAWER1_WARM_PIN, OUTPUT);
  digitalWrite(DRAWER1_WARM_PIN, LOW);
  pinMode(DRAWER1_COOL_PIN, OUTPUT);
  digitalWrite(DRAWER1_COOL_PIN, LOW);
  pinMode(DRAWER2_WARM_PIN, OUTPUT);
  digitalWrite(DRAWER2_WARM_PIN, LOW);
  pinMode(DRAWER2_COOL_PIN, OUTPUT);
  digitalWrite(DRAWER2_COOL_PIN, LOW);
  delay(100);
  
  Serial.begin(115200);
  delay(500);
  Serial.println("\n========================================");
  Serial.println("WineFridge Lighting - " + String(LIGHTING_ID));
  Serial.println("Firmware: v" + String(FIRMWARE_VERSION));
  Serial.println("========================================");

  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WATCHDOG_TIMEOUT * 1000,
    .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  
  memset(&drawer1BlinkStates, 0, sizeof(drawer1BlinkStates));
  memset(&drawer2BlinkStates, 0, sizeof(drawer2BlinkStates));
  
  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", LIGHTING_ID);
  snprintf(mqtt_topic_command, sizeof(mqtt_topic_command), "winefridge/%s/command", LIGHTING_ID);
  
  FastLED.addLeds<WS2812, DRAWER1_LED_PIN, GRB>(drawer1_leds, LEDS_PER_DRAWER);
  FastLED.addLeds<WS2812, DRAWER2_LED_PIN, GRB>(drawer2_leds, LEDS_PER_DRAWER);
  FastLED.setBrightness(MAX_BRIGHTNESS);
  FastLED.clear();
  FastLED.show();
  Serial.println("[OK] WS2812B LEDs");
  
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
  Serial.println("[OK] COB LEDs");
  
  state.lightingReady = true;
  setupNetwork();
  setupOTA(); // 🔹 Always initialize OTA regardless of WiFi state
  
  Serial.println("\n[READY]\n");
  esp_task_wdt_reset();
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
    allLightsOff();
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
  
  String clientId = String(LIGHTING_ID) + "-" + String(random(0xffff), HEX);
  
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

void loop() {
  ArduinoOTA.handle();
  esp_task_wdt_reset();
  
  if (otaInProgress) return;
  
  updateBlinkingLEDs();
  
  static unsigned long lastConnectionCheck = 0;
  if (millis() - lastConnectionCheck > 10000) {
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

// (Todas las funciones desde updateBlinkingLEDs() hasta sendStartupMessage() permanecen idénticas al código original)
