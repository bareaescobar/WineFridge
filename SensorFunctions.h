// ==========================================================
//                SENSOR FUNCTIONS
// ==========================================================
#ifndef SENSOR_FUNCTIONS_H
#define SENSOR_FUNCTIONS_H

#include <Arduino.h>
#include <Adafruit_SHT31.h>
#include <HX711.h>
#include "M5UnitQRCode.h"
#include "ButtonFunctions.h"

extern Adafruit_SHT31 sht31;
extern HX711 scale;
extern M5UnitQRCodeI2C qrcode;

float measureWeight() {
  scale.power_up();
  delay(100);
  float weight = scale.get_units(10);
  Serial.print("Weight (g): ");
  Serial.println(weight, 2);
  scale.power_down();
  return weight;
}

String scanBarcode() {
  qrcode.setDecodeTrigger(1);
  String code = "";
  Serial.println("Waiting for barcode scan...");
  
  unsigned long startTime = millis();
  while (millis() - startTime < 30000) { // 30 second timeout
    if (qrcode.getDecodeReadyStatus() == 1) {
      uint8_t buffer[512] = {0};
      uint16_t length = qrcode.getDecodeLength();
      qrcode.getDecodeData(buffer, length);
      
      for (int i = 0; i < length; i++) {
        code += (char)buffer[i];
      }
      
      Serial.print("Scanned code: ");
      Serial.println(code);
      break;
    }
    delay(100);
  }
  
  qrcode.setDecodeTrigger(0);
  return code;
}

// Find an available position in the fridge
int findAvailablePosition() {
  // Update all button states
  updateAllButtons();
  
  // First check for empty positions (not pressed buttons)
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    // If button is not pressed (HIGH state for PULLUP), position is available
    if (bottleButtons[i]->getState() == HIGH) {
      return i + 1; // Return 1-based position
    }
  }
  
  // If all positions are physically occupied, find a logical position
  // that's not marked as occupied in the database
  extern struct Bottle bottles[]; // Forward declaration
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (!bottles[i].inFridge) {
      return i + 1; // Return 1-based position
    }
  }
  
  // All positions are occupied
  return -1;
}

// Check if a position is physically occupied
bool isPositionOccupied(int position) {
  // Update button state
  bottleButtons[position-1]->loop();
  
  // If button is pressed (LOW for PULLUP), position is occupied
  return (bottleButtons[position-1]->getState() == LOW);
}

#endif // SENSOR_FUNCTIONS_H