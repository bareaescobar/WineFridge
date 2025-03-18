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
  // Set positions in a 3x3 grid
  int centerX = screenWidth / 2;
  int centerY = screenHeight / 2 - 30; // Slightly higher than center
  int spacingX = 180;
  int spacingY = 130;
  
  // Top row
  bottlePositions[0].x = centerX - spacingX;
  bottlePositions[0].y = centerY - spacingY;
  
  bottlePositions[1].x = centerX;
  bottlePositions[1].y = centerY - spacingY;
  
  bottlePositions[2].x = centerX + spacingX;
  bottlePositions[2].y = centerY - spacingY;
  
  // Middle row
  bottlePositions[3].x = centerX - spacingX;
  bottlePositions[3].y = centerY;
  
  bottlePositions[4].x = centerX;
  bottlePositions[4].y = centerY;
  
  bottlePositions[5].x = centerX + spacingX;
  bottlePositions[5].y = centerY;
  
  // Bottom row
  bottlePositions[6].x = centerX - spacingX;
  bottlePositions[6].y = centerY + spacingY;
  
  bottlePositions[7].x = centerX;
  bottlePositions[7].y = centerY + spacingY;
  
  bottlePositions[8].x = centerX + spacingX;
  bottlePositions[8].y = centerY + spacingY;
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