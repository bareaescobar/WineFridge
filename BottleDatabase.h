// ==========================================================
//              BOTTLE DATABASE MANAGEMENT
// ==========================================================
#ifndef BOTTLE_DATABASE_H
#define BOTTLE_DATABASE_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "Config.h"
#include "UtilityFunctions.h"

// Bottle structure
struct Bottle {
  String barcode;
  String name;
  int position;          // Logical position (1-9)
  float weight;
  String lastInteraction;
  bool inFridge;
};

extern Bottle bottles[BOTTLE_COUNT];
extern Preferences preferences;

// Save the current bottle database to persistent storage
void saveDatabase() {
  DynamicJsonDocument doc(2048);
  JsonArray array = doc.createNestedArray("bottles");
  
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    JsonObject bottleObj = array.createNestedObject();
    bottleObj["barcode"] = bottles[i].barcode;
    bottleObj["name"] = bottles[i].name;
    bottleObj["position"] = bottles[i].position;
    bottleObj["weight"] = bottles[i].weight;
    bottleObj["lastInteraction"] = bottles[i].lastInteraction;
    bottleObj["inFridge"] = bottles[i].inFridge;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  preferences.begin(STORAGE_NAMESPACE, false);
  preferences.putString(DB_KEY, jsonString);
  preferences.end();
  
  Serial.println("Database saved");
}

// Initialize the database with default bottle data
void initializeDefaultDatabase() {
  Serial.println("Initializing default database");
  // Initial bottle data
  bottles[0] = {"8410415520628", "Señorío de los Llanos - Valdepeñas - Reserva 2018", 1, 0.0, "N/A", true};
  bottles[1] = {"8420209033869", "Silanus - Ribera del Guadiana - Joven 2022", 2, 0.0, "N/A", true};
  bottles[2] = {"8411543110132", "Viña Pomal - Rioja - Crianza 2020", 3, 0.0, "N/A", true};
  
  // Initialize remaining bottles with placeholder data
  for (int i = 3; i < BOTTLE_COUNT; i++) {
    char barcode[11];
    sprintf(barcode, "%d000000000", i+1);
    char name[20];
    sprintf(name, "Bottle %d", i+1);
    bottles[i] = {String(barcode), String(name), i+1, 0.0, "N/A", false};
  }
  
  saveDatabase();
}

// Load bottle database from persistent storage
void loadDatabase() {
  preferences.begin(STORAGE_NAMESPACE, true);
  String jsonString = preferences.getString(DB_KEY, "{}");
  preferences.end();
  
  DynamicJsonDocument doc(2048);
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    Serial.print("Error parsing database: ");
    Serial.println(error.c_str());
    initializeDefaultDatabase();
    return;
  }
  
  JsonArray array = doc["bottles"].as<JsonArray>();
  if (array.size() == 0) {
    initializeDefaultDatabase();
    return;
  }
  
  int i = 0;
  for (JsonObject obj : array) {
    if (i >= BOTTLE_COUNT) break;
    bottles[i].barcode = obj["barcode"].as<String>();
    bottles[i].name = obj["name"].as<String>();
    bottles[i].position = obj["position"].as<int>();
    bottles[i].weight = obj["weight"].as<float>();
    bottles[i].lastInteraction = obj["lastInteraction"].as<String>();
    bottles[i].inFridge = obj["inFridge"].as<bool>();
    i++;
  }
  
  Serial.println("Database loaded");
}

// Find a bottle by its barcode
int findBottleByBarcode(const String& barcode) {
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    if (bottles[i].barcode == barcode) {
      return i;
    }
  }
  return -1;
}

// Print info about a specific bottle
void printBottleInfo(int index) {
  if (index < 0 || index >= BOTTLE_COUNT) return;
  
  Serial.print("Bottle: ");
  Serial.println(bottles[index].name);
  Serial.print("Barcode: ");
  Serial.println(bottles[index].barcode);
  Serial.print("Position: ");
  Serial.println(bottles[index].position);
  Serial.print("Weight: ");
  Serial.println(bottles[index].weight);
  Serial.print("Last Interaction: ");
  Serial.println(bottles[index].lastInteraction);
  Serial.print("In Fridge: ");
  Serial.println(bottles[index].inFridge ? "Yes" : "No");
  Serial.println("------------------------");
}

#endif // BOTTLE_DATABASE_H