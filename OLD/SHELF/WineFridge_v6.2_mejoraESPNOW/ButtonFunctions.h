// ==========================================================
//                BUTTON FUNCTIONS
// ==========================================================
#ifndef BUTTON_FUNCTIONS_H
#define BUTTON_FUNCTIONS_H

#include <Arduino.h>
#include <ezButton.h>
#include "Config.h"
#include "LEDFunctions.h"

extern ezButton* bottleButtons[BOTTLE_COUNT];

void updateAllButtons() {
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    bottleButtons[i]->loop();
  }
}

int waitForBottleRemoval() {
  Serial.println("Waiting for a bottle to be removed...");
  
  // Record initial button states
  bool initialState[BOTTLE_COUNT];
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    bottleButtons[i]->loop();
    initialState[i] = (bottleButtons[i]->getState() == LOW); // LOW means pressed
    
    // If a bottle is present, highlight its LED in blue
    if (initialState[i]) {
      highlightPosition(i + 1);
    }
  }
  
  while (true) {
    updateAllButtons();
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      bool currentState = (bottleButtons[i]->getState() == LOW); // LOW means pressed
      
      // If state changed from pressed to not pressed (bottle removed)
      if (initialState[i] && !currentState) {
        showSuccessFeedback(i + 1);
        return i;
      }
    }
    delay(50);
  }
  return -1;
}

int waitForBottlePlacement(int expectedPosition, const String& message) {
  Serial.println(message);
  
  // Highlight the expected position
  highlightPosition(expectedPosition);
  
  while (true) {
    updateAllButtons();
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      if (bottleButtons[i]->isPressed()) {
        int pressedPosition = i + 1;
        if (pressedPosition == expectedPosition) {
          Serial.println("Bottle placed correctly");
          showSuccessFeedback(expectedPosition);
          return i;
        } else {
          Serial.print("Error: Incorrect position. Expected: ");
          Serial.print(expectedPosition);
          Serial.print(", Got: ");
          Serial.println(pressedPosition);
          showErrorFeedback(pressedPosition);
          // Keep the expected position highlighted
          highlightPosition(expectedPosition);
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
  
  // Highlight the expected position
  highlightPosition(position);
  
  unsigned long startTime = millis();
  while (millis() - startTime < timeoutMs) {
    updateAllButtons();
    // Check if the correct button is pressed (position-1 because arrays are 0-indexed)
    if (bottleButtons[position-1]->isPressed()) {
      Serial.println("Bottle returned correctly");
      showSuccessFeedback(position);
      return true;
    }
    delay(50);
  }
  
  // Turn off the LED if timeout occurs
  setBottleLED(position, LED_COLOR_OFF);
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
    
    // Highlight all empty positions in blue
    if (!initialState[i]) {
      highlightPosition(i + 1);
    }
  }
  
  while (true) {
    updateAllButtons();
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      bool currentState = (bottleButtons[i]->getState() == LOW); // LOW means pressed
      
      // If state changed from not pressed to pressed
      if (!initialState[i] && currentState) {
        showSuccessFeedback(i + 1);
        
        // Turn off all other LEDs
        for (int j = 0; j < BOTTLE_COUNT; j++) {
          if (j != i) {
            setBottleLED(j + 1, LED_COLOR_OFF);
          }
        }
        
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