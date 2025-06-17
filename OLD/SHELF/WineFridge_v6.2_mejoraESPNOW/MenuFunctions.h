// ==========================================================
//                MENU FUNCTIONS
// ==========================================================
#ifndef MENU_FUNCTIONS_H
#define MENU_FUNCTIONS_H

#include <Arduino.h>
#include "Config.h"
#include "BottleDatabase.h"
#include "WineCatalog.h"
#include "SensorFunctions.h"
#include "ButtonFunctions.h"
#include "UtilityFunctions.h"
#include "LEDFunctions.h"
#include "ESPNowFunctions.h"

// Declaración de funciones para evitar errores de referencias cruzadas
// Funciones principales de menú
void scanBottleAndPlace();
void showBottleDatabase();
void handleAutomaticBottleEvent();
void swapBottlePositions();
void sequentialBottleLoading();
void unloadBottleByRegion();
void configurationAndTestingMenu();

//MENU 1 & 9

void scanBottleAndPlace() {
  Serial.println("\nMode 1: Scan Bottle and Place");
    
  sendStatusUpdateToDisplay("Scanning bottle...");

  String barcode = scanBarcode();
  if (barcode.length() == 0) {
    Serial.println("No barcode scanned");
    sendErrorToDisplay("No barcode scanned");
    return;
  }

  // Send status update
  char statusMsg[250];
  snprintf(statusMsg, sizeof(statusMsg), "Barcode scanned: %s", barcode.c_str());
  sendStatusUpdateToDisplay(statusMsg);

  int bottleIndex = findBottleByBarcode(barcode);
  if (bottleIndex == -1) {
    Serial.println("Bottle not found in database");
    sendStatusUpdateToDisplay("Bottle not found in database. Checking catalog...");
    
    // Add code for new bottle scanned
    // Check if the wine is in the catalog
    WineInfo* wineInfo = findWineInCatalog(barcode);
    if (wineInfo != NULL) {
      // Find an empty slot in the bottles array
      for (int i = 0; i < BOTTLE_COUNT; i++) {
        if (bottles[i].barcode == "") {
          bottleIndex = i;
          bottles[i].barcode = barcode;
          bottles[i].name = wineInfo->name;
          bottles[i].type = wineInfo->type;
          bottles[i].region = wineInfo->region;
          bottles[i].vintage = wineInfo->vintage;
          bottles[i].position = i + 1; // Default to position matching index+1
          
          snprintf(statusMsg, sizeof(statusMsg), "New bottle added from catalog: %s", wineInfo->name.c_str());
          Serial.print("New bottle added from catalog: ");
          Serial.println(wineInfo->name);
          sendStatusUpdateToDisplay(statusMsg);
          break;
        }
      }
    } else {
      Serial.println("Bottle not found in wine catalog either. Please enter details manually.");
      sendErrorToDisplay("Bottle not found in catalog. Please enter details manually.");
      return;
    }
    
    if (bottleIndex == -1) {
      Serial.println("No empty slots in database!");
      sendErrorToDisplay("No empty slots in database!");
      return;
    }
  }

  Serial.print("Bottle found: ");
  Serial.println(bottles[bottleIndex].name);
  Serial.print("Place bottle at position: ");
  Serial.println(bottles[bottleIndex].position);

  // Send bottle info to display
  sendBottleInfoToDisplay(bottleIndex);

  // Highlight the target position with blue LED
  highlightPosition(bottles[bottleIndex].position);

  String message = "Please place the bottle in position " + String(bottles[bottleIndex].position);
  
  snprintf(statusMsg, sizeof(statusMsg), "Place bottle: %s at position %d", 
           bottles[bottleIndex].name.c_str(), bottles[bottleIndex].position);
  sendStatusUpdateToDisplay(statusMsg);
  
  waitForBottlePlacement(bottles[bottleIndex].position, message);

  Serial.println("Measuring weight...");
  sendStatusUpdateToDisplay("Measuring weight...");
  float newWeight = measureWeight();

  bottles[bottleIndex].weight = newWeight;
  bottles[bottleIndex].lastInteraction = getTimeStamp();
  bottles[bottleIndex].inFridge = true;

  Serial.print("New weight recorded: ");
  Serial.println(newWeight);

  // Send updated bottle info
  snprintf(statusMsg, sizeof(statusMsg), "Bottle placed successfully. Weight: %.1fg", newWeight);
  sendStatusUpdateToDisplay(statusMsg);
  sendBottleInfoToDisplay(bottleIndex);

  // Success feedback is already handled by waitForBottlePlacement()

  saveDatabase();
}

//MENU 2

