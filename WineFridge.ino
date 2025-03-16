// ==========================================================
//                WINE FRIDGE MAIN
// ==========================================================

#include <Arduino.h>
#include "Config.h"
#include "BottleDatabase.h"
#include "WineCatalog.h"
#include "SensorFunctions.h"
#include "ButtonFunctions.h"
#include "UtilityFunctions.h"
#include "MenuFunctions.h"
#include "DisplayFunctions.h"

// Define hardware pins
#define BUTTON_SCAN_PIN 2
#define BUTTON_MENU_PIN 3
#define BUTTON_SELECT_PIN 4
#define LCD_RS_PIN 8
#define LCD_EN_PIN 9
#define LCD_D4_PIN 10
#define LCD_D5_PIN 11
#define LCD_D6_PIN 12
#define LCD_D7_PIN 13

// Menu state variables
int currentMenu = 0;
const int MENU_COUNT = 5;
String menuOptions[MENU_COUNT] = {
  "1. Scan & Place",
  "2. Show Database",
  "3. Remove Bottle",
  "4. Update Weight",
  "5. System Settings"
};

// Function prototypes for menu options not defined in MenuFunctions.h
void removeBottle();
void updateBottleWeight();
void systemSettings();

void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  Serial.println("\n=== Wine Fridge Management System ===");
  
  // Initialize pins
  pinMode(BUTTON_SCAN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_MENU_PIN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT_PIN, INPUT_PULLUP);
  
  // Initialize display
  initializeDisplay();
  
  // Initialize weight sensors
  initializeWeightSensors();
  
  // Initialize barcode scanner (if applicable)
  initializeBarcodeScanner();
  
  // Load bottle database from EEPROM or SD card
  loadDatabase();
  
  // Load wine catalog
  loadWineCatalog();
  
  // Display welcome message
  displayWelcomeMessage();
  
  // Initial weight check of all positions
  checkAllBottleWeights();
  
  Serial.println("System initialized and ready.");
  displayMenu();
}

void loop() {
  // Check button inputs
  if (isButtonPressed(BUTTON_MENU_PIN)) {
    // Cycle through menu options
    currentMenu = (currentMenu + 1) % MENU_COUNT;
    displayMenu();
    delay(300); // Debounce
  }
  
  if (isButtonPressed(BUTTON_SELECT_PIN)) {
    // Execute the selected menu option
    executeMenuOption(currentMenu);
    delay(300); // Debounce
    displayMenu();
  }
  
  if (isButtonPressed(BUTTON_SCAN_PIN)) {
    // Quick shortcut to scan bottle function
    scanBottleAndPlace();
    delay(300); // Debounce
    displayMenu();
  }
  
  // Monitor weight sensors for changes (bottle removal detection)
  checkWeightChanges();
  
  // Process any serial commands if available
  processSerialCommands();
  
  delay(100); // Small delay to prevent CPU hogging
}

// Display the current menu on LCD and Serial
void displayMenu() {
  clearDisplay();
  Serial.println("\n=== MAIN MENU ===");
  
  for (int i = 0; i < MENU_COUNT; i++) {
    if (i == currentMenu) {
      Serial.print("> "); // Current selection indicator
      displayTextLine(i, "> " + menuOptions[i]);
    } else {
      Serial.print("  ");
      displayTextLine(i, "  " + menuOptions[i]);
    }
    Serial.println(menuOptions[i]);
  }
  
  Serial.println("\nUse MENU button to navigate, SELECT to choose option");
}

// Execute the selected menu option
void executeMenuOption(int option) {
  Serial.print("\nExecuting menu option: ");
  Serial.println(menuOptions[option]);
  
  switch (option) {
    case 0:
      scanBottleAndPlace(); // Defined in MenuFunctions.h
      break;
    case 1:
      showBottleDatabase(); // Defined in MenuFunctions.h
      break;
    case 2:
      removeBottle();
      break;
    case 3:
      updateBottleWeight();
      break;
    case 4:
      systemSettings();
      break;
    default:
      Serial.println("Invalid menu option");
      break;
  }
}

