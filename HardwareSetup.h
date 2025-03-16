// ==========================================================
//              HARDWARE SETUP FUNCTIONS
// ==========================================================
#ifndef HARDWARE_SETUP_H
#define HARDWARE_SETUP_H

#include <Arduino.h>
#include <WiFi.h>
#include <ezButton.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <HX711.h>
#include <M5Unified.h>
#include "M5UnitQRCode.h"
#include <time.h>
#include "Config.h"

extern Adafruit_SHT31 sht31;
extern HX711 scale;
extern M5UnitQRCodeI2C qrcode;
extern ezButton* bottleButtons[BOTTLE_COUNT];

void setupWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 60) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
  } else {
    Serial.println("\nWiFi connection failed");
  }
}

void setupTime() {
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
  } else {
    Serial.println("Time synchronized");
  }
}

void setupButtons() {
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
    bottleButtons[i] = new ezButton(BUTTON_PINS[i]);
    bottleButtons[i]->setDebounceTime(100);
  }
}

void setupSensors() {
  // Initialize temperature sensor
  sht31.begin(0x44);
  
  // Initialize scale
  scale.begin(HX711_DT_PIN, HX711_SCK_PIN);
  scale.set_scale(94.308214f);
  scale.tare();
  
  // Initialize M5 and QR scanner
  M5.begin();
  qrcode.begin(&Wire, UNIT_QRCODE_ADDR, 21, 22, 100000U);
  qrcode.setTriggerMode(AUTO_SCAN_MODE);
}

void updateAllButtons() {
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    bottleButtons[i]->loop();
  }
}

#endif // HARDWARE_SETUP_H