void showBottleDatabase() {
  // Turn off all LEDs while viewing the database
  clearAllLEDs();
  
  // Send database to display
  sendBottleDatabaseToDisplay();
  sendStatusUpdateToDisplay("Viewing bottle database...");
  
  // Clear previous output with some newlines
  Serial.println("\n\n");

  // Print a decorative header using simple ASCII characters
  Serial.println("+----------------------------------------------------------------------+");
  Serial.println("|                           WINE FRIDGE INVENTORY                      |");
  Serial.println("+----------------------------------------------------------------------+");

  // Print column headers with alignment
  Serial.println("+----------------+--------------------------------------+----------------+----------------+--------+---------+");
  Serial.println("| Position       | Wine Name                            | Type           | Region         | Year   | Weight  |");
  Serial.println("+----------------+--------------------------------------+----------------+----------------+--------+---------+");

  // Sort bottles by position
  // Create a temporary array of indices
  int indices[BOTTLE_COUNT];
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    indices[i] = i;
  }
  
  // Simple bubble sort to order by position
  for (int i = 0; i < BOTTLE_COUNT - 1; i++) {
    for (int j = 0; j < BOTTLE_COUNT - i - 1; j++) {
      if (bottles[indices[j]].position > bottles[indices[j + 1]].position) {
        // Swap indices
        int temp = indices[j];
        indices[j] = indices[j + 1];
        indices[j + 1] = temp;
      }
    }
  }

  // Print each bottle's information in a table row (sorted by position)
  for (int idx = 0; idx < BOTTLE_COUNT; idx++) {
    int i = indices[idx];
    Bottle& bottle = bottles[i];

    // Format the bottle name (truncate if too long)
    String displayName = bottle.name;
    if (displayName.length() > 36) {
      displayName = displayName.substring(0, 33) + "...";
    }

    // Format position
    String positionStr = String(bottle.position) + " ";
    if (bottle.inFridge) {
      positionStr += "[PRESENT]";
    } else {
      positionStr += "[EMPTY]";
    }

    // Print the formatted row with consistent spacing
    Serial.print("| ");

    // Position column - ensure it's exactly 14 characters
    Serial.print(positionStr);
    for (int j = positionStr.length(); j < 14; j++) Serial.print(" ");
    Serial.print(" | ");

    // Name column - ensure it's exactly 36 characters
    Serial.print(displayName);
    for (int j = displayName.length(); j < 36; j++) Serial.print(" ");
    Serial.print(" | ");
    
    // Type column - ensure it's exactly 14 characters
    String typeStr = bottle.type;
    Serial.print(typeStr);
    for (int j = typeStr.length(); j < 14; j++) Serial.print(" ");
    Serial.print(" | ");
    
    // Region column - ensure it's exactly 14 characters
    String regionStr = bottle.region;
    Serial.print(regionStr);
    for (int j = regionStr.length(); j < 14; j++) Serial.print(" ");
    Serial.print(" | ");
    
    // Vintage column - ensure it's exactly 6 characters
    String vintageStr = bottle.vintage;
    Serial.print(vintageStr);
    for (int j = vintageStr.length(); j < 6; j++) Serial.print(" ");
    Serial.print(" | ");

    // Weight column - ensure it's exactly 7 characters
    String weightStr = String(bottle.weight, 1) + "g";
    Serial.print(weightStr);
    for (int j = weightStr.length(); j < 7; j++) Serial.print(" ");
    Serial.println(" |");
  }

  Serial.println("+----------------+--------------------------------------+----------------+----------------+--------+---------+");

  // Add detailed view section
  Serial.println();
  Serial.println("+----------------------------------------------------------------------+");
  Serial.println("|                              DETAILED VIEW                           |");
  Serial.println("+----------------------------------------------------------------------+");

  // Sort indices for detailed view (same order as table)
  for (int idx = 0; idx < BOTTLE_COUNT; idx++) {
    int i = indices[idx];
    Bottle& bottle = bottles[i];

    // Only show detailed info for bottles that are in the database (have names)
    if (bottle.name != "" && bottle.name != "Bottle " + String(i+1)) {
      Serial.println("+----------------------------------------------------------------------+");

      // Calculate total line width (68 characters between the borders)
      const int LINE_WIDTH = 68;

      // Format the position
      String headerText = "Position " + String(bottle.position) + " - " +
                          (bottle.inFridge ? "IN FRIDGE" : "REMOVED");
      Serial.print("| ");
      Serial.print(headerText);

      // Add spaces to align the right border
      for (int j = headerText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");

      // Print name with proper padding
      String nameText = "Name: " + bottle.name;
      Serial.print("| ");
      Serial.print(nameText);
      for (int j = nameText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");

      // Print type with proper padding
      String typeText = "Type: " + bottle.type;
      Serial.print("| ");
      Serial.print(typeText);
      for (int j = typeText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");
      
      // Print region with proper padding
      String regionText = "Region: " + bottle.region;
      Serial.print("| ");
      Serial.print(regionText);
      for (int j = regionText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");
      
      // Print vintage with proper padding
      String vintageText = "Vintage: " + bottle.vintage;
      Serial.print("| ");
      Serial.print(vintageText);
      for (int j = vintageText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");

      // Print barcode with proper padding
      String barcodeText = "Barcode: " + bottle.barcode;
      Serial.print("| ");
      Serial.print(barcodeText);
      for (int j = barcodeText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");

      // Print weight with proper padding
      String weightText = "Weight: " + String(bottle.weight, 2) + "g";
      Serial.print("| ");
      Serial.print(weightText);
      for (int j = weightText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");

      // Print last interaction with proper padding
      String lastActivityText = "Last Activity: " + bottle.lastInteraction;
      Serial.print("| ");
      Serial.print(lastActivityText);
      for (int j = lastActivityText.length(); j < LINE_WIDTH - 1; j++) Serial.print(" ");
      Serial.println(" |");

      Serial.println("+----------------------------------------------------------------------+");
    }
  }

  // Add a note about empty positions
  Serial.println("\nNote: Press any key to return to menu...");

  // Wait for a key press before returning to menu
  while (Serial.available() == 0) {
    delay(100);
  }
  Serial.read(); // Clear the input buffer
}

//MENU 3

void handleAutomaticBottleEvent() {
  Serial.println("\nMode 3: Automatic Bottle Removal/Return");
  sendStatusUpdateToDisplay("Automatic Bottle Removal/Return Mode");
  
  // Highlight all positions that have bottles with blue LED
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      highlightPosition(i + 1);
    }
  }
  
  // Send status message
  sendStatusUpdateToDisplay("Waiting for a bottle to be removed...");
  
  // Wait for a bottle to be removed
  int removedBottleIndex = waitForBottleRemoval();
  
  Serial.print("Bottle removed: ");
  Serial.println(bottles[removedBottleIndex].name);
  printBottleInfo(removedBottleIndex);
  
  // Send status update about removed bottle
  char statusMsg[250];
  snprintf(statusMsg, sizeof(statusMsg), "Bottle removed: %s from position %d", 
           bottles[removedBottleIndex].name.c_str(), 
           bottles[removedBottleIndex].position);
  sendStatusUpdateToDisplay(statusMsg);
  sendBottleInfoToDisplay(removedBottleIndex);
  
  // Wait for the bottle to be returned
  int expectedPosition = bottles[removedBottleIndex].position;
  
  snprintf(statusMsg, sizeof(statusMsg), "Please return the bottle to position %d within %d seconds", 
           expectedPosition, RETURN_TIMEOUT_MS/1000);
  sendStatusUpdateToDisplay(statusMsg);
  
  bool returned = waitForBottleReturn(expectedPosition, RETURN_TIMEOUT_MS);
  
  // Update bottle status
  bottles[removedBottleIndex].inFridge = returned;
  bottles[removedBottleIndex].lastInteraction = getTimeStamp();
  
  if (returned) {
    Serial.println("Bottle returned to fridge");
    snprintf(statusMsg, sizeof(statusMsg), "Bottle returned to fridge at position %d", expectedPosition);
    sendStatusUpdateToDisplay(statusMsg);
    // Success feedback is already handled by waitForBottleReturn()
  } else {
    Serial.println("Bottle removed from fridge");
    snprintf(statusMsg, sizeof(statusMsg), "Bottle removed: %s was not returned", 
             bottles[removedBottleIndex].name.c_str());
    sendStatusUpdateToDisplay(statusMsg);
  }
  
  // Send updated bottle info
  sendBottleInfoToDisplay(removedBottleIndex);
  
  saveDatabase();
}

//MENU 4 & 8

void swapBottlePositions() {
  // Clear previous output with some newlines
  Serial.println("\n\n");

  // Print a decorative header using simple ASCII characters
  Serial.println("+----------------------------------------------------------------------+");
  Serial.println("|                           SWAP BOTTLE POSITIONS                      |");
  Serial.println("+----------------------------------------------------------------------+");
  
  sendStatusUpdateToDisplay("Swap Bottle Positions Mode");
  
  // Highlight all positions that have bottles with blue LED
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      highlightPosition(i + 1);
    }
  }
  
  // First bottle selection
  sendStatusUpdateToDisplay("Please remove the first bottle to swap");
  Serial.println("\nPlease remove the first bottle to swap");
  int firstIndex = waitForBottleRemoval();
  if (firstIndex < 0 || firstIndex >= BOTTLE_COUNT) {
    Serial.println("Error: Invalid bottle selection. Returning to menu.");
    sendErrorToDisplay("Invalid bottle selection");
    return;
  }
  
  // Store the original position of the first bottle
  int firstPos = bottles[firstIndex].position;
  Serial.print("First bottle removed: ");
  Serial.println(bottles[firstIndex].name + " (Position " + String(firstPos) + ")");
  
  // Send status update
  char statusMsg[250];
  snprintf(statusMsg, sizeof(statusMsg), "First bottle removed: %s (Position %d)", 
           bottles[firstIndex].name.c_str(), firstPos);
  sendStatusUpdateToDisplay(statusMsg);
  
  // Turn off all LEDs except bottles that are still in place
  clearAllLEDs();
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (i != firstIndex && bottles[i].inFridge) {
      highlightPosition(i + 1);
    }
  }
  
  // Second bottle selection
  sendStatusUpdateToDisplay("Please remove the second bottle to swap");
  Serial.println("\nPlease remove the second bottle to swap");
  int secondIndex = waitForBottleRemoval();
  
  // Validate the second bottle selection
  if (secondIndex < 0 || secondIndex >= BOTTLE_COUNT) {
    Serial.println("Error: Invalid bottle selection. Returning to menu.");
    sendErrorToDisplay("Invalid second bottle selection");
    
    // Return the first bottle to its position
    Serial.println("Please return the first bottle to position " + String(firstPos));
    highlightPosition(firstPos);
    
    snprintf(statusMsg, sizeof(statusMsg), "Return first bottle to position %d", firstPos);
    sendStatusUpdateToDisplay(statusMsg);
    
    waitForBottlePlacement(firstPos, "Return first bottle to original position");
    return;
  }
  
  // Check if the same bottle was selected twice
  if (secondIndex == firstIndex) {
    Serial.println("Error: You cannot swap a bottle with itself. Returning to menu.");
    sendErrorToDisplay("Cannot swap a bottle with itself");
    
    // Return the first bottle to its position
    Serial.println("Please return the first bottle to position " + String(firstPos));
    highlightPosition(firstPos);
    
    snprintf(statusMsg, sizeof(statusMsg), "Return first bottle to position %d", firstPos);
    sendStatusUpdateToDisplay(statusMsg);
    
    waitForBottlePlacement(firstPos, "Return first bottle to original position");
    return;
  }
  
  // Store the original position of the second bottle
  int secondPos = bottles[secondIndex].position;
  Serial.print("Second bottle removed: ");
  Serial.println(bottles[secondIndex].name + " (Position " + String(secondPos) + ")");
  
  // Send status update
  snprintf(statusMsg, sizeof(statusMsg), "Second bottle removed: %s (Position %d)", 
           bottles[secondIndex].name.c_str(), secondPos);
  sendStatusUpdateToDisplay(statusMsg);
  
  // Print information about the swap
  Serial.println("\n+----------------------------------------------------------------------+");
  Serial.println("| Swap details:                                                        |");
  Serial.println("+----------------------------------------------------------------------+");
  
  String firstBottleInfo = "| First bottle: " + bottles[firstIndex].name + " (Position " + String(firstPos) + ")";
  Serial.print(firstBottleInfo);
  for (int j = firstBottleInfo.length(); j < 70; j++) Serial.print(" ");
  Serial.println("|");
  
  String secondBottleInfo = "| Second bottle: " + bottles[secondIndex].name + " (Position " + String(secondPos) + ")";
  Serial.print(secondBottleInfo);
  for (int j = secondBottleInfo.length(); j < 70; j++) Serial.print(" ");
  Serial.println("|");
  
  Serial.println("+----------------------------------------------------------------------+");
  
  // Place bottles in swapped positions
  Serial.println("\nPlease follow the instructions below to complete the swap:");
  
  // Guide user to place first bottle in second position
  String message1 = "Place the first bottle ('" + bottles[firstIndex].name + "') in position " + String(secondPos);
  highlightPosition(secondPos);
  
  snprintf(statusMsg, sizeof(statusMsg), "Place %s in position %d", 
           bottles[firstIndex].name.c_str(), secondPos);
  sendStatusUpdateToDisplay(statusMsg);
  
  waitForBottlePlacement(secondPos, message1);
  
  // Guide user to place second bottle in first position
  String message2 = "Place the second bottle ('" + bottles[secondIndex].name + "') in position " + String(firstPos);
  highlightPosition(firstPos);
  
  snprintf(statusMsg, sizeof(statusMsg), "Place %s in position %d", 
           bottles[secondIndex].name.c_str(), firstPos);
  sendStatusUpdateToDisplay(statusMsg);
  
  waitForBottlePlacement(firstPos, message2);
  
  // Update database with the swapped positions
  int tempPosition = bottles[firstIndex].position;
  bottles[firstIndex].position = bottles[secondIndex].position;
  bottles[secondIndex].position = tempPosition;
  
  // Mark both bottles as in the fridge and update timestamps
  bottles[firstIndex].inFridge = true;
  bottles[secondIndex].inFridge = true;
  bottles[firstIndex].lastInteraction = getTimeStamp();
  bottles[secondIndex].lastInteraction = getTimeStamp();
  
  Serial.println("\n+----------------------------------------------------------------------+");
  Serial.println("|                     BOTTLES SWAPPED SUCCESSFULLY                     |");
  Serial.println("+----------------------------------------------------------------------+");
  
  // Send success message and updated bottle info
  sendStatusUpdateToDisplay("Bottles swapped successfully!");
  sendBottleInfoToDisplay(firstIndex);
  sendBottleInfoToDisplay(secondIndex);
  
  saveDatabase();
  
  // Add a note about returning to menu
  Serial.println("\nNote: Press any key to return to menu...");

  // Wait for a key press before returning to menu
  while (Serial.available() == 0) {
    delay(100);
  }
  Serial.read(); // Clear the input buffer
}

//MENU 6 SEQUENCIAL BOTTLE LOADING PROCESS

void sequentialBottleLoading() {
  Serial.println("\nMode 6: Individual Sequential Loading");
  Serial.println("\n+-----------------------------------------------------+");
  Serial.println("|        SEQUENTIAL BOTTLE LOADING PROCESS            |");
  Serial.println("+-----------------------------------------------------+");
  
  sendStatusUpdateToDisplay("Sequential Bottle Loading Mode");
  
  // First, check how many available positions we have
  int availablePositions[BOTTLE_COUNT];
  int numAvailable = 0;
  
  // Update all button states
  updateAllButtons();
  
  // Find all available positions (physical check)
  Serial.println("\nChecking available positions...");
  sendStatusUpdateToDisplay("Checking available positions...");
  
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottleButtons[i]->getState() == HIGH) { // HIGH means not pressed (position is empty)
      availablePositions[numAvailable] = i + 1; // Store 1-based position
      numAvailable++;
    }
  }
  
  if (numAvailable == 0) {
    Serial.println("\nError: No available positions detected in the fridge!");
    Serial.println("Please remove some bottles and try again.");
    sendErrorToDisplay("No available positions in the fridge! Please remove some bottles.");
    delay(2000);
    return;
  }
  
  char statusMsg[250];
  snprintf(statusMsg, sizeof(statusMsg), "Found %d available positions", numAvailable);
  sendStatusUpdateToDisplay(statusMsg);
  
  Serial.printf("\nFound %d available positions: ", numAvailable);
  for (int i = 0; i < numAvailable; i++) {
    Serial.printf("%d ", availablePositions[i]);
    // Highlight available positions
    highlightPosition(availablePositions[i]);
  }
  Serial.println();
  
  Serial.println("\nThis mode will help you scan and load bottles into available positions.");
  Serial.println("Press 'q' at any time to quit this mode.\n");
  
  // Process bottles for each available position
  for (int posIndex = 0; posIndex < numAvailable; posIndex++) {
    int currentPosition = availablePositions[posIndex];
    
    // Check if user wants to exit
    if (Serial.available() > 0) {
      char input = Serial.read();
      if (input == 'q' || input == 'Q') {
        Serial.println("\nExiting sequential loading mode...");
        sendStatusUpdateToDisplay("Exiting sequential loading mode...");
        return;
      }
    }
    
    // Clear LEDs and highlight current position
    clearAllLEDs();
    highlightPosition(currentPosition);
    
    // Show progress - using actual position number instead of sequential
    Serial.println("\n+-----------------------------------------------------+");
    Serial.printf("| BOTTLE %d                                           |\n", currentPosition);
    Serial.println("+-----------------------------------------------------+");
    
    snprintf(statusMsg, sizeof(statusMsg), "Processing bottle for position %d", currentPosition);
    sendStatusUpdateToDisplay(statusMsg);
    
    // Scan barcode
    Serial.printf("\nPlease scan the barcode for position %d:\n", currentPosition);
    sendStatusUpdateToDisplay("Scan barcode for the bottle");
    
    String barcode = scanBarcode();
    
    if (barcode.length() == 0) {
      Serial.println("No barcode scanned. Try again or press 'q' to quit.");
      sendErrorToDisplay("No barcode scanned");
      posIndex--; // Retry this position
      continue;
    }
    
    snprintf(statusMsg, sizeof(statusMsg), "Barcode scanned: %s", barcode.c_str());
    sendStatusUpdateToDisplay(statusMsg);
    
    // Check if bottle already exists in database
    int existingIndex = findBottleByBarcode(barcode);
    int bottleIndex;

    if (existingIndex != -1) {
      // Bottle already exists in database
      bottleIndex = existingIndex;
      Serial.print("Bottle found in database: ");
      Serial.println(bottles[bottleIndex].name);
      
      snprintf(statusMsg, sizeof(statusMsg), "Bottle found: %s", bottles[bottleIndex].name.c_str());
      sendStatusUpdateToDisplay(statusMsg);

      if (bottles[bottleIndex].inFridge) {
        Serial.println("Warning: This bottle is already registered as being in the fridge!");
        Serial.println("Do you want to update its position? (y/n)");
        
        sendStatusUpdateToDisplay("Warning: Bottle already in fridge. Update position? (y/n)");

        while (Serial.available() == 0) {
          delay(100);
        }
        char choice = Serial.read();
        if (choice != 'y' && choice != 'Y') {
          Serial.println("Skipping this bottle...");
          sendStatusUpdateToDisplay("Skipping this bottle");
          continue;
        }
      }
    } else {
      // New bottle, add to database
      // Find an empty slot in the bottles array
      bottleIndex = -1;
      for (int i = 0; i < BOTTLE_COUNT; i++) {
        if (bottles[i].barcode == "") {
          bottleIndex = i;
          break;
        }
      }

      if (bottleIndex == -1) {
        Serial.println("Error: No empty slots in database!");
        sendErrorToDisplay("No empty slots in database!");
        continue;
      }
      
      // Check if wine exists in catalog
      WineInfo* wineInfo = findWineInCatalog(barcode);
      
      if (wineInfo != NULL) {
        // Fill in details from catalog
        bottles[bottleIndex].barcode = barcode;
        bottles[bottleIndex].name = wineInfo->name;
        bottles[bottleIndex].type = wineInfo->type;
        bottles[bottleIndex].region = wineInfo->region;
        bottles[bottleIndex].vintage = wineInfo->vintage;
        
        Serial.println("New bottle added from catalog:");
        Serial.print("Name: ");
        Serial.println(wineInfo->name);
        
        snprintf(statusMsg, sizeof(statusMsg), "New bottle from catalog: %s", wineInfo->name.c_str());
        sendStatusUpdateToDisplay(statusMsg);
      } else {
        // Manual entry for bottles not in catalog
        Serial.println("New bottle detected (not in catalog)!");
        sendStatusUpdateToDisplay("New bottle (not in catalog). Enter details:");
        
        Serial.print("Enter bottle name: ");
        sendStatusUpdateToDisplay("Enter bottle name");
        while (Serial.available() == 0) { delay(100); }
        String name = Serial.readStringUntil('\n');
        name.trim();
        
        Serial.print("Enter type (e.g., Reserva, Crianza): ");
        sendStatusUpdateToDisplay("Enter bottle type");
        while (Serial.available() == 0) { delay(100); }
        String type = Serial.readStringUntil('\n');
        type.trim();
        
        Serial.print("Enter region (e.g., Rioja): ");
        sendStatusUpdateToDisplay("Enter bottle region");
        while (Serial.available() == 0) { delay(100); }
        String region = Serial.readStringUntil('\n');
        region.trim();
        
        Serial.print("Enter vintage year: ");
        sendStatusUpdateToDisplay("Enter vintage year");
        while (Serial.available() == 0) { delay(100); }
        String vintage = Serial.readStringUntil('\n');
        vintage.trim();
        
        bottles[bottleIndex].barcode = barcode;
        bottles[bottleIndex].name = name;
        bottles[bottleIndex].type = type;
        bottles[bottleIndex].region = region;
        bottles[bottleIndex].vintage = vintage;
        
        Serial.print("New bottle added: ");
        Serial.println(name);
        
        snprintf(statusMsg, sizeof(statusMsg), "New bottle added: %s", name.c_str());
        sendStatusUpdateToDisplay(statusMsg);
      }
    }
    
    // Tell user where to place the bottle
    Serial.print("Please place the bottle in position ");
    Serial.println(currentPosition);
    
    // Ensure the current position is highlighted
    highlightPosition(currentPosition);
    
    snprintf(statusMsg, sizeof(statusMsg), "Place bottle in position %d", currentPosition);
    sendStatusUpdateToDisplay(statusMsg);
    
    // Wait for bottle placement in the specified position
    String message = "Waiting for placement in position " + String(currentPosition) + "...";
    int placedIndex = waitForBottlePlacement(currentPosition, message);
    
    if (placedIndex != currentPosition - 1) {
      Serial.println("Error: Bottle not placed in the correct position!");
      showErrorFeedback(placedIndex + 1);
      sendErrorToDisplay("Bottle not placed in correct position!");
      posIndex--; // Retry this position
      continue;
    }
    
    delay(2000);

    // Measure weight
    Serial.println("Measuring bottle weight...");
    sendStatusUpdateToDisplay("Measuring bottle weight...");
    float weight = measureWeight();
    
    // Update bottle information
    bottles[bottleIndex].position = currentPosition;
    bottles[bottleIndex].weight = weight;
    bottles[bottleIndex].lastInteraction = getTimeStamp();
    bottles[bottleIndex].inFridge = true;
    
    // Save to database
    saveDatabase();
    
    // Provide feedback
    Serial.println("\n+-----------------------------------------------------+");
    Serial.println("| BOTTLE SUCCESSFULLY REGISTERED                      |");
    Serial.println("+-----------------------------------------------------+");
    Serial.print("Name: ");
    Serial.println(bottles[bottleIndex].name);
    Serial.print("Position: ");
    Serial.println(currentPosition);
    Serial.print("Weight: ");
    Serial.print(weight);
    Serial.println("g");
    
    // Send updated bottle info to display
    char statusMsg[250];
    snprintf(statusMsg, sizeof(statusMsg), "Bottle registered: %s, Position: %d, Weight: %.1fg", 
             bottles[bottleIndex].name.c_str(), currentPosition, weight);
    sendStatusUpdateToDisplay(statusMsg);
    sendBottleInfoToDisplay(bottleIndex);
    
    // If all available positions are filled, break the loop
    if (posIndex >= numAvailable - 1) {
      Serial.println("\nAll available positions filled!");
      sendStatusUpdateToDisplay("All available positions filled!");
      break;
    }
    
    Serial.println("\nPress any key to continue with the next position or 'q' to quit...");
    sendStatusUpdateToDisplay("Press any key to continue or 'q' to quit");
    
    // Wait for user input
    while (Serial.available() == 0) {
      delay(100);
    }
    
    char input = Serial.read();
    if (input == 'q' || input == 'Q') {
      Serial.println("\nExiting sequential loading mode...");
      sendStatusUpdateToDisplay("Exiting sequential loading mode...");
      return;
    }
    
    // Clear any remaining characters in the input buffer
    while (Serial.available() > 0) {
      Serial.read();
    }
  }
  
  Serial.println("\n+-----------------------------------------------------+");
  Serial.println("|       SEQUENTIAL LOADING PROCESS COMPLETED          |");
  Serial.println("+-----------------------------------------------------+");
  Serial.println("\nAll available positions have been filled.");
  Serial.println("Returning to main menu...");
  
  sendStatusUpdateToDisplay("Sequential loading completed. Returning to main menu...");
  delay(2000);
}