// Menu 3 - Remove Bottle
void removeBottle() {
  Serial.println("\nMode 3: Remove Bottle");
  
  // Ask which bottle to remove
  Serial.println("Enter bottle position (1-" + String(BOTTLE_COUNT) + "):");
  
  // Wait for input (either from serial or buttons)
  int position = -1;
  while (position < 1 || position > BOTTLE_COUNT) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      position = input.toInt();
    }
    
    // Allow navigation with buttons as an alternative
    if (isButtonPressed(BUTTON_MENU_PIN)) {
      // Implementation for button-based selection would go here
      delay(200);
    }
    
    delay(100);
  }
  
  // Find the bottle in that position
  int bottleIndex = -1;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].position == position && bottles[i].inFridge) {
      bottleIndex = i;
      break;
    }
  }
  
  if (bottleIndex == -1) {
    Serial.println("No bottle found at position " + String(position));
    return;
  }
  
  // Show bottle details and confirm removal
  Serial.print("Bottle at position ");
  Serial.print(position);
  Serial.print(": ");
  Serial.println(bottles[bottleIndex].name);
  Serial.println("Remove bottle and press SELECT to confirm");
  
  // Wait for confirmation (either button press or bottle weight change)
  bool bottleRemoved = false;
  while (!bottleRemoved) {
    // Check if bottle was physically removed
    float currentWeight = getPositionWeight(position);
    if (currentWeight < bottles[bottleIndex].weight * 0.5) {
      // Significant weight decrease indicates removal
      bottleRemoved = true;
    }
    
    // Check for button confirmation
    if (isButtonPressed(BUTTON_SELECT_PIN)) {
      bottleRemoved = true;
    }
    
    delay(100);
  }
  
  // Update bottle status
  bottles[bottleIndex].inFridge = false;
  bottles[bottleIndex].lastInteraction = getTimeStamp();
  
  Serial.println("Bottle removed from database");
  saveDatabase();
}

// Menu 4 - Update Bottle Weight
void updateBottleWeight() {
  Serial.println("\nMode 4: Update Bottle Weight");
  
  // Ask which bottle to update
  Serial.println("Enter bottle position (1-" + String(BOTTLE_COUNT) + "):");
  
  int position = -1;
  while (position < 1 || position > BOTTLE_COUNT) {
    if (Serial.available() > 0) {
      String input = Serial.readStringUntil('\n');
      position = input.toInt();
    }
    delay(100);
  }
  
  // Find the bottle in that position
  int bottleIndex = -1;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].position == position && bottles[i].inFridge) {
      bottleIndex = i;
      break;
    }
  }
  
  if (bottleIndex == -1) {
    Serial.println("No bottle found at position " + String(position));
    return;
  }
  
  // Show current weight
  Serial.print("Current weight for ");
  Serial.print(bottles[bottleIndex].name);
  Serial.print(": ");
  Serial.print(bottles[bottleIndex].weight);
  Serial.println("g");
  
  // Measure new weight
  Serial.println("Measuring new weight...");
  float newWeight = measureWeight();
  
  // Update bottle weight
  bottles[bottleIndex].weight = newWeight;
  bottles[bottleIndex].lastInteraction = getTimeStamp();
  
  Serial.print("New weight recorded: ");
  Serial.println(newWeight);
  
  saveDatabase();
}

// Menu 5 - System Settings
void systemSettings() {
  Serial.println("\nMode 5: System Settings");
  
  // Define submenu options
  const int SETTINGS_COUNT = 4;
  String settingsOptions[SETTINGS_COUNT] = {
    "1. Calibrate Weight Sensors",
    "2. Reset Database",
    "3. Export Database",
    "4. Return to Main Menu"
  };
  
  int currentSetting = 0;
  bool inSettingsMenu = true;
  
  // Display settings menu
  while (inSettingsMenu) {
    Serial.println("\n=== SETTINGS MENU ===");
    for (int i = 0; i < SETTINGS_COUNT; i++) {
      if (i == currentSetting) {
        Serial.print("> ");
      } else {
        Serial.print("  ");
      }
      Serial.println(settingsOptions[i]);
    }
    
    // Wait for button input
    while (!isButtonPressed(BUTTON_MENU_PIN) && !isButtonPressed(BUTTON_SELECT_PIN)) {
      delay(100);
    }
    
    if (isButtonPressed(BUTTON_MENU_PIN)) {
      // Navigate through settings
      currentSetting = (currentSetting + 1) % SETTINGS_COUNT;
      delay(300); // Debounce
    }
    
    if (isButtonPressed(BUTTON_SELECT_PIN)) {
      // Execute selected setting
      switch (currentSetting) {
        case 0:
          calibrateWeightSensors();
          break;
        case 1:
          resetDatabase();
          break;
        case 2:
          exportDatabase();
          break;
        case 3:
          inSettingsMenu = false;
          break;
      }
      delay(300); // Debounce
    }
  }
}

// Process commands from Serial
void processSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command.startsWith("scan")) {
      scanBottleAndPlace();
    } else if (command.startsWith("show")) {
      showBottleDatabase();
    } else if (command.startsWith("remove")) {
      // Extract position number if provided
      int pos = -1;
      if (command.length() > 7) {
        pos = command.substring(7).toInt();
      }
      
      if (pos > 0 && pos <= BOTTLE_COUNT) {
        // Set position and call remove function
        for (int i = 0; i < BOTTLE_COUNT; i++) {
          if (bottles[i].position == pos) {
            Serial.print("Removing bottle at position ");
            Serial.println(pos);
            bottles[i].inFridge = false;
            bottles[i].lastInteraction = getTimeStamp();
            saveDatabase();
            return;
          }
        }
        Serial.println("No bottle found at position " + String(pos));
      } else {
        removeBottle();
      }
    } else if (command.startsWith("update")) {
      updateBottleWeight();
    } else if (command.startsWith("settings")) {
      systemSettings();
    } else if (command.startsWith("help")) {
      Serial.println("\n=== AVAILABLE COMMANDS ===");
      Serial.println("scan - Scan and place a new bottle");
      Serial.println("show - Display bottle database");
      Serial.println("remove [position] - Remove a bottle");
      Serial.println("update - Update bottle weight");
      Serial.println("settings - System settings");
      Serial.println("help - Show this help message");
    } else {
      Serial.println("Unknown command. Type 'help' for available commands.");
    }
  }
}

