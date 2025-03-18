// ==========================================================
//              ESP-NOW COMMUNICATION FUNCTIONS
// ==========================================================
#ifndef ESP_NOW_FUNCTIONS_H
#define ESP_NOW_FUNCTIONS_H

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "BottleDatabase.h"
#include "Config.h"

// Retry configuration
#define ESP_NOW_MAX_RETRIES 3
#define ESP_NOW_RETRY_DELAY 100
#define ESP_NOW_SEND_DELAY 200

// Message structures
typedef struct simple_message {
  int messageType;     // 0: Menu, 1: Bottle DB, 2: Bottle Info, 3: Status, 4: Error
  char trayID[10];     // Identificador único para cada bandeja
  char text[100];      // Texto reducido
  int bottleCount;     // Número de botellas
} simple_message;

// Structure for sending a single bottle's info
typedef struct bottle_info_message {
  int messageType;     // Always 2: Bottle Info
  char trayID[10];     // Identificador único para cada bandeja
  int bottleIndex;     // Index of this bottle in the database
  int bottlePosition;  // Position of this bottle (1-9)
  char barcode[20];    // Barcode
  char name[50];       // Name
  char type[20];       // Type
  char region[20];     // Region
  char vintage[10];    // Vintage year
  float weight;        // Weight
  char lastInteraction[30]; // Last interaction time
  bool inFridge;       // Whether the bottle is in the fridge
} bottle_info_message;

simple_message simpleData;
bottle_info_message bottleInfoMsg;

// Structure to send data - main structure
typedef struct struct_message {
  int messageType;     // 0: Menu, 1: Bottle DB, 2: Bottle Info, 3: Status, 4: Error
  char trayID[10];     // Identificador único para cada bandeja (ej: "Tray001")
  char text[250];      // Mensaje principal
  int bottleCount;     // Número de botellas en la nevera
  Bottle bottles[BOTTLE_COUNT]; // Array de datos de botellas
} struct_message;

// Create a structured object
struct_message wineData;

// MAC Address of receiver ESP32 (7-inch display)
uint8_t receiverMacAddress[] = {0x3C, 0x84, 0x27, 0xFF, 0x50, 0x98};

// Track send success
volatile bool lastSendSuccess = false;

// Forward declarations
void sendBottleInfoToDisplay(int bottleIndex);

// Safe send function with retries
bool safeSendESPNow(const uint8_t* data, size_t length) {
  // Ensure WiFi is in proper mode and ready
  if (WiFi.getMode() != WIFI_STA) {
    WiFi.mode(WIFI_STA);
    delay(100);
  }
  
  for (int attempt = 0; attempt < ESP_NOW_MAX_RETRIES; attempt++) {
    lastSendSuccess = false;
    
    esp_err_t result = esp_now_send(receiverMacAddress, data, length);
    if (result == ESP_OK) {
      // Wait for callback to set status
      delay(50);
      
      if (lastSendSuccess) {
        return true;
      }
    }
    
    Serial.print("Send attempt ");
    Serial.print(attempt + 1);
    Serial.println(" failed");
    
    delay(ESP_NOW_RETRY_DELAY * (attempt + 1));
  }
  
  return false;
}