// MENU 7 - Unloading bottle by region

void unloadBottleByRegion() {
  Serial.println("\nMode 7: Unload Bottle by Region");
  Serial.println("\n+-----------------------------------------------------+");
  Serial.println("|          UNLOAD BOTTLE BY REGION PROCESS            |");
  Serial.println("+-----------------------------------------------------+");

  sendStatusUpdateToDisplay("Unload Bottle by Region Mode");

  // Step 1: Display available regions
  Serial.println("\nAvailable regions:");
  
  // Create an array to store unique regions
  String uniqueRegions[BOTTLE_COUNT];
  int regionCount = 0;
  
  // Find all unique regions from bottles that are currently in the fridge
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge && bottles[i].region.length() > 0) {
      bool regionExists = false;
      
      // Check if this region is already in our list
      for (int j = 0; j < regionCount; j++) {
        if (uniqueRegions[j] == bottles[i].region) {
          regionExists = true;
          break;
        }
      }
      
      // If not, add it
      if (!regionExists) {
        uniqueRegions[regionCount] = bottles[i].region;
        regionCount++;
      }
    }
  }
  
  // Display the regions with numbers for selection
  for (int i = 0; i < regionCount; i++) {
    Serial.print(i + 1);
    Serial.print(" - ");
    Serial.println(uniqueRegions[i]);
  }
  
  // If no regions found
  if (regionCount == 0) {
    Serial.println("No bottles with region information found in the fridge!");
    sendErrorToDisplay("No bottles with region information found in the fridge!");
    return;
  }
  
  // Create region list for display
  char regionListMsg[250] = "Available regions:\n";
  for (int i = 0; i < regionCount && i < 5; i++) { // Limit to 5 regions to fit in message
    char regionItem[50];
    snprintf(regionItem, sizeof(regionItem), "%d - %s\n", i+1, uniqueRegions[i].c_str());
    strcat(regionListMsg, regionItem);
  }
  if (regionCount > 5) {
    strcat(regionListMsg, "...");
  }
  sendStatusUpdateToDisplay(regionListMsg);
  
  // Step 2: Ask user to select a region
  Serial.print("\nSelect a region (1-");
  Serial.print(regionCount);
  Serial.print("): ");
  
  int regionSelection = readIntFromSerial();
  
  // Validate selection
  if (regionSelection <= 0 || regionSelection > regionCount) {
    Serial.println("Invalid selection!");
    sendErrorToDisplay("Invalid region selection!");
    return;
  }
  
  String selectedRegion = uniqueRegions[regionSelection - 1];
  Serial.print("Selected region: ");
  Serial.println(selectedRegion);
  
  char statusMsg[250];
  snprintf(statusMsg, sizeof(statusMsg), "Selected region: %s", selectedRegion.c_str());
  sendStatusUpdateToDisplay(statusMsg);
  
  // Step 3: Display bottles from the selected region
  Serial.println("\nBottles from the selected region:");
  Serial.println("+----+----------------+--------------------------------------+----------------+--------+---------+");
  Serial.println("| ID | Position       | Wine Name                            | Type           | Year   | Weight  |");
  Serial.println("+----+----------------+--------------------------------------+----------------+--------+---------+");
  
  int bottleOptions[BOTTLE_COUNT];
  int optionCount = 0;
  
  // Clear LEDs to start fresh
  clearAllLEDs();
  
  // Find and display bottles from the selected region and highlight their positions
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge && bottles[i].region == selectedRegion) {
      Bottle& bottle = bottles[i];
      
      // Highlight the position of this bottle
      highlightPosition(bottle.position);
      
      // Format the bottle name (truncate if too long)
      String displayName = bottle.name;
      if (displayName.length() > 36) {
        displayName = displayName.substring(0, 33) + "...";
      }
      
      // Format position
      String positionStr = String(bottle.position);
      
      // Print the formatted row with consistent spacing
      Serial.print("| ");
      Serial.print(optionCount + 1);  // Display option number
      for (int j = String(optionCount + 1).length(); j < 2; j++) Serial.print(" ");
      Serial.print(" | ");
      
      // Position column - ensure it's exactly 14 characters
      Serial.print(positionStr);
      for (int j = positionStr.length(); j < 14; j++) Serial.print(" ");
      Serial.print(" | ");
      
      // Name column - ensure it's exactly 36 characters
      Serial.print(displayName);
      for (int j = displayName.length(); j < 36; j++) Serial.print(" ");
      Serial.print(" | ");
      
      // Type column - ensure it's exactly 14 characters
      Serial.print(bottle.type);
      for (int j = bottle.type.length(); j < 14; j++) Serial.print(" ");
      Serial.print(" | ");
      
      // Vintage column - ensure it's exactly 6 characters
      Serial.print(bottle.vintage);
      for (int j = bottle.vintage.length(); j < 6; j++) Serial.print(" ");
      Serial.print(" | ");
      
      // Weight column - ensure it's exactly 7 characters
      String weightStr = String(bottle.weight, 1) + "g";
      Serial.print(weightStr);
      for (int j = weightStr.length(); j < 7; j++) Serial.print(" ");
      Serial.println(" |");
      
      // Store the bottle index for this option
      bottleOptions[optionCount] = i;
      optionCount++;
    }
  }
  
  Serial.println("+----+----------------+--------------------------------------+----------------+--------+---------+");
  
  // If no bottles from this region
  if (optionCount == 0) {
    Serial.println("No bottles from this region found in the fridge!");
    sendErrorToDisplay("No bottles from this region found in the fridge!");
    return;
  }
  
  // Send info to display about available bottles
  snprintf(statusMsg, sizeof(statusMsg), 
           "Found %d bottles from %s region.\nThe bottles are highlighted. Please remove one.", 
           optionCount, selectedRegion.c_str());
  sendStatusUpdateToDisplay(statusMsg);
  
  // Step 4: Wait for user to physically remove the bottle
  Serial.println("\nPlease remove one of the bottles from the list.");
  Serial.println("The system will detect which bottle was removed.\n");
  
  int removedBottlePosition = waitForBottleRemoval();
  
  // Find which bottle was removed based on the position
  int removedBottleIndex = -1;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].position == removedBottlePosition + 1) {  // +1 because position is 1-indexed
      removedBottleIndex = i;
      break;
    }
  }
  
  if (removedBottleIndex == -1) {
    Serial.println("Error: Could not determine which bottle was removed!");
    sendErrorToDisplay("Could not determine which bottle was removed!");
    return;
  }
  
  // Check if the removed bottle belongs to the selected region
  if (bottles[removedBottleIndex].region != selectedRegion) {
    Serial.println("\n+-----------------------------------------------------+");
    Serial.println("|                      ERROR!                          |");
    Serial.println("+-----------------------------------------------------+");
    Serial.println("You have removed a bottle from a different region!");
    Serial.print("The bottle you removed is from: ");
    Serial.println(bottles[removedBottleIndex].region);
    Serial.print("But you selected the region: ");
    Serial.println(selectedRegion);
    Serial.println("\nPlease place the bottle back in its position.");
    
    // Send error message to display
    snprintf(statusMsg, sizeof(statusMsg), 
             "ERROR: Removed bottle is from %s, not from %s.\nPlease return it.", 
             bottles[removedBottleIndex].region.c_str(), selectedRegion.c_str());
    sendErrorToDisplay(statusMsg);
    
    // Show error feedback with LED
    showErrorFeedback(bottles[removedBottleIndex].position);
    
    // Wait for the bottle to be placed back
    String message = "Waiting for bottle to be returned to position " + String(bottles[removedBottleIndex].position) + "...";
    highlightPosition(bottles[removedBottleIndex].position);
    
    snprintf(statusMsg, sizeof(statusMsg), "Return bottle to position %d", bottles[removedBottleIndex].position);
    sendStatusUpdateToDisplay(statusMsg);
    
    waitForBottlePlacement(bottles[removedBottleIndex].position, message);
    
    Serial.println("\nBottle returned to position. Please try again with the correct bottle.");
    sendStatusUpdateToDisplay("Bottle returned to position. Please try again with the correct bottle.");
    delay(2000);
    return;
  }
  
  // Step 5: Measure the weight of the empty position
  Serial.println("\nMeasuring weight of the empty position...");
  sendStatusUpdateToDisplay("Measuring weight of the empty position...");
  float emptyWeight = measureWeight();
  
  // Step 6: Update the bottle status
  bottles[removedBottleIndex].inFridge = false;
  bottles[removedBottleIndex].lastInteraction = getTimeStamp();
  
  // Save database
  saveDatabase();
  
  // Step 7: Display summary
  Serial.println("\n+-----------------------------------------------------+");
  Serial.println("|                BOTTLE UNLOADED                      |");
  Serial.println("+-----------------------------------------------------+");
  Serial.print("Bottle name: ");
  Serial.println(bottles[removedBottleIndex].name);
  Serial.print("Type: ");
  Serial.println(bottles[removedBottleIndex].type);
  Serial.print("Region: ");
  Serial.println(bottles[removedBottleIndex].region);
  Serial.print("Vintage: ");
  Serial.println(bottles[removedBottleIndex].vintage);
  Serial.print("Position: ");
  Serial.println(bottles[removedBottleIndex].position);
  Serial.print("Empty position weight: ");
  Serial.print(emptyWeight);
  Serial.println("g");
  Serial.print("Last interaction: ");
  Serial.println(bottles[removedBottleIndex].lastInteraction);
  Serial.println("\nBottle has been marked as removed from the fridge.");
  Serial.println("When this bottle is returned, place it in the same position.");
  
  // Send bottle info and status to display
  snprintf(statusMsg, sizeof(statusMsg), 
           "BOTTLE UNLOADED: %s (%s %s)\nPosition: %d\nBottle marked as removed.", 
           bottles[removedBottleIndex].name.c_str(), 
           bottles[removedBottleIndex].type.c_str(),
           bottles[removedBottleIndex].vintage.c_str(),
           bottles[removedBottleIndex].position);
  sendStatusUpdateToDisplay(statusMsg);
  
  // Send bottle info using the dedicated function as well
  sendBottleInfoToDisplay(removedBottleIndex);
  
  Serial.println("\nPress any key to return to the main menu...");
  
  // Wait for key press
  while (Serial.available() == 0) {
    delay(100);
  }
  Serial.read(); // Clear the input buffer
}

