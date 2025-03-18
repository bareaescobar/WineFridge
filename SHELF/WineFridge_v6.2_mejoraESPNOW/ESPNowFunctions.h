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

// Added constants for reliable communication
#define ESP_NOW_SEND_DELAY 150    // Delay between sends in ms
#define ESP_NOW_MAX_RETRIES 3     // Maximum retry attempts
#define ESP_NOW_RETRY_DELAY 50    // Base delay between retries (increases with each retry)

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
// Must match the receiver structure
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

// Tracking variable for ESP-NOW success
volatile bool lastSendSuccess = false;

// Forward declarations
void sendBottleInfoToDisplay(int bottleIndex);

// New function to send data with retries
bool sendESPNowData(const uint8_t* data, size_t size) {
  bool success = false;
  int attempts = 0;
  
  while (!success && attempts < ESP_NOW_MAX_RETRIES) {
    lastSendSuccess = false; // Reset before sending
    esp_err_t result = esp_now_send(receiverMacAddress, data, size);
    
    if (result == ESP_OK) {
      // Wait a bit for the callback to set lastSendSuccess
      unsigned long startTime = millis();
      while (millis() - startTime < 50) {
        delay(1); // Small delay to allow callback to set the status
      }
      
      if (lastSendSuccess) {
        success = true;
      } else {
        Serial.print("Transmission attempt ");
        Serial.print(attempts + 1);
        Serial.println(" - Callback reported failure");
      }
    } else {
      Serial.print("Send error on attempt ");
      Serial.print(attempts + 1);
      Serial.print(": ");
      Serial.println(result);
    }
    
    if (!success) {
      attempts++;
      // Exponential backoff delay
      delay(ESP_NOW_RETRY_DELAY * attempts);
    }
  }
  
  return success;
}

// Función de prueba para enviar un mensaje simple
bool sendSimpleTestMessage() {
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 3; // Status
  strcpy(simpleData.text, "Test message from tray");
  simpleData.bottleCount = 0;
  
  bool success = sendESPNowData((uint8_t*)&simpleData, sizeof(simpleData));
  if (success) {
    Serial.println("Test message sent successfully");
  } else {
    Serial.println("Error sending test message after retries");
  }
  
  delay(ESP_NOW_SEND_DELAY); // Add delay after sending
  return success;
}

// Callback function called when data is sent
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
  // Inicialización básica de ESP-NOW
  esp_err_t initResult = esp_now_init();
  if (initResult != ESP_OK) {
    Serial.print("Error initializing ESP-NOW: ");
    Serial.println(initResult);
    return;
  }
  
  Serial.println("ESP-NOW initialized");
  
  // Registrar callback de envío
  esp_now_register_send_cb(OnDataSent);
  
  // Intentar eliminar cualquier peer antiguo primero (por seguridad)
  esp_now_del_peer(receiverMacAddress);
  
  // Configurar peer
  esp_now_peer_info_t peerInfo;
  memset(&peerInfo, 0, sizeof(peerInfo));
  memcpy(peerInfo.peer_addr, receiverMacAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Agregar peer con manejo de errores mejorado
  esp_err_t result = esp_now_add_peer(&peerInfo);
  if (result != ESP_OK) {
    Serial.print("Failed to add peer. Error code: ");
    Serial.println(result);
    
    // Imprimir la MAC que estamos intentando usar
    Serial.print("Trying to add peer with MAC: ");
    for (int i = 0; i < 6; i++) {
      Serial.print(receiverMacAddress[i], HEX);
      if (i < 5) Serial.print(":");
    }
    Serial.println();
    
    return;
  }
  
  Serial.println("Peer added successfully");
  
  // Enviar un mensaje de prueba simple para verificar la conexión
  delay(1000);
  sendSimpleTestMessage();
}

// Get the MAC address of this device (for setup)
void printMacAddress() {
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
}

