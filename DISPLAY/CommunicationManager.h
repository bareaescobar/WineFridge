/**
 * CommunicationManager.h
 * Handles ESP-NOW communication and message processing
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <esp_now.h>
#include "DataStructures.h"

// Forward declaration
class BottleManager;

class CommunicationManager {
private:
  struct_message incomingData;     // Original structure
  simple_message simpleData;       // Simplified structure
  bottle_info_message bottleInfo;  // Bottle info message structure
  
public:
  CommunicationManager();
  
  // Initialize ESP-NOW
  bool initESPNow();
  
  // Process incoming ESP-NOW data
  void processIncomingData(const uint8_t *incomingBuf, int len);
  
  // Get reference to incoming data
  struct_message* getIncomingData() { return &incomingData; }
  
  // Static callback function (will call instance method)
  static void staticOnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingBuf, int len);
};

// Declare external instance for global use
extern CommunicationManager commManager;

#endif // COMMUNICATION_MANAGER_H