// MENU 0 CONFIGURACIÓN/TESTEO
void configurationAndTestingMenu() {
  Serial.println("\nMode 0: Configuration and Testing");
  Serial.println("1 - Reset database to initial values");
  Serial.println("2 - Empty database to 0");
  Serial.println("3 - Test LEDs");
  Serial.println("4 - Show ESP32 MAC Address");
  Serial.println("5 - Calibrate Scale to 0 (tare)");
  Serial.println("0 - Return to main menu");
  
  // Send config menu to display
  sendStatusUpdateToDisplay("Configuration Menu:\n1-Reset DB\n2-Empty DB\n3-Test LEDs\n4-Show MAC\n0-Return");

  while (true) {
    if (Serial.available() > 0) {
      int option = readIntFromSerial();  // Leer la opción del usuario

      switch (option) {
        case 0:
          sendStatusUpdateToDisplay("Returning to main menu");
          return;
        case 1:
          sendStatusUpdateToDisplay("Resetting database to initial values...");
          initializeDefaultDatabase();  // Función para resetear la base de datos
          sendStatusUpdateToDisplay("Database reset to default values");
          break;
        case 2:
          sendStatusUpdateToDisplay("Emptying database...");
          emptyDatabase();  // Función para vaciar la base de datos
          sendStatusUpdateToDisplay("Database emptied");
          break;
        case 3:
          sendStatusUpdateToDisplay("Testing LEDs...");
          testLEDs();  // Función para probar los LEDs
          sendStatusUpdateToDisplay("LED test completed");
          break;
        case 4:
          // Show MAC Address for ESP-NOW setup
          printMacAddress();
          char macMsg[250];
          snprintf(macMsg, sizeof(macMsg), "ESP32 MAC Address: %s", WiFi.macAddress().c_str());
          sendStatusUpdateToDisplay(macMsg);
          break;
        case 5:
          sendStatusUpdateToDisplay("Calibrating scale to zero...");
          tareScale();
          sendStatusUpdateToDisplay("Scale calibration completed!");
          break;
        default:
          Serial.println("Invalid option. Please try again.");
          sendErrorToDisplay("Invalid configuration option");
          break;
      }
    }
    delay(100);
  }
}

#endif // MENU_FUNCTIONS_H