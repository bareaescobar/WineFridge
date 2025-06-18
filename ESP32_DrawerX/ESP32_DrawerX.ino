// === WineFridge Drawer ESP32 v2.1 ===
// Compatible with RPI backend (server.js, mqtt_handler.py)
// Fixes publish bug with String topic

#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

#define DRAWER_ID "drawer_1"

#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"

#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883

#define HEARTBEAT_INTERVAL 30000
#define SENSOR_READ_INTERVAL 1000
#define DEBOUNCE_TIME 50

#define WS2812_DATA_PIN 13
#define NUM_LEDS 9

const uint8_t SWITCH_PINS[9] = {
  4, 5, 23, 14, 27, 26, 12, 33, 32
};

#define COLOR_AVAILABLE 0x0000FF  // Blue
#define COLOR_SUCCESS   0x00FF00  // Green
#define COLOR_ERROR     0xFF0000  // Red
#define COLOR_OFF       0x000000  // Off

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_NeoPixel strip(NUM_LEDS, WS2812_DATA_PIN, NEO_GRB + NEO_KHZ800);

struct DrawerState {
  bool positions[9];
  unsigned long lastHeartbeat;
  unsigned long uptime;
  bool systemReady;
} drawerState;

char mqtt_topic_status[64];

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n--- ESP32 WineFridge Drawer v2.1 ---");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setSleep(false);
  Serial.print("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqttClient.setCallback(onMQTTMessage);
  mqttClient.setSocketTimeout(10);
  mqttClient.setKeepAlive(60);
  connectMQTT();

  snprintf(mqtt_topic_status, sizeof(mqtt_topic_status), "winefridge/%s/status", DRAWER_ID);

  for (int i = 0; i < 9; i++) {
    pinMode(SWITCH_PINS[i], INPUT_PULLUP);
    drawerState.positions[i] = false;
  }

  strip.begin();
  strip.clear();
  strip.show();
  testLEDs();

  readAllPositions();

  drawerState.systemReady = true;
  drawerState.lastHeartbeat = millis();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(2000);
    return;
  }

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  drawerState.uptime = millis();

  static unsigned long lastPositionCheck = 0;
  if (millis() - lastPositionCheck > SENSOR_READ_INTERVAL) {
    checkPositionChanges();
    lastPositionCheck = millis();
  }

  static unsigned long lastTestMsg = 0;
  if (millis() - lastTestMsg > 10000) {  // Cada 10s
      lastTestMsg = millis();
      mqttClient.publish(mqtt_topic_status, "TEST SMALL MSG");
      Serial.println("Published test small msg");
  }


  if (millis() - drawerState.lastHeartbeat > HEARTBEAT_INTERVAL) {
    sendHeartbeat();
    drawerState.lastHeartbeat = millis();
  }

  handleSerialCommands();
}

void connectMQTT() {
  Serial.println("\nConnecting to MQTT...");
  int attempt = 0;
  while (!mqttClient.connected() && attempt < 3) {
    String clientId = String(DRAWER_ID) + "-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT Connected!");
      String cmdTopic = "winefridge/" + String(DRAWER_ID) + "/command";
      mqttClient.subscribe(cmdTopic.c_str());
      Serial.print("Subscribed to: ");
      Serial.println(cmdTopic);
    } else {
      Serial.printf("Failed (rc=%d), retrying...\n", mqttClient.state());
      attempt++;
      delay(3000);
    }
  }

  if (!mqttClient.connected()) {
    Serial.println("ERROR: MQTT connection failed");
  }
}

void sendHeartbeat() {
  StaticJsonDocument<512> doc;
  doc["action"] = "heartbeat";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();

  JsonObject data = doc.createNestedObject("data");
  data["uptime"] = drawerState.uptime;
  data["wifi_rssi"] = WiFi.RSSI();

  JsonArray positions = data.createNestedArray("positions");
  for (int i = 0; i < 9; i++) {
    JsonObject pos = positions.createNestedObject();
    pos["position"] = i + 1;
    pos["occupied"] = drawerState.positions[i];
  }

  String output;
  serializeJson(doc, output);
  mqttClient.publish(mqtt_topic_status, output.c_str());
  Serial.println("Heartbeat sent");
}

void sendBottleEvent(uint8_t position, bool occupied) {
  StaticJsonDocument<256> doc;
  doc["action"] = "bottle_event";
  doc["source"] = DRAWER_ID;
  doc["timestamp"] = millis();

  JsonObject data = doc.createNestedObject("data");
  data["event"] = occupied ? "placed" : "removed";
  data["position"] = position;

  String output;
  serializeJson(doc, output);
  mqttClient.publish(mqtt_topic_status, output.c_str());

  Serial.printf("Bottle %s at position %d\n", occupied ? "PLACED" : "REMOVED", position);
}

