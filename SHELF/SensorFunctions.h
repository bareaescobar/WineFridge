// ==========================================================
//                SENSOR FUNCTIONS
// ==========================================================
#ifndef SENSOR_FUNCTIONS_H
#define SENSOR_FUNCTIONS_H

#include <Arduino.h>
#include <Adafruit_SHT31.h>
#include <HX711.h>
#include "M5UnitQRCode.h"
#include "Config.h"

// Forward declarations para evitar dependencias circulares
struct Bottle;  // Forward declaration de la estructura Bottle
extern Bottle bottles[];  // Declaración externa del array de botellas

#include "ButtonFunctions.h"

extern Adafruit_SHT31 sht31;
extern HX711 scale;
extern M5UnitQRCodeI2C qrcode;

float lastWeight = 0.0;

float measureWeight() {
  scale.power_up();
  delay(500);
  float currentWeight = scale.get_units(10);
  float weightDifference = currentWeight - lastWeight; // Calcula la diferencia
  
  Serial.print("Peso actual (g): ");
  Serial.println(currentWeight, 2);
  Serial.print("Diferencia de peso (g): ");
  Serial.println(weightDifference, 2);
  
  lastWeight = currentWeight; // Actualiza el último peso medido
  scale.power_down();
  
  return weightDifference; // Devuelve la diferencia de peso
}

void tareScale() {
  Serial.println("Realizando tara de la báscula...");
  
  // Asegurarse de que la báscula esté encendida
  scale.power_up();
  delay(500); // Dar tiempo a que se estabilice
  
  // Realizar la tara (establecer el cero)
  scale.tare(10);  // Usa 10 lecturas para un valor más estable
  
  // Actualizar el valor de lastWeight a 0
  lastWeight = 0.0;
  
  Serial.println("Tara completada. Báscula calibrada a cero.");
}

String scanBarcode() {
  String code = "";
  delay(500);
  qrcode.begin();
  qrcode.setDecodeTrigger(0); // First turn off to reset
  delay(100);
  qrcode.setDecodeTrigger(1); // Then turn on for new scan
  
  // Clear any previous scan data
  uint8_t clearBuffer[1] = {0};
  qrcode.getDecodeData(clearBuffer, 1);
  Serial.println("Waiting for barcode scan... (press 'qq' to cancel)");
  
  unsigned long startTime = millis();
  while (millis() - startTime < 30000) { // 30 second timeout
    // Check for QR code scan
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
    
    // Check if user wants to quit
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == 'q' || input == 'Q') {
        Serial.println("Scan cancelled by user.");
        code = ""; // Clear code to indicate cancellation
        break;
      }
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
  // Ya no necesitamos la declaración extern aquí porque la tenemos arriba
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