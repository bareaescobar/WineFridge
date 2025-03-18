/**
 * DataStructures.h
 * Defines data structures for bottle information and communication messages
 */

#ifndef DATA_STRUCTURES_H
#define DATA_STRUCTURES_H

#include "config.h"
#include <Arduino.h>

// Bottle structure - must match the sender structure
struct Bottle {
  String barcode;
  String name;
  String type;          // Wine type (Crianza, Reserva, etc.)
  String region;        // Wine region (Rioja, Ribera del Duero, etc.)
  String vintage;       // Wine vintage year
  int position;         // Logical position (1-9)
  float weight;
  String lastInteraction;
  bool inFridge;
};

// Structure to receive data - must match the sender structure
typedef struct struct_message {
  int messageType;  // 0: Menu, 1: Bottle Database, 2: Bottle Info, 3: Status Update, 4: Error
  char trayID[10];  // Unique identifier for each tray
  char text[250];   // Main text message
  int bottleCount;  // Number of bottles in rack
  Bottle bottles[BOTTLE_COUNT]; // Array of bottle data
} struct_message;

// Simplified structure for better compatibility
typedef struct simple_message {
  int messageType;      // 0: Menu, 1: Bottle DB, 2: Bottle Info, 3: Status, 4: Error
  char trayID[10];      // Unique identifier for each tray
  char text[100];       // Reduced text
  int bottleCount;      // Number of bottles
} simple_message;

// Structure for receiving bottle information message
typedef struct bottle_info_message {
  int messageType;     // Always 2: Bottle Info
  char trayID[10];     // Unique identifier for each tray
  int bottleIndex;     // Index of this bottle in the database
  int bottlePosition;  // Position of this bottle (1-9)
  char barcode[20];    // Barcode
  char name[50];       // Name
  char type[20];       // Type
  char region[20];     // Region
  char vintage[10];    // Vintage year
  float weight;        // Weight
  char lastInteraction[30]; // Last interaction time
  bool inFridge;       // Whether the bottle is in the rack
} bottle_info_message;

// Structure for bottle position on screen
struct BottlePosition {
  int x;
  int y;
  int status; // 0: none, 1: empty, 2: waiting, 3: present
  int bottleIndex; // Index to the bottle in the array
};

#endif // DATA_STRUCTURES_H