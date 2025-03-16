// ==========================================================
//                DISPLAY FUNCTIONS
// ==========================================================
#ifndef DISPLAY_FUNCTIONS_H
#define DISPLAY_FUNCTIONS_H

#include <Arduino.h>
#include <LiquidCrystal.h>
#include "Config.h"

// LCD pin configuration
const int rs = 8, en = 9, d4 = 10, d5 = 11, d6 = 12, d7 = 13;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Initialize the LCD display
void initializeDisplay() {
  lcd.begin(16, 2);  // 16 columns, 2 rows
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Wine Fridge");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1000);
}

// Clear the display
void clearDisplay() {
  lcd.clear();
}

// Display text on a specific line (0 = top, 1 = bottom)
void displayTextLine(int line, String text) {
  lcd.setCursor(0, line % 2);  // Ensure line is either 0 or 1
  
  // If text is longer than display width, truncate with ellipsis
  if (text.length() > 16) {
    lcd.print(text.substring(0, 13) + "...");
  } else {
    lcd.print(text);
    
    // Clear the rest of the line
    for (int i = text.length(); i < 16; i++) {
      lcd.print(" ");
    }
  }
}

// Display a centered message on the top line
void displayCenteredText(String text) {
  lcd.setCursor(0, 0);
  
  // Calculate padding for centering
  int padding = (16 - text.length()) / 2;
  
  // If text is too long, truncate with ellipsis
  if (text.length() > 16) {
    lcd.print(text.substring(0, 13) + "...");
  } else {
    // Print spaces for padding
    for (int i = 0; i < padding; i++) {
      lcd.print(" ");
    }
    
    // Print the text
    lcd.print(text);
    
    // Clear the rest of the line
    for (int i = padding + text.length(); i < 16; i++) {
      lcd.print(" ");
    }
  }
}

// Display a scrolling message on the bottom line
void displayScrollingText(String text) {
  // If text fits on screen, just display it
  if (text.length() <= 16) {
    displayTextLine(1, text);
    return;
  }
  
  // Add spaces at the beginning and end for scrolling
  text = "    " + text + "    ";
  
  // Scroll the text
  for (int i = 0; i < text.length() - 16; i++) {
    lcd.setCursor(0, 1);
    lcd.print(text.substring(i, i + 16));
    delay(300);  // Scroll speed
  }
}

// Display a progress bar on the bottom line
void displayProgressBar(int percentage) {
  lcd.setCursor(0, 1);
  
  // Constrain percentage between 0 and 100
  percentage = constrain(percentage, 0, 100);
  
  // Calculate number of filled blocks (each block = 6.25%)
  int filledBlocks = (percentage * 16) / 100;
  
  // Display the progress bar
  for (int i = 0; i < 16; i++) {
    if (i < filledBlocks) {
      lcd.write(0xFF);  // Filled block character
    } else {
      lcd.print(" ");
    }
  }
}

// Display a welcome message when system starts
void displayWelcomeMessage() {
  clearDisplay();
  displayCenteredText("Wine Fridge");
  displayTextLine(1, "System Ready");
  delay(2000);
}

// Display a bottle status message
void displayBottleStatus(String bottleName, bool inFridge) {
  clearDisplay();
  
  // Truncate bottle name if needed
  if (bottleName.length() > 16) {
    bottleName = bottleName.substring(0, 13) + "...";
  }
  
  displayTextLine(0, bottleName);
  
  if (inFridge) {
    displayTextLine(1, "Status: IN");
  } else {
    displayTextLine(1, "Status: REMOVED");
  }
}

// Display weight information
void displayWeight(float weight) {
  clearDisplay();
  displayTextLine(0, "Weight:");
  displayTextLine(1, String(weight, 1) + "g");
}

// Display a message that requires confirmation
void displayConfirmationMessage(String message) {
  clearDisplay();
  
  // If message is short enough, display on one line
  if (message.length() <= 16) {
    displayTextLine(0, message);
    displayTextLine(1, "Press SELECT");
  } 
  // Otherwise split across both lines
  else if (message.length() <= 32) {
    displayTextLine(0, message.substring(0, 16));
    displayTextLine(1, message.substring(16));
  }
  // If too long, truncate with ellipsis
  else {
    displayTextLine(0, message.substring(0, 13) + "...");
    displayTextLine(1, "Press SELECT");
  }
}

// Display an error message
void displayErrorMessage(String errorMessage) {
  clearDisplay();
  displayTextLine(0, "ERROR:");
  
  // If error message is short, display on second line
  if (errorMessage.length() <= 16) {
    displayTextLine(1, errorMessage);
  }
  // Otherwise scroll it
  else {
    displayScrollingText(errorMessage);
  }
}

#endif // DISPLAY_FUNCTIONS_H