void checkPositionChanges() {
  static bool lastPositions[9] = {false};
  for (int i = 0; i < 9; i++) {
    bool currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
    if (currentState != lastPositions[i]) {
      delay(DEBOUNCE_TIME);
      currentState = (digitalRead(SWITCH_PINS[i]) == LOW);
      if (currentState != lastPositions[i]) {
        drawerState.positions[i] = currentState;
        lastPositions[i] = currentState;

        sendBottleEvent(i + 1, currentState);

        if (currentState) {
          setPositionLED(i + 1, COLOR_AVAILABLE, 100);
        } else {
          setPositionLED(i + 1, COLOR_OFF, 0);
        }
      }
    }
  }
}

void readAllPositions() {
  for (int i = 0; i < 9; i++) {
    bool occupied = (digitalRead(SWITCH_PINS[i]) == LOW);
    drawerState.positions[i] = occupied;
    Serial.printf("Position %d: %s\n", i + 1, occupied ? "OCCUPIED" : "EMPTY");
  }
}

void setPositionLED(uint8_t position, uint32_t color, uint8_t brightness) {
  if (position < 1 || position > 9) return;
  uint8_t index = position - 1;
  uint8_t r = ((color >> 16) & 0xFF) * brightness / 255;
  uint8_t g = ((color >> 8) & 0xFF) * brightness / 255;
  uint8_t b = (color & 0xFF) * brightness / 255;
  strip.setPixelColor(index, r, g, b);
  strip.show();
}

void testLEDs() {
  for (int i = 1; i <= 9; i++) {
    setPositionLED(i, COLOR_AVAILABLE, 100);
    delay(100);
  }
  delay(500);
  strip.clear();
  strip.show();
}

void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  Serial.printf("MQTT message on topic: %s\n", topic);
  char message[length + 1];
  memcpy(message, payload, length);
  message[length] = '\0';
  Serial.println(message);

  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, message);
  if (error) {
    Serial.print("JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }

  handleCommand(doc);
}

void handleCommand(JsonDocument& doc) {
  String action = doc["action"];
  JsonObject data = doc["data"];
  Serial.printf("Handling command: %s\n", action.c_str());

  if (action == "set_leds") {
    JsonArray positions = data["positions"];
    for (JsonObject pos : positions) {
      uint8_t position = pos["position"];
      String colorStr = pos["color"];
      uint8_t brightness = pos["brightness"];
      uint32_t color = strtol(colorStr.c_str() + 1, NULL, 16);
      setPositionLED(position, color, brightness);
      Serial.printf("Set LED pos %d -> %s at %d%%\n", position, colorStr.c_str(), brightness);
    }
  } else if (action == "read_sensors") {
    sendHeartbeat();
  } else if (action == "test_leds") {
    testLEDs();
  } else {
    Serial.printf("Unknown command: %s\n", action.c_str());
  }
}

void handleSerialCommands() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toUpperCase();

    if (cmd == "STATUS") {
      printStatus();
    } else if (cmd == "HEARTBEAT") {
      sendHeartbeat();
    } else if (cmd == "TEST") {
      testLEDs();
    } else if (cmd == "POSITIONS") {
      readAllPositions();
    } else if (cmd == "PING") {
      connectMQTT();
    } else if (cmd.startsWith("LED")) {
      handleLEDCommand(cmd);
    } else if (cmd == "HELP") {
      printHelp();
    }
  }
}

void handleLEDCommand(String cmd) {
  int s1 = cmd.indexOf(' ');
  int s2 = cmd.indexOf(' ', s1 + 1);
  if (s1 != -1 && s2 != -1) {
    int pos = cmd.substring(s1 + 1, s2).toInt();
    String color = cmd.substring(s2 + 1);
    if (pos >= 1 && pos <= 9) {
      if (color == "BLUE") {
        setPositionLED(pos, COLOR_AVAILABLE, 100);
      } else if (color == "RED") {
        setPositionLED(pos, COLOR_ERROR, 100);
      } else if (color == "GREEN") {
        setPositionLED(pos, COLOR_SUCCESS, 100);
      } else if (color == "OFF") {
        setPositionLED(pos, COLOR_OFF, 0);
      }
      Serial.printf("Set LED pos %d to %s\n", pos, color.c_str());
    }
  }
}

void printStatus() {
  Serial.println("\n=== DRAWER STATUS ===");
  Serial.printf("ID: %s\n", DRAWER_ID);
  Serial.printf("WiFi: %s (RSSI %d dBm)\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.printf("MQTT: %s\n", mqttClient.connected() ? "Connected" : "Disconnected");
  Serial.printf("Uptime: %lu sec\n", drawerState.uptime / 1000);
  Serial.println("Positions:");
  for (int i = 0; i < 9; i++) {
    Serial.printf("  %d: %s\n", i + 1, drawerState.positions[i] ? "OCCUPIED" : "EMPTY");
  }
  Serial.println("====================\n");
}

void printHelp() {
  Serial.println("\n=== SERIAL COMMANDS ===");
  Serial.println("STATUS     - Show system status");
  Serial.println("HEARTBEAT  - Send heartbeat");
  Serial.println("TEST       - Test LEDs");
  Serial.println("POSITIONS  - Read switches");
  Serial.println("PING       - Reconnect MQTT");
  Serial.println("LED 3 BLUE - Set LED");
  Serial.println("HELP       - Show help");
  Serial.println("========================\n");
}