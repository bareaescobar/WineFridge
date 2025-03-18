/**
 * CommunicationManager.h
 * Handles ESP-NOW communication and message processing
 */

#ifndef COMMUNICATION_MANAGER_H
#define COMMUNICATION_MANAGER_H

#include <esp_now.h>
#include "DataStructures.h"

// Message buffer constants
#define MAX_BUFFER_SIZE 10
#define MAX_MESSAGE_SIZE 512

// Forward declaration
class BottleManager;

class CommunicationManager {
private:
  struct_message incomingData;     // Original structure
  simple_message simpleData;       // Simplified structure
  bottle_info_message bottleInfo;  // Bottle info message structure
  
  // Message buffer to handle quick message arrivals
  struct MessageBuffer {
    uint8_t data[MAX_MESSAGE_SIZE];
    size_t length;
    bool used;
  };
  
  MessageBuffer messageBuffer[MAX_BUFFER_SIZE];
  int bufferIndex;
  bool processingBuffer;
  
public:
  CommunicationManager();
  
  // Initialize ESP-NOW
  bool initESPNow();
  
  // Process incoming ESP-NOW data
  void processIncomingData(const uint8_t *incomingBuf, int len);
  
  // Process any buffered messages
  void processBufferedMessages();
  
  // Buffer a new message
  void bufferMessage(const uint8_t *data, size_t length);
  
  // Get reference to incoming data
  struct_message* getIncomingData() { return &incomingData; }
  
  // Static callback function (will call instance method)
  static void staticOnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingBuf, int len);
};

// Declare external instance for global use
extern CommunicationManager commManager;

#endif // COMMUNICATION_MANAGER_H