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

public:
  DisplayManager(struct_message* data);
  
  // Main display functions
  void showWelcomeScreen();
  void drawBottleGrid();
  void drawBottleDetail(int index);
  void displayStatusUpdate();
  void displayErrorMessage();
  void updateDisplay(int messageType);
  
  // Helper functions
  void drawHeader(const char* title);
  void drawBottleIcon(int x, int y, int status, int index);
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
};

// Declare external instance for global use
extern DisplayManager* displayManager;

#endif // DISPLAY_MANAGER_H