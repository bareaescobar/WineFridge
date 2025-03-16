// ==========================================================
//                BUTTON FUNCTIONS
// ==========================================================
#ifndef BUTTON_FUNCTIONS_H
#define BUTTON_FUNCTIONS_H

#include <Arduino.h>
#include <ezButton.h>
#include "Config.h"

extern ezButton* bottleButtons[BOTTLE_COUNT];

void updateAllButtons();  // Forward declaration

int waitForBottleRemoval() {
  Serial.println("Waiting for a bottle to be removed...");
  while (true) {
    updateAllButtons();
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      if (bottleButtons[i]->isReleased()) {
        return i;
      }
    }
    delay(50);
  }
  return -1;
}

int waitForBottlePlacement(int expectedPosition, const String& message) {
  Serial.println(message);
  while (true) {
    updateAllButtons();
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      if (bottleButtons[i]->isPressed()) {
        int pressedPosition = i + 1;
        if (pressedPosition == expectedPosition) {
          Serial.println("Bottle placed correctly");
          return i;
        } else {
          Serial.print("Error: Incorrect position. Expected: ");
          Serial.print(expectedPosition);
          Serial.print(", Got: ");
          Serial.println(pressedPosition);
        }
      }
    }
    delay(50);
  }
  return -1;
}

bool waitForBottleReturn(int position, unsigned long timeoutMs) {
  Serial.print("Waiting for bottle return to position ");
  Serial.print(position);
  Serial.println("...");
  
  unsigned long startTime = millis();
  while (millis() - startTime < timeoutMs) {
    updateAllButtons();
    // Check if the correct button is pressed (position-1 because arrays are 0-indexed)
    if (bottleButtons[position-1]->isPressed()) {
      Serial.println("Bottle returned correctly");
      return true;
    }
    delay(50);
  }
  
  Serial.println("Timeout: Bottle not returned in time");
  return false;
}

// Wait for a bottle to be placed in any position
int waitForAnyBottlePlacement() {
  Serial.println("Waiting for bottle placement in any position...");
  
  // Record initial button states
  bool initialState[BOTTLE_COUNT];
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    bottleButtons[i]->loop();
    initialState[i] = (bottleButtons[i]->getState() == LOW); // LOW means pressed
  }
  
  while (true) {
    updateAllButtons();
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      bool currentState = (bottleButtons[i]->getState() == LOW); // LOW means pressed
      
      // If state changed from not pressed to pressed
      if (!initialState[i] && currentState) {
        Serial.print("Bottle placed at position ");
        Serial.println(i + 1);
        return i; // Return the 0-based index
      }
    }
    delay(50);
  }
  return -1;
}

#endif // BUTTON_FUNCTIONS_H