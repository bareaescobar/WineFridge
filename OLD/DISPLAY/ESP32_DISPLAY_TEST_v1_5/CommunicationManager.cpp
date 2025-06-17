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
  // Call the instance method to process the data
  commManager.processIncomingData(incomingBuf, len);
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
    
    if (bottleInfo.isEmpty) {
      Serial.print("Empty bottle position received for position ");
      Serial.println(bottleInfo.bottlePosition);
      
      // Update this position as empty in the bottle manager
      int position = bottleInfo.bottlePosition - 1; // Convert 1-based to 0-based
      if (position >= 0 && position < BOTTLE_COUNT) {
        // Get the current position data and update its status
        BottlePosition pos = bottleManager.getBottlePosition(position);
        pos.status = BOTTLE_EMPTY;
        pos.bottleIndex = -1; // No bottle associated
        
        // Update the position in the bottle manager
        bottleManager.updatePositionStatus(position, pos);
      }
    } else {
      Serial.print("Bottle info message received for bottle index ");
      Serial.println(bottleInfo.bottleIndex);
      
      // Update bottle data in the bottle manager
      bottleManager.updateBottleFromMessage(bottleInfo);
    }
    
    // Copy to the main structure for display updates
    incomingData.messageType = bottleInfo.messageType;
    strncpy(incomingData.trayID, bottleInfo.trayID, sizeof(bottleInfo.trayID));
    
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

// Process any buffered messages
void CommunicationManager::processBufferedMessages() {
  // This function is called in the main loop to process any
  // messages that might be buffered in the ESP-NOW queue
  
  // Currently, ESP-NOW doesn't require manual processing of buffered messages
  // as it uses callbacks. However, we keep this function in case we need
  // to add message queuing or buffering in the future.
  
  // Nothing to do here for now - all processing happens in callbacks
}