// Weight sensor calibration
void calibrateWeightSensors() {
  Serial.println("\n=== Weight Sensor Calibration ===");
  Serial.println("Remove all bottles from the fridge");
  Serial.println("Press SELECT when ready");
  
  while (!isButtonPressed(BUTTON_SELECT_PIN)) {
    delay(100);
  }
  
  Serial.println("Calibrating zero points...");
  calibrateWeightSensorZero();
  
  Serial.println("Place a known weight (500g) at position 1");
  Serial.println("Press SELECT when ready");
  
  while (!isButtonPressed(BUTTON_SELECT_PIN)) {
    delay(100);
  }
  
  Serial.println("Calibrating reference weight...");
  calibrateWeightSensorReference(500.0);
  
  Serial.println("Calibration complete");
}

// Reset the bottle database
void resetDatabase() {
  Serial.println("\n=== Reset Database ===");
  Serial.println("This will delete all bottle data");
  Serial.println("Are you sure? Press SELECT to confirm, MENU to cancel");
  
  while (!isButtonPressed(BUTTON_SELECT_PIN) && !isButtonPressed(BUTTON_MENU_PIN)) {
    delay(100);
  }
  
  if (isButtonPressed(BUTTON_SELECT_PIN)) {
    initializeBottleDatabase();
    saveDatabase();
    Serial.println("Database reset complete");
  } else {
    Serial.println("Database reset cancelled");
  }
}

// Export the database through serial
void exportDatabase() {
  Serial.println("\n=== Database Export ===");
  Serial.println("Exporting data in CSV format...");
  
  // Print CSV header
  Serial.println("Position,Barcode,Name,Weight,InFridge,LastInteraction");
  
  // Print each bottle's data
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].name != "" && bottles[i].name != "Bottle " + String(i+1)) {
      Serial.print(bottles[i].position);
      Serial.print(",");
      Serial.print(bottles[i].barcode);
      Serial.print(",\"");
      Serial.print(bottles[i].name);
      Serial.print("\",");
      Serial.print(bottles[i].weight);
      Serial.print(",");
      Serial.print(bottles[i].inFridge ? "1" : "0");
      Serial.print(",");
      Serial.println(bottles[i].lastInteraction);
    }
  }
  
  Serial.println("Export complete");
}

// Check for weight changes in all positions to detect bottle removal
void checkWeightChanges() {
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      float currentWeight = getPositionWeight(bottles[i].position);
      
      // If weight has significantly decreased (bottle removed)
      if (currentWeight < bottles[i].weight * 0.5) {
        Serial.print("Bottle removed from position ");
        Serial.print(bottles[i].position);
        Serial.print(": ");
        Serial.println(bottles[i].name);
        
        bottles[i].inFridge = false;
        bottles[i].lastInteraction = getTimeStamp();
        saveDatabase();
      }
    }
  }
}

// Check all bottle weights on startup
void checkAllBottleWeights() {
  Serial.println("Checking all bottle positions...");
  
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    float weight = getPositionWeight(i + 1);
    
    // If significant weight detected but bottle marked as not in fridge
    if (weight > 100.0 && !bottles[i].inFridge) {
      Serial.print("Bottle detected at position ");
      Serial.println(i + 1);
      
      // If this position is associated with a known bottle
      if (bottles[i].position == i + 1 && bottles[i].name != "" && 
          bottles[i].name != "Bottle " + String(i+1)) {
        bottles[i].inFridge = true;
        bottles[i].weight = weight;
        bottles[i].lastInteraction = getTimeStamp();
      } else {
        // Create a temporary bottle entry
        bottles[i].barcode = String(i+1) + "000000";
        bottles[i].name = "Bottle " + String(i+1);
        bottles[i].position = i + 1;
        bottles[i].weight = weight;
        bottles[i].inFridge = true;
        bottles[i].lastInteraction = getTimeStamp();
      }
    }
    // If no significant weight but bottle marked as in fridge
    else if (weight < 50.0 && bottles[i].inFridge) {
      Serial.print("Bottle missing from position ");
      Serial.println(i + 1);
      bottles[i].inFridge = false;
    }
  }
  
  saveDatabase();
}