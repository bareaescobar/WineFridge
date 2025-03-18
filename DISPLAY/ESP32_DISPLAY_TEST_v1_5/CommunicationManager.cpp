/**
 * CommunicationManager.cpp
 * Implementation of ESP-NOW communication functions
 */

#include "CommunicationManager.h"
#include "DisplayManager.h"
#include "BottleManager.h"
#include <WiFi.h>
#include <esp_now.h>

// References to external objects
extern DisplayManager* displayManager;
extern BottleManager bottleManager;

// Global instance
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
  // Ensure proper WiFi mode
  WiFi.mode(WIFI_STA);
  delay(100);
  
  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return false;
  }
  
  // Register callback
  esp_now_register_recv_cb(staticOnDataRecv);
  
  Serial.println("ESP-NOW initialized successfully");
  Serial.print("MAC Address: ");
  Serial.println(WiFi.macAddress());
  
  return true;
}

// Static callback function
void CommunicationManager::staticOnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingBuf, int len) {
  // Buffer message for later processing
  commManager.bufferMessage(incomingBuf, len);
}

// Buffer a message
void CommunicationManager::bufferMessage(const uint8_t *data, size_t length) {
  if (length > MAX_MESSAGE_SIZE) {
    Serial.println("Message too large for buffer");
    return;
  }
  
  // Find free slot
  int slot = -1;
  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    if (!messageBuffer[i].used) {
      slot = i;
      break;
    }
  }
  
  // If no free slot, overwrite oldest
  if (slot == -1) {
    unsigned long oldest = ULONG_MAX;
    for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
      if (messageBuffer[i].timestamp < oldest) {
        oldest = messageBuffer[i].timestamp;
        slot = i;
      }
    }
    Serial.println("Buffer full, overwriting oldest message");
  }
  
  // Copy message
  memcpy(messageBuffer[slot].data, data, length);
  messageBuffer[slot].length = length;
  messageBuffer[slot].used = true;
  messageBuffer[slot].timestamp = millis();
  
  Serial.print("Message buffered in slot ");
  Serial.println(slot);
}

// Process buffered messages
void CommunicationManager::processBufferedMessages() {
  if (processingBuffer) return;
  
  processingBuffer = true;
  
  for (int i = 0; i < MAX_BUFFER_SIZE; i++) {
    if (messageBuffer[i].used) {
      processIncomingData(messageBuffer[i].data, messageBuffer[i].length);
      messageBuffer[i].used = false;
    }
  }
  
  processingBuffer = false;
}

// Process incoming data
void CommunicationManager::processIncomingData(const uint8_t *incomingBuf, int len) {
  // Check message type by size
  if (len == sizeof(struct_message)) {
    // Full message
    memcpy(&incomingData, incomingBuf, sizeof(struct_message));
    Serial.print("Full message received: ");
    Serial.println(incomingData.messageType);
    
    // Update display
    if (displayManager != NULL) {
      displayManager->updateDisplay(incomingData.messageType);
    }
  } 
  else if (len == sizeof(simple_message)) {
    // Simple message
    memcpy(&simpleData, incomingBuf, sizeof(simple_message));
    Serial.print("Simple message received: ");
    Serial.println(simpleData.messageType);
    
    // Copy to main structure
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
    // Bottle info
    memcpy(&bottleInfo, incomingBuf, sizeof(bottle_info_message));
    Serial.print("Bottle info received: ");
    Serial.println(bottleInfo.bottleIndex);
    
    // Copy to main structure
    incomingData.messageType = bottleInfo.messageType;
    strncpy(incomingData.trayID, bottleInfo.trayID, sizeof(bottleInfo.trayID));
    
    // Update bottle data
    bottleManager.updateBottleFromMessage(bottleInfo);
    
    // Update display
    if (displayManager != NULL) {
      displayManager->updateDisplay(incomingData.messageType);
    }
  }
  else {
    Serial.print("Unknown message format: ");
    Serial.println(len);
  }
}