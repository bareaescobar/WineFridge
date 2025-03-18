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

// Estructura simplificada para pruebas
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
  bool isEmpty;        // Whether this is an empty bottle slot
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
// Alternativa: probar con orden inverso si no funciona
// uint8_t receiverMacAddress[] = {0x98, 0x50, 0xFF, 0x27, 0x84, 0x3C};

// Forward declarations
void sendBottleInfoToDisplay(int bottleIndex);
void sendEmptyBottleInfoToDisplay(int position);

// Función de prueba para enviar un mensaje simple
bool sendSimpleTestMessage() {
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 3; // Status
  strcpy(simpleData.text, "Test message from tray");
  simpleData.bottleCount = 0;
  
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&simpleData, sizeof(simpleData));
  if (result != ESP_OK) {
    Serial.print("Error sending test message. Error code: ");
    Serial.println(result);
    return false;
  }
  Serial.println("Test message sent successfully");
  return true;
}

// Callback function called when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
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
  
  // Send simple message via ESP-NOW
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&simpleData, sizeof(simpleData));
  
  if (result != ESP_OK) {
    Serial.print("Error sending menu to display. Error code: ");
    Serial.println(result);
  } else {
    Serial.println("Menu sent to display");
  }
}

// Send information about a specific bottle - updated detailed version
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
  bottleInfoMsg.isEmpty = (bottles[bottleIndex].barcode == ""); // Check if this is an empty bottle
  
  // Send bottle info message via ESP-NOW
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&bottleInfoMsg, sizeof(bottleInfoMsg));
  
  if (result != ESP_OK) {
    Serial.print("Error sending bottle info to display. Error code: ");
    Serial.println(result);
  } else {
    Serial.print("Bottle info sent to display for bottle ");
    Serial.print(bottleIndex);
    Serial.print(": ");
    Serial.println(bottles[bottleIndex].name);
  }
}

// Send empty bottle info for a specific position
void sendEmptyBottleInfoToDisplay(int position) {
  // Initialize bottle info message
  memset(&bottleInfoMsg, 0, sizeof(bottleInfoMsg));
  strcpy(bottleInfoMsg.trayID, "Tray001");
  bottleInfoMsg.messageType = 2; // Bottle Info
  bottleInfoMsg.bottleIndex = position - 1; // Convert position (1-9) to index (0-8)
  bottleInfoMsg.bottlePosition = position;
  
  // Mark as empty
  bottleInfoMsg.isEmpty = true;
  bottleInfoMsg.inFridge = false;
  
  // Send bottle info message via ESP-NOW
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&bottleInfoMsg, sizeof(bottleInfoMsg));
  
  if (result != ESP_OK) {
    Serial.print("Error sending empty bottle info to display. Error code: ");
    Serial.println(result);
  } else {
    Serial.print("Empty bottle info sent to display for position ");
    Serial.println(position);
  }
}

// Send the full bottle database to the display - updated version
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
  
  // Send simple message via ESP-NOW
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&simpleData, sizeof(simpleData));
  
  if (result != ESP_OK) {
    Serial.print("Error sending bottle database header to display. Error code: ");
    Serial.println(result);
    return;
  }
  
  Serial.println("Database header sent, now sending individual bottles...");
  delay(50); // Small delay between messages
  
  // Now send individual bottle information for each position
  for (int position = 1; position <= BOTTLE_COUNT; position++) {
    // Find bottle at this position
    int bottleIndex = -1;
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      if (bottles[i].position == position) {
        bottleIndex = i;
        break;
      }
    }
    
    if (bottleIndex >= 0 && bottles[bottleIndex].barcode != "") {
      // Valid bottle found at this position
      sendBottleInfoToDisplay(bottleIndex);
    } else {
      // No valid bottle at this position, send empty slot info
      sendEmptyBottleInfoToDisplay(position);
    }
    
    delay(50); // Small delay between each message to avoid packet loss
  }
  
  Serial.println("All bottle information sent to display");
}

// Send a status update message - simplified version
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
  
  // Send simple message via ESP-NOW
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&simpleData, sizeof(simpleData));
  
  if (result != ESP_OK) {
    Serial.print("Error sending status update to display. Error code: ");
    Serial.println(result);
  } else {
    Serial.println("Status update sent to display");
  }
}

// Send an error message - simplified version
void sendErrorToDisplay(const char* errorMessage) {
  // Initialize simple message
  memset(&simpleData, 0, sizeof(simpleData));
  strcpy(simpleData.trayID, "Tray001");
  simpleData.messageType = 4; // Error
  
  // Copy the error message (truncated if needed)
  strncpy(simpleData.text, errorMessage, sizeof(simpleData.text) - 1);
  
  // Send simple message via ESP-NOW
  esp_err_t result = esp_now_send(receiverMacAddress, (uint8_t*)&simpleData, sizeof(simpleData));
  
  if (result != ESP_OK) {
    Serial.print("Error sending error message to display. Error code: ");
    Serial.println(result);
  } else {
    Serial.println("Error message sent to display");
  }
}

#endif // ESP_NOW_FUNCTIONS_H