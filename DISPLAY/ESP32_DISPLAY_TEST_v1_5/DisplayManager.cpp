/**
 * DisplayManager.cpp
 * Implementation of display functions
 */

#include "DisplayManager.h"
#include "gfx_conf.h"
#include "config.h"
#include "BottleManager.h"
#include <WiFi.h>  // Add WiFi.h include here
#include <time.h>  // Add time.h for time functions

// Reference to external bottleManager object
extern BottleManager bottleManager;

// Constructor
DisplayManager::DisplayManager(struct_message* data) {
  incomingData = data;
  currentView = VIEW_MAIN;
  selectedBottle = -1;
  lastUpdateTime = 0;
  lastUpdateTimeString = "";
  showingUpdateIndicator = false;
  timeInitialized = false;
  processingMessage = false;
}

void DisplayManager::updateLastUpdateTime() {
  lastUpdateTime = millis();
  lastUpdateTimeString = getTimeString();
  
  // Show update indicator
  showUpdateIndicator();
}

// Generate a time string based on internal millis()
String DisplayManager::getTimeString() {
  unsigned long currentTime = millis();
  int seconds = (currentTime / 1000) % 60;
  int minutes = (currentTime / 60000) % 60;
  int hours = (currentTime / 3600000) % 24;
  
  char timeStr[20];
  sprintf(timeStr, "%02d:%02d:%02d", hours, minutes, seconds);
  return String(timeStr);
}

// Get current time from system time if available
String DisplayManager::getCurrentTime() const {
  if (!timeInitialized) {
    return "Time not set";
  }
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char timeStr[20];
    strftime(timeStr, sizeof(timeStr), "%H:%M:%S", &timeinfo);
    return String(timeStr);
  }
  return "??:??:??";
}

// Get current date from system time if available
String DisplayManager::getCurrentDate() const {
  if (!timeInitialized) {
    return "Date not set";
  }
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char dateStr[20];
    strftime(dateStr, sizeof(dateStr), "%d/%m/%Y", &timeinfo);
    return String(dateStr);
  }
  return "??/??/????";
}

// Update time display in current view
void DisplayManager::updateTimeDisplay() {
  if (currentView == VIEW_MAIN) {
    // Just redraw the header in main view to update time
    drawHeader("WINE RACK");
  } else if (currentView == VIEW_DETAIL && selectedBottle >= 0) {
    // In detail view, redraw the header with the bottle name
    BottlePosition bottlePos = bottleManager.getBottlePosition(selectedBottle);
    if (bottlePos.bottleIndex >= 0) {
      Bottle bottle = bottleManager.getBottle(bottlePos.bottleIndex);
      String title = bottle.name;
      if (title.length() > 20) {
        title = title.substring(0, 17) + "...";
      }
      drawHeader(title.c_str());
    } else {
      String headerText = "POSITION DETAILS " + String(selectedBottle + 1);
      drawHeader(headerText.c_str());
    }
  }
}

// Show a visual indicator for updates
void DisplayManager::showUpdateIndicator() {
  // Small indicator in bottom right corner
  tft.fillRect(screenWidth - 120, screenHeight - 30, 110, 25, TFT_DARKGREEN);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(SMALL_FONT);
  tft.setCursor(screenWidth - 115, screenHeight - 20);
  tft.print("Auto Update");
  
  showingUpdateIndicator = true;
  updateIndicatorTimeout = millis() + 3000; // Set timeout 3 seconds from now
}

// Hide update indicator after timeout
void DisplayManager::hideUpdateIndicator() {
  if (showingUpdateIndicator) {
    if (currentView == VIEW_MAIN) {
      // Just clear the indicator area in main view
      tft.fillRect(screenWidth - 120, screenHeight - 30, 110, 25, BACKGROUND);
    }
    showingUpdateIndicator = false;
  }
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
  
  // Display current time and date if available
  if (timeInitialized) {
    String currentTime = getCurrentTime();
    String currentDate = getCurrentDate();
    
    tft.setTextSize(SMALL_FONT);
    tft.setCursor(screenWidth - 190, 45);
    tft.print(currentDate);
    tft.print(" ");
    tft.print(currentTime);
  } else {
    // Display last update time if time not initialized
    if (lastUpdateTime > 0) {
      tft.setTextSize(SMALL_FONT);
      tft.setCursor(screenWidth - 190, 45);
      tft.print("Last update: ");
      tft.print(lastUpdateTimeString);
    }
  }
  
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
  
  // Last interaction - Modified to use same font size
  tft.setTextColor(TEXT_COLOR);
  tft.setCursor(infoX, infoY + 5 * lineHeight);
  tft.print("Last action: ");
  int lastActionLabelWidth = tft.textWidth("Last action: ");
  tft.setTextColor(HIGHLIGHT_COLOR);
  tft.setCursor(infoX + lastActionLabelWidth, infoY + 5 * lineHeight);
  tft.print(bottle.lastInteraction);
  
  // Barcode - Modified to be on the same line
  tft.setTextColor(TEXT_COLOR);
  tft.setTextSize(MEDIUM_FONT);
  tft.setCursor(infoX, infoY + 6 * lineHeight);
  tft.print("Barcode: ");
  int barcodeWidth = tft.textWidth("Barcode: ");
  tft.setTextColor(HIGHLIGHT_COLOR);
  tft.setCursor(infoX + barcodeWidth, infoY + 6 * lineHeight);
  tft.print(bottle.barcode);
  
  // Back button - moved up a bit to avoid overlap
  drawButton(screenWidth / 2 - 100, screenHeight - 100, 200, 60, "BACK", HEADER_COLOR);
}

