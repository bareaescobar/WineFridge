/**
 * DisplayManager.h
 * Handles all display functions
 */

#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "DataStructures.h"

// Forward declaration
class BottleManager;

class DisplayManager {
private:
  int currentView;
  int selectedBottle;
  struct_message* incomingData; // Reference to received data

  unsigned long lastUpdateTime;
  String lastUpdateTimeString;
  bool showingUpdateIndicator;
  unsigned long updateIndicatorTimeout; // Timestamp when indicator should hide
  
  bool timeInitialized;  // Flag to indicate if time has been initialized
  bool processingMessage; // Flag to indicate if we're processing a message
  
  // Fixed position numbers array for consistent bottle numbering
  int positionNumbers[9] = {2, 4, 6, 8, 1, 3, 5, 7, 9};

public:
  DisplayManager(struct_message* data);
  
  // Main display functions
  void showWelcomeScreen(const char* statusMessage = "Waiting for connection...");
  void drawBottleGrid();
  void drawBottleDetail(int index);
  void displayStatusUpdate();
  void displayErrorMessage();
  void updateDisplay(int messageType);
  
  // Helper functions
  void drawHeader(const char* title);
  void drawBottleIcon(int x, int y, int status, int positionIndex);
  void drawWrappedText(const char* text, int x, int y, int maxWidth, uint8_t font, uint16_t color);
  void drawButton(int x, int y, int w, int h, const char* text, uint16_t color);
  
  // UI interaction
  bool isButtonPressed(int x, int y, int w, int h, uint16_t touchX, uint16_t touchY);
  void handleTouch(uint16_t touchX, uint16_t touchY);
  
  // State getters/setters
  int getCurrentView() { return currentView; }
  int getSelectedBottle() { return selectedBottle; }
  void setCurrentView(int view) { currentView = view; }
  void setSelectedBottle(int bottle) { selectedBottle = bottle; }

  void updateLastUpdateTime();
  void showUpdateIndicator();
  void hideUpdateIndicator();
  String getTimeString();
  
  // Time-related functions
  void setTimeInitialized(bool initialized) { timeInitialized = initialized; }
  bool isTimeInitialized() const { return timeInitialized; }
  String getCurrentTime() const;
  String getCurrentDate() const;
  void updateTimeDisplay();
  
  // Processing state management
  void setProcessingMessage(bool processing) { processingMessage = processing; }
  bool isProcessingMessage() const { return processingMessage; }

  bool isShowingUpdateIndicator() const { return showingUpdateIndicator; }
  bool shouldHideUpdateIndicator() const { return showingUpdateIndicator && millis() > updateIndicatorTimeout; }
};

// Declare external instance for global use
extern DisplayManager* displayManager;

#endif // DISPLAY_MANAGER_H