/**
 * CommunicationManager.cpp
 * Implementation of ESP-NOW communication functions
 */

#include "CommunicationManager.h"
#include "DisplayManager.h"
#include "BottleManager.h"  // Include the full definition, not just forward declaration
#include <WiFi.h>  // Include WiFi for ESP-NOW
#include <esp_now.h>

// Declare reference to the display manager
extern DisplayManager* displayManager;
// Declare reference to the bottle manager at global scope
extern BottleManager bottleManager;

// Define the global instance
CommunicationManager commManager;

// Constructor
CommunicationManager::CommunicationManager() {
  // Initialize data structures
  memset(&incomingData, 0, sizeof(struct_message));
  memset(&simpleData, 0, sizeof(simple_message));
  memset(&bottleInfo, 0, sizeof(bottle_info_message));
  
  // Initialize buffer
  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    messageBuffer[i].used = false;
  }
  bufferIndex = 0;
  processingBuffer = false;
}

// Initialize ESP-NOW
bool CommunicationManager::initESPNow() {
  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }
  
  // Register callback function for received data
  esp_now_register_recv_cb(staticOnDataRecv);
  
  Serial.println("ESP-NOW initialized successfully");
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  return true;
}

// Static callback function (to be registered with ESP-NOW)
void CommunicationManager::staticOnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingBuf, int len) {
  // Buffer the message for later processing to avoid blocking callback
  commManager.bufferMessage(incomingBuf, len);
}

// Buffer the received message
void CommunicationManager::bufferMessage(const uint8_t *data, size_t length) {
  if (length > MAX_MESSAGE_SIZE) {
    Serial.println("Message too large for buffer");
    return;
  }
  
  // Find a free buffer slot
  int index = bufferIndex;
  int attempts = 0;
  
  while (messageBuffer[index].used && attempts < MAX_BUFFER_SIZE) {
    index = (index + 1) % MAX_BUFFER_SIZE;
    attempts++;
  }
  
  if (attempts >= MAX_BUFFER_SIZE) {
    Serial.println("No free buffer slots available - message lost");
    return;
  }
  
  // Copy message to buffer
  memcpy(messageBuffer[index].data, data, length);
  messageBuffer[index].length = length;
  messageBuffer[index].used = true;
  
  bufferIndex = (index + 1) % MAX_BUFFER_SIZE;
}

// Process any buffered messages
void CommunicationManager::processBufferedMessages() {
  if (processingBuffer) return; // Prevent reentrant calls
  
  processingBuffer = true;
  
  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    if (messageBuffer[i].used) {
      processIncomingData(messageBuffer[i].data, messageBuffer[i].length);
      messageBuffer[i].used = false;
    }
  }
  
  processingBuffer = false;
}

// Process incoming ESP-NOW data
void CommunicationManager::processIncomingData(const uint8_t *incomingBuf, int len) {
  // Check data type based on size
  if (len == sizeof(struct_message)) {
    // Regular message
    memcpy(&incomingData, incomingBuf, sizeof(struct_message));
    Serial.print("Full message received: ");
    Serial.println(incomingData.messageType);
    
    // Update display based on message type
    if (displayManager != NULL) {
      displayManager->updateDisplay(incomingData.messageType);
    }
  } 
  else if (len == sizeof(simple_message)) {
    // Simple message (status, error)
    memcpy(&simpleData, incomingBuf, sizeof(simple_message));
    Serial.print("Simple message received: ");
    Serial.println(simpleData.messageType);
    
    // Copy to the main structure for display
    incomingData.messageType = simpleData.messageType;
    strncpy(incomingData.trayID, simpleData.trayID, sizeof(simpleData.trayID));
    strncpy(incomingData.text, simpleData.text, sizeof(simpleData.text));
    incomingData.bottleCount = simpleData.bottleCount;
    
    // Update display
    if (displayManager != NULL) {
      displayManager->updateDisplay(incomingData.messageType);
    }
  }
  else if (len == sizeof(bottle_info_message)) {
    // Bottle info message
    memcpy(&bottleInfo, incomingBuf, sizeof(bottle_info_message));
    Serial.print("Bottle info message received for bottle index ");
    Serial.println(bottleInfo.bottleIndex);
    
    // Copy to the main structure for display
    incomingData.messageType = bottleInfo.messageType;
    strncpy(incomingData.trayID, bottleInfo.trayID, sizeof(bottleInfo.trayID));
    
    // Update bottle data - No local extern declaration needed
    bottleManager.updateBottleFromMessage(bottleInfo);
    
    // Log this as an automatic update if appropriate
    static unsigned long lastUserAction = 0;
    unsigned long currentTime = millis();
    if (currentTime - lastUserAction > 15000) { // If no user action in 15 seconds
      Serial.println("Automatic update received");
    }
    
    // Update display
    if (displayManager != NULL) {
      displayManager->updateDisplay(incomingData.messageType);
    }
  }
  else {
    Serial.print("Unknown message format. Size: ");
    Serial.println(len);
  }
}