// Show welcome screen with status message
void DisplayManager::showWelcomeScreen(const char* statusMessage) {
  tft.fillScreen(BACKGROUND);
  
  // Draw header
  tft.fillRect(0, 0, screenWidth, 60, HEADER_COLOR);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(LARGE_FONT);
  
  // Center title text "SMART SHELF DISPLAY"
  const char* titleText = "SMART SHELF DISPLAY";
  int16_t textWidth = tft.textWidth(titleText);
  tft.setCursor((screenWidth - textWidth) / 2, 20);
  tft.print(titleText);
  
  // Add separator line
  tft.fillRect(0, 60, screenWidth, 2, TFT_WHITE);
  
  // Show large version of title in center
  tft.setTextSize(LARGE_FONT + 1);
  textWidth = tft.textWidth(titleText) * 1.5; // Approximate width with larger font
  tft.setCursor((screenWidth - textWidth) / 2, screenHeight / 2 - 50);
  tft.print(titleText);
  
  // Show status message
  tft.setTextSize(MEDIUM_FONT);
  textWidth = tft.textWidth(statusMessage);
  tft.setCursor((screenWidth - textWidth) / 2, screenHeight / 2 + 50);
  tft.print(statusMessage);
  
  // Show MAC address
  tft.setTextSize(SMALL_FONT);
  String macAddress = "MAC: " + WiFi.macAddress();
  textWidth = tft.textWidth(macAddress.c_str());
  tft.setCursor((screenWidth - textWidth) / 2, screenHeight / 2 + 100);
  tft.print(macAddress);
  
  // Show time if available
  if (timeInitialized) {
    String currentTime = getCurrentTime();
    String currentDate = getCurrentDate();
    String timeText = currentDate + " " + currentTime;
    
    textWidth = tft.textWidth(timeText.c_str());
    tft.setCursor((screenWidth - textWidth) / 2, screenHeight / 2 + 130);
    tft.print(timeText);
  }
  
  // Add footer
  tft.fillRect(0, screenHeight - 30, screenWidth, 30, HEADER_COLOR);
  const char* footerText = "v1.0";
  textWidth = tft.textWidth(footerText);
  tft.setCursor((screenWidth - textWidth) / 2, screenHeight - 20);
  tft.print(footerText);
}

// Display status update as an overlay
void DisplayManager::displayStatusUpdate() {
  processingMessage = true;
  
  // Show a status message at the bottom of the screen
  tft.fillRect(50, screenHeight - 150, screenWidth - 100, 80, HEADER_COLOR);
  tft.drawRect(50, screenHeight - 150, screenWidth - 100, 80, TFT_WHITE);
  
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(MEDIUM_FONT);
  
  // Draw the status message
  drawWrappedText(incomingData->text, 70, screenHeight - 130, screenWidth - 140, MEDIUM_FONT, TFT_WHITE);
  
  processingMessage = false;
}

// Display error message as an overlay
void DisplayManager::displayErrorMessage() {
  processingMessage = true;
  
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
  
  processingMessage = false;
}

// Update display based on incoming data
void DisplayManager::updateDisplay(int messageType) {
  // Set processing flag
  processingMessage = true;
  
  // Update the timestamp whenever we receive any data
  updateLastUpdateTime();
  
  // If we receive a bottle database update (type 1) or bottle info (type 2)
  if (messageType == MSG_TYPE_BOTTLE_DB || messageType == MSG_TYPE_BOTTLE_INFO) {
    // If it's the first bottle_db message or a full update, refresh screen
    if (messageType == MSG_TYPE_BOTTLE_DB) {
      // Show the main bottle grid
      drawBottleGrid();
    } 
    // If it's just bottle info and we're in main view, update without full redraw
    else if (messageType == MSG_TYPE_BOTTLE_INFO && currentView == VIEW_MAIN) {
      // Redraw only the relevant bottle icons
      for (int i = 0; i < BOTTLE_COUNT; i++) {
        BottlePosition pos = bottleManager.getBottlePosition(i);
        
        // Convert the position index to display number (1-based)
        int displayNumber = i + 1;
        
        // Display number mapping: 2,4,6,8,1,3,5,7,9
        int displayOrderNumber;
        switch (displayNumber) {
          case 1: displayOrderNumber = 5; break;
          case 2: displayOrderNumber = 1; break;
          case 3: displayOrderNumber = 6; break;
          case 4: displayOrderNumber = 2; break;
          case 5: displayOrderNumber = 7; break;
          case 6: displayOrderNumber = 3; break;
          case 7: displayOrderNumber = 8; break;
          case 8: displayOrderNumber = 4; break;
          case 9: displayOrderNumber = 9; break;
          default: displayOrderNumber = displayNumber;
        }
        
        // Redraw the bottle with updated status
        drawBottleIcon(pos.x, pos.y, pos.status, displayOrderNumber - 1);
      }
      
      // Also redraw the header to update bottle count
      drawHeader("WINE RACK");
    }
    // If we're in detail view and the current bottle is updated, refresh
    else if (messageType == MSG_TYPE_BOTTLE_INFO && currentView == VIEW_DETAIL) {
      // Redraw detail view with fresh data
      drawBottleDetail(selectedBottle);
    }
    
    // Show update indicator
    showUpdateIndicator();
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
  
  // Clear processing flag
  processingMessage = false;
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
    if (isButtonPressed(screenWidth / 2 - 100, screenHeight - 100, 200, 60, touchX, touchY)) {
      Serial.println("BACK button pressed");
      drawBottleGrid();
    }
  }
}