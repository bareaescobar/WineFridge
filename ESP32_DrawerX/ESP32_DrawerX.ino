#include <WiFi.h>
#include <PubSubClient.h>

// ==== CONFIG ====
#define WIFI_SSID "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD "sA77n4kXrss9k9fn377i"

#define MQTT_BROKER_IP "192.168.1.84"
#define MQTT_PORT 1883
#define MQTT_TOPIC "winefridge/test"

// ==== GLOBAL ====
WiFiClient espClient;
PubSubClient mqttClient(espClient);

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println("\n\n--- ESP32 MQTT TEST ---");

  // PATCH: Force station mode for WiFi6 routers
  WiFi.mode(WIFI_STA);

  // Connect WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to WiFi...");
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 30) {
    delay(500);
    Serial.print(".");
    retries++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println(" connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println(" ERROR: WiFi failed to connect!");
    return;  // Abort if no WiFi
  }

  // PATCH: disable WiFi sleep
  WiFi.setSleep(false);

  // Setup MQTT
  mqttClient.setServer(MQTT_BROKER_IP, MQTT_PORT);
  mqttClient.setSocketTimeout(10);
  mqttClient.setKeepAlive(60);

  connectMQTT();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi lost, reconnecting...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  if (!mqttClient.connected()) {
    connectMQTT();
  }

  mqttClient.loop();

  static unsigned long lastMsg = 0;
  if (millis() - lastMsg > 5000) {  // Every 5s
    lastMsg = millis();
    String msg = "Hello from ESP32!";
    Serial.print("Publishing: ");
    Serial.println(msg);
    mqttClient.publish(MQTT_TOPIC, msg.c_str());
  }
}

void connectMQTT() {
  Serial.println("\nConnecting to MQTT...");
  int attempt = 0;
  while (!mqttClient.connected() && attempt < 3) {
    String clientId = "ESP32Test-" + String(random(0xffff), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      Serial.println("MQTT Connected!");
      mqttClient.subscribe(MQTT_TOPIC);
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
