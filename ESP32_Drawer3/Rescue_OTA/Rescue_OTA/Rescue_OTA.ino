#include <WiFi.h>
#include <ArduinoOTA.h>

#define WIFI_SSID_2 "MOVISTAR-WIFI6-65F8"
#define WIFI_PASSWORD_2 "sA77n4kXrss9k9fn377i"

void setup() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);                      // Mantén WiFi activo
  WiFi.begin(WIFI_SSID_2, WIFI_PASSWORD_2);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(100);
  }

  if (WiFi.status() != WL_CONNECTED) ESP.restart();

  ArduinoOTA.setHostname("Rescue_OTA");
  ArduinoOTA.setPassword("WineFridge");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  delay(1);   // Evita watchdog
}
