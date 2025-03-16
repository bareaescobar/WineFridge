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

// MENU 1 - Scan Bottle and Place
void scanBottleAndPlace() {
  Serial.println("\nMode 1: Scan Bottle and Place");
  
  String barcode = scanBarcode();
  if (barcode.length() == 0) {
    Serial.println("No barcode scanned");
    return;
  }
  
  int bottleIndex = findBottleByBarcode(barcode);
  
  // Check if bottle is not in the database
  if (bottleIndex == -1) {
    Serial.println("Bottle not found in database");
    
    // Check if the wine is in our catalog
    WineInfo* wineInfo = findWineInCatalog(barcode);
    
    if (wineInfo != NULL) {
      // Wine found in catalog, find empty slot in database
      for (int i = 0; i < BOTTLE_COUNT; i++) {
        if (bottles[i].barcode == "" || bottles[i].barcode.startsWith(String(i+1) + "00000")) {
          bottleIndex = i;
          break;
        }
      }
      
      if (bottleIndex != -1) {
        // Add the wine to our database
        String formattedName = getFormattedWineName(barcode);
        bottles[bottleIndex].barcode = barcode;
        bottles[bottleIndex].name = formattedName;
        bottles[bottleIndex].position = bottleIndex + 1;
        Serial.print("Wine found in catalog: ");
        Serial.println(formattedName);
      } else {
        Serial.println("No empty slots in database");
        return;
      }
    } else {
      Serial.println("Wine not found in catalog. Please enter a name:");
      
      while (Serial.available() == 0) {
        delay(100);
      }
      
      String name = Serial.readStringUntil('\n');
      name.trim();
      
      // Find empty slot in database
      for (int i = 0; i < BOTTLE_COUNT; i++) {
        if (bottles[i].barcode == "" || bottles[i].barcode.startsWith(String(i+1) + "00000")) {
          bottleIndex = i;
          break;
        }
      }
      
      if (bottleIndex != -1) {
        // Add the new wine to database
        bottles[bottleIndex].barcode = barcode;
        bottles[bottleIndex].name = name;
        bottles[bottleIndex].position = bottleIndex + 1;
      } else {
        Serial.println("No empty slots in database");
        return;
      }
    }
  }
  
  Serial.print("Bottle found: ");
  Serial.println(bottles[bottleIndex].name);
  Serial.print("Place bottle at position: ");
  Serial.println(bottles[bottleIndex].position);
  
  String message = "Please place the bottle in position " + String(bottles[bottleIndex].position);
  waitForBottlePlacement(bottles[bottleIndex].position, message);
  
  Serial.println("Measuring weight...");
  float newWeight = measureWeight();
  
  bottles[bottleIndex].weight = newWeight;
  bottles[bottleIndex].lastInteraction = getTimeStamp();
  bottles[bottleIndex].inFridge = true;
  
  Serial.print("New weight recorded: ");
  Serial.println(newWeight);
  
  saveDatabase();
}

// MENU 2 - Show Bottle Database
void showBottleDatabase() {
  // Clear previous output with some newlines
  Serial.println("\n\n");
  
  // Print a decorative header using simple ASCII characters
  Serial.println("+----------------------------------------------------------------------+");
  Serial.println("|                           WINE FRIDGE INVENTORY                      |");
  Serial.println("+----------------------------------------------------------------------+");
  
  // Print column headers with alignment
  Serial.println("+----+----------------+--------------------------------------+---------+---------+");
  Serial.println("| ID | Position       | Wine Name                            | Weight  | Status  |");
  Serial.println("+----+----------------+--------------------------------------+---------+---------+");
  
  // Print each bottle's information in a table row
  for (int i = 0; i < BOTTLE_COUNT; i++) {
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
    Serial.print(i + 1);
    for (int j = String(i + 1).length(); j < 2; j++) Serial.print(" ");
    Serial.print(" | ");
    
    // Position column - ensure it's exactly 14 characters
    Serial.print(positionStr);
    for (int j = positionStr.length(); j < 14; j++) Serial.print(" ");
    Serial.print(" | ");
    
    // Name column - ensure it's exactly 36 characters
    Serial.print(displayName);
    for (int j = displayName.length(); j < 36; j++) Serial.print(" ");
    Serial.print(" | ");
    
    // Weight column - ensure it's exactly 7 characters
    String weightStr = String(bottle.weight, 1) + "g";
    Serial.print(weightStr);
    for (int j = weightStr.length(); j < 7; j++) Serial.print(" ");
    Serial.print(" | ");
    
    // Status column - ensure it's exactly 7 characters
    String statusStr = bottle.inFridge ? "IN" : "OUT";
    Serial.print(statusStr);
    for (int j = statusStr.length(); j < 7; j++) Serial.print(" ");
    Serial.println(" |");
  }
  
  // Close the table
  Serial.println("+----+----------------+--------------------------------------+---------+---------+");
  
  // Add detailed view section
  Serial.println();
  Serial.println("+----------------------------------------------------------------------+");
  Serial.println("|                              DETAILED VIEW                           |");
  Serial.println("+----------------------------------------------------------------------+");
  
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    Bottle& bottle = bottles[i];
    
    // Only show detailed info for bottles that are in the database (have names)
    if (bottle.name != "" && bottle.name != "Bottle " + String(i+1)) {
      Serial.println("+----------------------------------------------------------------------+");
      
      // Calculate total line width (68 characters between the borders)
      const int LINE_WIDTH = 68;
      
      // Format the ID and position
      String headerText = "#" + String(i + 1) + " - Position " + String(bottle.position) + " - " +
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
    }
  }
  
  Serial.println("+----------------------------------------------------------------------+");
}

#endif // MENU_FUNCTIONS_H