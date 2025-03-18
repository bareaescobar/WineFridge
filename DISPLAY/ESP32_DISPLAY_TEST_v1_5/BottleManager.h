/**
 * BottleManager.h
 * Manages bottle data and positions
 */

#ifndef BOTTLE_MANAGER_H
#define BOTTLE_MANAGER_H

// Incluir solo lo necesario, NO incluir BottleManager.h aquí (¡esto causaba el bucle!)
#include "config.h"
#include "DataStructures.h"

class BottleManager {
private:
  Bottle localBottles[BOTTLE_COUNT];
  BottlePosition bottlePositions[BOTTLE_COUNT];

public:
  BottleManager();
  
  // Initialize bottle positions on screen
  void initializeBottlePositions();
  
  // Update bottle status based on received data
  void updateBottleStatus();
  
  // Getters
  Bottle getBottle(int index);
  BottlePosition getBottlePosition(int index);
  
  // Get bottle index from touch coordinates
  int getBottleTouched(uint16_t touchX, uint16_t touchY);
  
  // Update a bottle in the local array
  void updateBottle(int index, const Bottle& bottle);
  
  // Update a bottle from bottle_info_message
  void updateBottleFromMessage(const bottle_info_message& message);


};

// Declare external instance for global use
extern BottleManager bottleManager;

#endif // BOTTLE_MANAGER_H