// Send test message
bool sendSimpleTestMessage() {
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 3; // Status
  strcpy(simpleData.text, "Test message from tray");
  simpleData.bottleCount = 0;
  
  bool success = safeSendESPNow((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (success) {
    Serial.println("Test message sent successfully");
  } else {
    Serial.println("Failed to send test message after retries");
  }
  
  delay(ESP_NOW_SEND_DELAY);
  return success;
}

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  lastSendSuccess = (status == ESP_NOW_SEND_SUCCESS);
  
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
          mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  
  Serial.print("ESP-NOW Send Status: ");
  if (status == ESP_NOW_SEND_SUCCESS) {
    Serial.print("Success to MAC: ");
  } else {
    Serial.print("Failed to MAC: ");
  }
  Serial.println(macStr);
}

// Initialize ESP-NOW communication
void setupESPNow() {
  // Clear WiFi connections but keep station mode
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  Serial.println("ESP-NOW initialized");
  
  // Register callback
  esp_now_register_send_cb(OnDataSent);
  
  // Remove old peers
  esp_now_del_peer(receiverMacAddress);
  
  // Add peer with proper configuration
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;  // Auto select channel
  peerInfo.encrypt = false;
  
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add ESP-NOW peer");
    Serial.print("MAC Address: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(receiverMacAddress[i], HEX);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    return;
  }
  
  Serial.println("ESP-NOW peer added successfully");
  delay(1000);
  
  // Send test message
  sendSimpleTestMessage();
}

// Get device MAC address
void printMacAddress() {
  Serial.print("This device MAC: ");
  Serial.println(WiFi.macAddress());
}

// Send menu to display
void sendMenuToDisplay() {
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 0; // Menu
  
  // Format menu text
  snprintf(simpleData.text, sizeof(simpleData.text), 
    "WINE FRIDGE MENU\n"
    "0-Config, 1-Scan, 2-DB\n"
    "3-Auto, 4-Swap, 6-Load\n"
    "7-Region, 8-Swap, 9-Return"
  );
  
  // Count bottles in fridge
  int count = 0;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      count++;
    }
  }
  simpleData.bottleCount = count;
  
  // Send message
  bool success = safeSendESPNow((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (success) {
    Serial.println("Menu sent to display");
  } else {
    Serial.println("Failed to send menu to display");
  }
  
  delay(ESP_NOW_SEND_DELAY);
}

// Send bottle info to display
void sendBottleInfoToDisplay(int bottleIndex) {
  if (bottleIndex < 0 || bottleIndex >= BOTTLE_COUNT) return;
  
  memset(&bottleInfoMsg, 0, sizeof(bottleInfoMsg));
  strcpy(bottleInfoMsg.trayID, "Tray001");
  bottleInfoMsg.messageType = 2; // Bottle Info
  bottleInfoMsg.bottleIndex = bottleIndex;
  bottleInfoMsg.bottlePosition = bottles[bottleIndex].position;
  
  // Copy bottle data
  strncpy(bottleInfoMsg.barcode, bottles[bottleIndex].barcode.c_str(), sizeof(bottleInfoMsg.barcode) - 1);
  strncpy(bottleInfoMsg.name, bottles[bottleIndex].name.c_str(), sizeof(bottleInfoMsg.name) - 1);
  strncpy(bottleInfoMsg.type, bottles[bottleIndex].type.c_str(), sizeof(bottleInfoMsg.type) - 1);
  strncpy(bottleInfoMsg.region, bottles[bottleIndex].region.c_str(), sizeof(bottleInfoMsg.region) - 1);
  strncpy(bottleInfoMsg.vintage, bottles[bottleIndex].vintage.c_str(), sizeof(bottleInfoMsg.vintage) - 1);
  strncpy(bottleInfoMsg.lastInteraction, bottles[bottleIndex].lastInteraction.c_str(), sizeof(bottleInfoMsg.lastInteraction) - 1);
  
  bottleInfoMsg.weight = bottles[bottleIndex].weight;
  bottleInfoMsg.inFridge = bottles[bottleIndex].inFridge;
  
  // Send message
  bool success = safeSendESPNow((uint8_t*)&bottleInfoMsg, sizeof(bottleInfoMsg));
  
  if (success) {
    Serial.print("Bottle info sent for bottle ");
    Serial.print(bottleIndex);
    Serial.print(": ");
    Serial.println(bottles[bottleIndex].name);
  } else {
    Serial.print("Failed to send bottle info for bottle ");
    Serial.println(bottleIndex);
  }
  
  delay(ESP_NOW_SEND_DELAY);
}

// Send full database to display
void sendBottleDatabaseToDisplay() {
  // Send header first
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 1; // Bottle Database
  
  snprintf(simpleData.text, sizeof(simpleData.text), "WINE FRIDGE INVENTORY");
  
  // Count bottles in fridge
  int count = 0;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      count++;
    }
  }
  simpleData.bottleCount = count;
  
  // Send header message
  bool success = safeSendESPNow((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (!success) {
    Serial.println("Failed to send database header");
    return;
  }
  
  Serial.println("Database header sent, sending individual bottles...");
  delay(ESP_NOW_SEND_DELAY * 2); // Extra delay after header
  
  // Send each bottle separately
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].barcode != "") {  // Only send bottles with data
      sendBottleInfoToDisplay(i);
    }
  }
  
  Serial.println("All bottle information sent");
}

// Send status update
void sendStatusUpdateToDisplay(const char* message) {
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 3; // Status Update
  
  strncpy(simpleData.text, message, sizeof(simpleData.text) - 1);
  
  // Count bottles in fridge
  int count = 0;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      count++;
    }
  }
  simpleData.bottleCount = count;
  
  // Send message
  bool success = safeSendESPNow((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (success) {
    Serial.println("Status update sent to display");
  } else {
    Serial.println("Failed to send status update");
  }
  
  delay(ESP_NOW_SEND_DELAY);
}

// Send error message
void sendErrorToDisplay(const char* errorMessage) {
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 4; // Error
  
  strncpy(simpleData.text, errorMessage, sizeof(simpleData.text) - 1);
  
  // Send message
  bool success = safeSendESPNow((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (success) {
    Serial.println("Error message sent to display");
  } else {
    Serial.println("Failed to send error message");
  }
  
  delay(ESP_NOW_SEND_DELAY);
}

#endif // ESP_NOW_FUNCTIONS_H