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
#include <FastLED.h>
#include <esp_now.h>
#include "Config.h"
#include "LEDFunctions.h"
#include "ESPNowFunctions.h"

extern Adafruit_SHT31 sht31;
extern HX711 scale;
extern M5UnitQRCodeI2C qrcode;
extern ezButton* bottleButtons[BOTTLE_COUNT];
extern CRGB leds[NUM_LEDS];
extern float lastWeight;

// Modificada para solo mostrar información, sin conectarse a WiFi
void setupWiFi() {
  Serial.println("WiFi setup for ESP-NOW (station mode only)");
  // No intentamos conectarnos a WiFi aquí, ya que interfiere con ESP-NOW
}

void setupTime() {
  // Intentar configurar el tiempo aunque no estemos conectados
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time - No WiFi connection");
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

  lastWeight = 0.0;

  scale.power_up();
  delay(500);
  lastWeight = scale.get_units(10);
  scale.power_down();
  
  // Initialize M5 and QR scanner
  M5.begin();
  qrcode.begin(&Wire, UNIT_QRCODE_ADDR, 21, 22, 100000U);
  qrcode.setTriggerMode(AUTO_SCAN_MODE);
}

// Esta función ya no se usa en esta versión. La inicialización
// ahora se hace directamente en setup() en orden específico
void setupHardware() {
  // No utilizamos esta función en esta versión
  // Ver función setup() en el archivo principal
}

#endif // HARDWARE_SETUP_H