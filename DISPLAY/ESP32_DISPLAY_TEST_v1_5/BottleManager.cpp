/**
 * BottleManager.cpp
 * Implementation of bottle manager functions
 */

#include "BottleManager.h"
#include "gfx_conf.h"
#include "config.h"
#include <math.h> // For sqrt function in getBottleTouched

// Define the global instance
BottleManager bottleManager;

// Constructor
BottleManager::BottleManager() {
  // Initialize bottles to empty state
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    bottlePositions[i].status = BOTTLE_EMPTY;
    bottlePositions[i].bottleIndex = -1;
  }
}

// Initialize bottle positions on the screen
void BottleManager::initializeBottlePositions() {
  // Set positions in a 2-row grid (4 on top, 5 on bottom)
  int centerX = screenWidth / 2;
  int upperY = screenHeight / 3; // First row at 1/3 of the screen
  int lowerY = 2 * screenHeight / 3; // Second row at 2/3 of the screen
  
  // Spacing for top row (4 bottles)
  int topSpacingX = 160;
  
  // Spacing for bottom row (5 bottles) - wider
  int bottomSpacingX = 140;
  
  // Top row - 4 bottles (positions 0, 1, 2, 3)
  bottlePositions[0].x = centerX - (topSpacingX * 1.5);
  bottlePositions[0].y = upperY;
  
  bottlePositions[1].x = centerX - (topSpacingX * 0.5);
  bottlePositions[1].y = upperY;
  
  bottlePositions[2].x = centerX + (topSpacingX * 0.5);
  bottlePositions[2].y = upperY;
  
  bottlePositions[3].x = centerX + (topSpacingX * 1.5);
  bottlePositions[3].y = upperY;
  
  // Bottom row - 5 bottles (positions 4, 5, 6, 7, 8)
  bottlePositions[4].x = centerX - (bottomSpacingX * 2);
  bottlePositions[4].y = lowerY;
  
  bottlePositions[5].x = centerX - bottomSpacingX;
  bottlePositions[5].y = lowerY;
  
  bottlePositions[6].x = centerX;
  bottlePositions[6].y = lowerY;
  
  bottlePositions[7].x = centerX + bottomSpacingX;
  bottlePositions[7].y = lowerY;
  
  bottlePositions[8].x = centerX + (bottomSpacingX * 2);
  bottlePositions[8].y = lowerY;
}

// Get bottle data for a specific index
Bottle BottleManager::getBottle(int index) {
  if (index >= 0 && index < BOTTLE_COUNT) {
    return localBottles[index];
  }
  
  // Return an empty bottle if index is invalid
  Bottle emptyBottle;
  emptyBottle.position = -1;
  return emptyBottle;
}

// Get bottle position data
BottlePosition BottleManager::getBottlePosition(int index) {
  if (index >= 0 && index < BOTTLE_COUNT) {
    return bottlePositions[index];
  }
  
  // Return an empty position if index is invalid
  BottlePosition emptyPos;
  emptyPos.x = 0;
  emptyPos.y = 0;
  emptyPos.status = BOTTLE_NONE;
  emptyPos.bottleIndex = -1;
  return emptyPos;
}

// Update a bottle in the local array
void BottleManager::updateBottle(int index, const Bottle& bottle) {
  if (index >= 0 && index < BOTTLE_COUNT) {
    localBottles[index] = bottle;
    
    // Update the corresponding position status
    int position = bottle.position - 1; // Convert from 1-based to 0-based
    if (position >= 0 && position < BOTTLE_COUNT) {
      bottlePositions[position].bottleIndex = index;
      bottlePositions[position].status = bottle.inFridge ? BOTTLE_PRESENT : BOTTLE_WAITING;
    }
  }
}

// Update a bottle from bottle_info_message
void BottleManager::updateBottleFromMessage(const bottle_info_message& message) {
  if (message.bottleIndex >= 0 && message.bottleIndex < BOTTLE_COUNT) {
    int index = message.bottleIndex;
    int position = message.bottlePosition - 1; // Convert from 1-based to 0-based
    
    // If it's an empty bottle, just update the position status
    if (message.isEmpty) {
      if (position >= 0 && position < BOTTLE_COUNT) {
        bottlePositions[position].status = BOTTLE_EMPTY;
        bottlePositions[position].bottleIndex = -1;
      }
      return;
    }
    
    // Update the bottle data
    localBottles[index].barcode = message.barcode;
    localBottles[index].name = message.name;
    localBottles[index].type = message.type;
    localBottles[index].region = message.region;
    localBottles[index].vintage = message.vintage;
    localBottles[index].position = message.bottlePosition;
    localBottles[index].weight = message.weight;
    localBottles[index].lastInteraction = message.lastInteraction;
    localBottles[index].inFridge = message.inFridge;
    
    // Update the position status
    if (position >= 0 && position < BOTTLE_COUNT) {
      bottlePositions[position].bottleIndex = index;
      bottlePositions[position].status = message.inFridge ? BOTTLE_PRESENT : BOTTLE_WAITING;
    }
  }
}

// Update position status directly
void BottleManager::updatePositionStatus(int position, const BottlePosition& posData) {
  if (position >= 0 && position < BOTTLE_COUNT) {
    bottlePositions[position].status = posData.status;
    bottlePositions[position].bottleIndex = posData.bottleIndex;
  }
}

// Get the bottle index from touch coordinates
int BottleManager::getBottleTouched(uint16_t touchX, uint16_t touchY) {
  const int touchRadius = 50; // Touch detection radius
  
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    int deltaX = touchX - bottlePositions[i].x;
    int deltaY = touchY - bottlePositions[i].y;
    int distance = sqrt(deltaX * deltaX + deltaY * deltaY);
    
    if (distance < touchRadius) {
      return i;
    }
  }
  
  return -1; // No bottle touched
}