// Send the current menu to the display - using simple structure for reliability
void sendMenuToDisplay() {
  // Initialize simple message structure
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 0; // Menu
  
  // Format simple menu text
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
  
  // Send simple message via ESP-NOW with retry
  bool success = sendESPNowData((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (success) {
    Serial.println("Menu sent to display");
  } else {
    Serial.println("Error sending menu to display after retries");
  }
  
  delay(ESP_NOW_SEND_DELAY); // Add delay after sending
}

// Send information about a specific bottle - updated for reliability
void sendBottleInfoToDisplay(int bottleIndex) {
  if (bottleIndex < 0 || bottleIndex >= BOTTLE_COUNT) return;
  
  // Initialize bottle info message
  memset(&bottleInfoMsg, 0, sizeof(bottleInfoMsg));
  strcpy(bottleInfoMsg.trayID, "Tray001");
  bottleInfoMsg.messageType = 2; // Bottle Info
  bottleInfoMsg.bottleIndex = bottleIndex;
  bottleInfoMsg.bottlePosition = bottles[bottleIndex].position;
  
  // Copy all bottle data
  strncpy(bottleInfoMsg.barcode, bottles[bottleIndex].barcode.c_str(), sizeof(bottleInfoMsg.barcode) - 1);
  strncpy(bottleInfoMsg.name, bottles[bottleIndex].name.c_str(), sizeof(bottleInfoMsg.name) - 1);
  strncpy(bottleInfoMsg.type, bottles[bottleIndex].type.c_str(), sizeof(bottleInfoMsg.type) - 1);
  strncpy(bottleInfoMsg.region, bottles[bottleIndex].region.c_str(), sizeof(bottleInfoMsg.region) - 1);
  strncpy(bottleInfoMsg.vintage, bottles[bottleIndex].vintage.c_str(), sizeof(bottleInfoMsg.vintage) - 1);
  strncpy(bottleInfoMsg.lastInteraction, bottles[bottleIndex].lastInteraction.c_str(), sizeof(bottleInfoMsg.lastInteraction) - 1);
  
  bottleInfoMsg.weight = bottles[bottleIndex].weight;
  bottleInfoMsg.inFridge = bottles[bottleIndex].inFridge;
  
  // Send bottle info message via ESP-NOW with retry
  bool success = sendESPNowData((uint8_t*)&bottleInfoMsg, sizeof(bottleInfoMsg));
  
  if (success) {
    Serial.print("Bottle info sent to display for bottle ");
    Serial.print(bottleIndex);
    Serial.print(": ");
    Serial.println(bottles[bottleIndex].name);
  } else {
    Serial.print("Error sending bottle info to display for bottle ");
    Serial.print(bottleIndex);
    Serial.print(": ");
    Serial.println(bottles[bottleIndex].name);
  }
  
  delay(ESP_NOW_SEND_DELAY); // Add delay after sending
}

// Send the full bottle database to the display - updated for reliability
void sendBottleDatabaseToDisplay() {
  // First, send the header information
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 1; // Bottle Database
  
  // Format header text
  snprintf(simpleData.text, sizeof(simpleData.text), "WINE FRIDGE INVENTORY");
  
  // Count bottles in fridge
  int count = 0;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      count++;
    }
  }
  simpleData.bottleCount = count;
  
  // Send simple message via ESP-NOW with retry
  bool success = sendESPNowData((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (!success) {
    Serial.println("Error sending bottle database header to display after retries");
    return;
  }
  
  Serial.println("Bottle database header sent, now sending individual bottles...");
  delay(ESP_NOW_SEND_DELAY); // Added larger delay between header and first bottle
  
  // Now send individual bottle information for each bottle one by one
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].barcode != "") {  // Only send bottles with valid data
      sendBottleInfoToDisplay(i);
      delay(ESP_NOW_SEND_DELAY); // Already has delay in sendBottleInfoToDisplay, but added here too for clarity
    }
  }
  
  Serial.println("All bottle information sent to display");
}

// Send a status update message - updated for reliability
void sendStatusUpdateToDisplay(const char* message) {
  // Initialize simple message
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 3; // Status Update
  
  // Copy the message (truncated if needed)
  strncpy(simpleData.text, message, sizeof(simpleData.text) - 1);
  
  // Count bottles in fridge
  int count = 0;
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].inFridge) {
      count++;
    }
  }
  simpleData.bottleCount = count;
  
  // Send simple message via ESP-NOW with retry
  bool success = sendESPNowData((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (success) {
    Serial.println("Status update sent to display");
  } else {
    Serial.println("Error sending status update to display after retries");
  }
  
  delay(ESP_NOW_SEND_DELAY); // Add delay after sending
}

// Send an error message - updated for reliability
void sendErrorToDisplay(const char* errorMessage) {
  // Initialize simple message
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 4; // Error
  
  // Copy the error message (truncated if needed)
  strncpy(simpleData.text, errorMessage, sizeof(simpleData.text) - 1);
  
  // Send simple message via ESP-NOW with retry
  bool success = sendESPNowData((uint8_t*)&simpleData, sizeof(simpleData));
  
  if (success) {
    Serial.println("Error message sent to display");
  } else {
    Serial.println("Error sending error message to display after retries");
  }
  
  delay(ESP_NOW_SEND_DELAY); // Add delay after sending
}

#endif // ESP_NOW_FUNCTIONS_H