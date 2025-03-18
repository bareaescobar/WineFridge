/**
 * DisplayManager.cpp
 * Implementation of display functions
 */

#include "DisplayManager.h"
#include "gfx_conf.h"
#include "config.h"
#include "BottleManager.h"
#include <WiFi.h>  // Add WiFi.h include here

// Reference to external bottleManager object
extern BottleManager bottleManager;

// Constructor
DisplayManager::DisplayManager(struct_message* data) {
  incomingData = data;
  currentView = VIEW_MAIN;
  selectedBottle = -1;
}
// Draw the header section
void DisplayManager::drawHeader(const char* title) {
  tft.fillRect(0, 0, screenWidth, 60, HEADER_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(LARGE_FONT);
  tft.setCursor(20, 15);
  tft.print(title);
  
  // Display bottle count
  tft.setTextSize(MEDIUM_FONT);
  tft.setCursor(screenWidth - 190, 22);
  tft.print("Bottles: ");
  tft.print(incomingData->bottleCount);
  tft.print("/");
  tft.print(BOTTLE_COUNT);
  
  // Add a separator line
  tft.fillRect(0, 60, screenWidth, 2, TFT_WHITE);
}

// Draw a bottle icon with color based on its status
void DisplayManager::drawBottleIcon(int x, int y, int status, int index) {
  uint16_t bottleColor;
  
  // Select color based on status
  switch (status) {
    case BOTTLE_PRESENT:
      bottleColor = WINE_COLOR;
      break;
    case BOTTLE_WAITING:
      bottleColor = WAITING_COLOR;
      break;
    case BOTTLE_EMPTY:
      bottleColor = EMPTY_COLOR;
      break;
    default:
      bottleColor = EMPTY_COLOR;
      break;
  }
  
  // Bottle dimensions
  int bottleWidth = 40;     // Reduced from 80 to 40
  int bottleHeight = 75;    // Reduced from 150 to 75
  int neckWidth = 20;       // Reduced from 40 to 20
  int neckHeight = 13;      // Reduced from 25 to approximately half
  
  // Draw bottle body
  tft.fillRoundRect(x - (bottleWidth / 2), y - (bottleHeight / 2), bottleWidth, bottleHeight, 8, bottleColor);
  tft.drawRoundRect(x - (bottleWidth / 2), y - (bottleHeight / 2), bottleWidth, bottleHeight, 8, TFT_WHITE);
  
  // Draw bottle neck
  tft.fillRoundRect(x - (neckWidth / 2), y - (bottleHeight / 2) - neckHeight, neckWidth, neckHeight, 4, bottleColor);
  tft.drawRoundRect(x - (neckWidth / 2), y - (bottleHeight / 2) - neckHeight, neckWidth, neckHeight, 4, TFT_WHITE);
  
  // Show position number
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(SMALL_FONT);
  
  // Center text position - use the provided index directly (already mapped)
  String posText = String(index + 1);
  int16_t textWidth = tft.textWidth(posText);
  tft.setCursor(x - (textWidth / 2), y - 5);
  tft.print(posText);
  
  // Show status indicator if not empty
  if (status == BOTTLE_PRESENT || status == BOTTLE_WAITING) {
    tft.setTextSize(SMALL_FONT);
    String statusText = (status == BOTTLE_PRESENT) ? "IN" : "OUT";
    textWidth = tft.textWidth(statusText);
    tft.setCursor(x - (textWidth / 2), y + 15);
    tft.print(statusText);
  }
}

// Draw the main bottle grid
void DisplayManager::drawBottleGrid() {
  currentView = VIEW_MAIN;
  
  // Clear screen and draw header
  tft.fillScreen(BACKGROUND);
  drawHeader("WINE RACK");
  
  // Define the new order array: 2,4,6,8,1,3,5,7,9
  int displayNumber[BOTTLE_COUNT] = {2, 4, 6, 8, 1, 3, 5, 7, 9};
  
  // Draw each bottle in its position
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    BottlePosition pos = bottleManager.getBottlePosition(i);
    // Pass the display number instead of index for showing on bottle
    drawBottleIcon(pos.x, pos.y, pos.status, displayNumber[i]-1);
  }
  
  // Draw legend
  int legendY = screenHeight - 80;
  
  // Present bottle - 50% smaller
  tft.fillRoundRect(50, legendY, 20, 30, 4, WINE_COLOR);
  tft.drawRoundRect(50, legendY, 20, 30, 4, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(SMALL_FONT);
  tft.setCursor(80, legendY + 10);
  tft.print("Bottle present");
  
  // Waiting bottle - 50% smaller
  tft.fillRoundRect(280, legendY, 20, 30, 4, WAITING_COLOR);
  tft.drawRoundRect(280, legendY, 20, 30, 4, TFT_WHITE);
  tft.setCursor(310, legendY + 10);
  tft.print("Bottle out");
  
  // Empty position - 50% smaller
  tft.fillRoundRect(510, legendY, 20, 30, 4, EMPTY_COLOR);
  tft.drawRoundRect(510, legendY, 20, 30, 4, TFT_WHITE);
  tft.setCursor(540, legendY + 10);
  tft.print("Empty slot");
  
  // Instructions
  tft.setTextSize(SMALL_FONT);
  tft.setCursor(screenWidth / 2 - 150, screenHeight - 20);
  tft.print("Touch a bottle to see details");
}

// Draw the detail of a specific bottle
void DisplayManager::drawBottleDetail(int index) {
  currentView = VIEW_DETAIL;
  selectedBottle = index;
  
  BottlePosition bottlePos = bottleManager.getBottlePosition(index);
  
  // If the bottle index is invalid or there's no information
  if (bottlePos.bottleIndex < 0) {
    // Empty slot, show basic screen
    tft.fillScreen(BACKGROUND);
    String headerText = "POSITION DETAILS " + String(index + 1);
    drawHeader(headerText.c_str());
    
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(MEDIUM_FONT);
    tft.setCursor(screenWidth / 2 - 100, screenHeight / 2 - 20);
    tft.print("Empty slot");
    
    // Back button
    drawButton(screenWidth / 2 - 100, screenHeight - 80, 200, 60, "BACK", HEADER_COLOR);
    return;
  }
  
  // Get bottle information
  int bottleIdx = bottlePos.bottleIndex;
  Bottle bottle = bottleManager.getBottle(bottleIdx);
  
  // Clear screen and draw header
  tft.fillScreen(BACKGROUND);
  
  // Limit title to a reasonable size
  String title = bottle.name;
  if (title.length() > 20) {
    title = title.substring(0, 17) + "...";
  }
  drawHeader(title.c_str());
  
  // Draw large bottle icon on the left (50% smaller)
  int bottleX = 150;
  int bottleY = 240;
  int bottleWidth = 70;      // Reduced from 140 to 70
  int bottleHeight = 125;    // Reduced from 250 to 125
  int neckWidth = 30;        // Reduced from 60 to 30
  int neckHeight = 20;       // Reduced from 40 to 20
  
  uint16_t bottleColor = bottle.inFridge ? WINE_COLOR : WAITING_COLOR;
  
  // Draw bottle body
  tft.fillRoundRect(bottleX - (bottleWidth / 2), bottleY - (bottleHeight / 2), bottleWidth, bottleHeight, 10, bottleColor);
  tft.drawRoundRect(bottleX - (bottleWidth / 2), bottleY - (bottleHeight / 2), bottleWidth, bottleHeight, 10, TFT_WHITE);
  
  // Draw bottle neck
  tft.fillRoundRect(bottleX - (neckWidth / 2), bottleY - (bottleHeight / 2) - neckHeight, neckWidth, neckHeight, 5, bottleColor);
  tft.drawRoundRect(bottleX - (neckWidth / 2), bottleY - (bottleHeight / 2) - neckHeight, neckWidth, neckHeight, 5, TFT_WHITE);
  
  // Show position
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(MEDIUM_FONT);
  tft.setCursor(bottleX - 10, bottleY - 15);
  tft.print(bottle.position);
  
  // Show information on the right side
  int infoX = 280;
  int infoY = 100;
  int lineHeight = 40;
  
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(MEDIUM_FONT);
  
  // Status
  tft.setCursor(infoX, infoY);
  tft.print("Status: ");
  if (bottle.inFridge) {
    tft.setTextColor(SUCCESS_COLOR);
    tft.print("In Rack");
  } else {
    tft.setTextColor(WAITING_COLOR);
    tft.print("Out of Rack");
  }
  
  // Type
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(infoX, infoY + lineHeight);
  tft.print("Type: ");
  tft.setTextColor(HIGHLIGHT_COLOR);
  tft.print(bottle.type);
  
  // Region
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(infoX, infoY + 2 * lineHeight);
  tft.print("Region: ");
  tft.setTextColor(HIGHLIGHT_COLOR);
  tft.print(bottle.region);
  
  // Vintage
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(infoX, infoY + 3 * lineHeight);
  tft.print("Vintage: ");
  tft.setTextColor(HIGHLIGHT_COLOR);
  tft.print(bottle.vintage);
  
  // Weight
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(infoX, infoY + 4 * lineHeight);
  tft.print("Weight: ");
  tft.setTextColor(HIGHLIGHT_COLOR);
  tft.print(bottle.weight, 1);
  tft.print("g");
  
  // Last interaction - Modified to be on same line and smaller font
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(infoX, infoY + 5 * lineHeight);
  tft.print("Last action: ");
  int lastActionLabelWidth = tft.textWidth("Last action: ");
  
  // Use smaller font for the date/time
  tft.setTextSize(SMALL_FONT);
  tft.setTextColor(HIGHLIGHT_COLOR);
  // Position text after the label on the same line
  tft.setCursor(infoX + lastActionLabelWidth, infoY + 5 * lineHeight + 5); // +5 to align vertically
  tft.print(bottle.lastInteraction);
  
  // Barcode - position moved up slightly
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(MEDIUM_FONT);
  tft.setCursor(infoX, infoY + 6 * lineHeight);
  tft.print("Barcode: ");
  tft.setTextSize(SMALL_FONT);
  tft.setCursor(infoX, infoY + 6.5 * lineHeight);
  tft.print(bottle.barcode);
  
  // Back button - moved up a bit to avoid overlap
  drawButton(screenWidth / 2 - 100, screenHeight - 100, 200, 60, "BACK", HEADER_COLOR);
}

// Show welcome screen while waiting for connection
void DisplayManager::showWelcomeScreen() {
  tft.fillScreen(BACKGROUND);
  
  // Draw header
  tft.fillRect(0, 0, screenWidth, 60, HEADER_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(LARGE_FONT);
  tft.setCursor(20, 15);
  tft.print("WINE RACK");
  
  // Add a separator line
  tft.fillRect(0, 60, screenWidth, 2, TFT_WHITE);
  
  // Draw wine bottle icons
  for (int i = 0; i < 5; i++) {
    int xPos = 75 + (i * 150);
    tft.fillRoundRect(xPos, 120, 100, 220, 15, WINE_COLOR);
    tft.fillRoundRect(xPos + 30, 100, 40, 30, 8, WINE_COLOR);
    tft.drawRoundRect(xPos, 120, 100, 220, 15, TFT_WHITE);
    tft.drawRoundRect(xPos + 30, 100, 40, 30, 8, TFT_WHITE);
  }
  
  // Display welcome message
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(MEDIUM_FONT);
  tft.setCursor(160, 360);
  tft.print("Waiting for Wine Rack connection...");
  
  // Display MAC address for pairing
  tft.setTextSize(SMALL_FONT);
  tft.setCursor(250, 400);
  tft.print("MAC Address: ");
  tft.print(WiFi.macAddress());
  
  // Add footer
  tft.fillRect(0, screenHeight - 30, screenWidth, 30, HEADER_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(SMALL_FONT);
  tft.setCursor(screenWidth / 2 - 130, screenHeight - 20);
  tft.print("Wine Rack Display Controller v1.0");
}

// Display status update as an overlay
void DisplayManager::displayStatusUpdate() {
  // Show a status message at the bottom of the screen
  tft.fillRect(50, screenHeight - 150, screenWidth - 100, 80, HEADER_COLOR);
  tft.drawRect(50, screenHeight - 150, screenWidth - 100, 80, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(MEDIUM_FONT);
  
  // Draw the status message
  drawWrappedText(incomingData->text, 70, screenHeight - 130, screenWidth - 140, MEDIUM_FONT, TFT_WHITE);
}

// Display error message as an overlay
void DisplayManager::displayErrorMessage() {
  // Show a prominent error message
  tft.fillRect(100, 160, screenWidth - 200, 160, ERROR_COLOR);
  tft.drawRect(100, 160, screenWidth - 200, 160, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(MEDIUM_FONT);
  
  // Show error title
  tft.setCursor(120, 180);
  tft.print("ERROR");
  
  // Draw separator line
  tft.drawLine(120, 210, screenWidth - 120, 210, TFT_WHITE);
  
  // Draw error message
  drawWrappedText(incomingData->text, 120, 230, screenWidth - 240, MEDIUM_FONT, TFT_WHITE);
}

// Update display based on incoming data
void DisplayManager::updateDisplay(int messageType) {
  // If we receive data for the first time, switch to the main view
  if (messageType == MSG_TYPE_BOTTLE_DB) {
    // Show the main bottle grid
    drawBottleGrid();
  }
  else if (messageType == MSG_TYPE_ERROR) {
    // Show error message as an overlay
    displayErrorMessage();
    
    // After 5 seconds, return to the previous view
    delay(5000);
    
    if (currentView == VIEW_MAIN) {
      drawBottleGrid();
    } else if (currentView == VIEW_DETAIL && selectedBottle >= 0) {
      drawBottleDetail(selectedBottle);
    }
  }
  else if (messageType == MSG_TYPE_STATUS) {
    // Show status message as an overlay
    displayStatusUpdate();
    
    // After 3 seconds, return to the previous view
    delay(3000);
    
    if (currentView == VIEW_MAIN) {
      drawBottleGrid();
    } else if (currentView == VIEW_DETAIL && selectedBottle >= 0) {
      drawBottleDetail(selectedBottle);
    }
  }
}

// Function to draw text with word wrap
void DisplayManager::drawWrappedText(const char* text, int x, int y, int maxWidth, uint8_t font, uint16_t color) {
  tft.setTextColor(color);
  tft.setTextSize(font);
  
  int startX = x;
  int currentY = y;
  
  String word = "";
  String line = "";
  
  for (size_t i = 0; i < strlen(text); i++) {
    char c = text[i];
    
    if (c == '\n') {
      // Draw the current line and move to the next line
      tft.setCursor(startX, currentY);
      tft.print(line);
      line = "";
      currentY += tft.fontHeight() * font;
      continue;
    }
    
    if (c == ' ' || c == '\t') {
      // Check if adding this word would exceed maxWidth
      int lineWidth = tft.textWidth(line + word + " ");
      if (lineWidth > maxWidth && line.length() > 0) {
        // Draw the current line and start a new one
        tft.setCursor(startX, currentY);
        tft.print(line);
        line = word + " ";
        currentY += tft.fontHeight() * font;
      } else {
        line += word + " ";
      }
      word = "";
    } else {
      word += c;
    }
  }
  
  // Add the last word to the line
  if (word.length() > 0) {
    int lineWidth = tft.textWidth(line + word);
    if (lineWidth > maxWidth && line.length() > 0) {
      // Draw the current line and start a new one with the last word
      tft.setCursor(startX, currentY);
      tft.print(line);
      currentY += tft.fontHeight() * font;
      line = word;
    } else {
      line += word;
    }
  }
  
  // Draw the final line
  if (line.length() > 0) {
    tft.setCursor(startX, currentY);
    tft.print(line);
  }
}

// Draw a button
void DisplayManager::drawButton(int x, int y, int w, int h, const char* text, uint16_t color) {
  tft.fillRoundRect(x, y, w, h, 10, color);
  tft.drawRoundRect(x, y, w, h, 10, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(MEDIUM_FONT);
  
  // Center text
  int16_t textWidth = tft.textWidth(text);
  int16_t textHeight = tft.fontHeight() * MEDIUM_FONT;
  tft.setCursor(x + (w - textWidth) / 2, y + (h - textHeight) / 2);
  tft.print(text);
}

// Check if a button was touched
bool DisplayManager::isButtonPressed(int x, int y, int w, int h, uint16_t touchX, uint16_t touchY) {
  return (touchX >= x && touchX <= x + w && touchY >= y && touchY <= y + h);
}

// Handle touch input
void DisplayManager::handleTouch(uint16_t touchX, uint16_t touchY) {
  if (currentView == VIEW_MAIN) {
    // Check if a bottle was touched
    int bottleIndex = bottleManager.getBottleTouched(touchX, touchY);
    if (bottleIndex >= 0) {
      Serial.print("Bottle touched: ");
      Serial.println(bottleIndex + 1);
      drawBottleDetail(bottleIndex);
    }
  } 
  else if (currentView == VIEW_DETAIL) {
    // Check if "BACK" button was pressed
    if (isButtonPressed(screenWidth / 2 - 100, screenHeight - 80, 200, 60, touchX, touchY)) {
      Serial.println("BACK button pressed");
      drawBottleGrid();
    }
  }
}