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
#include "ButtonFunctions.h"

void tareScale();

// Bottle structure
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
    bottleObj["type"] = bottles[i].type;
    bottleObj["region"] = bottles[i].region;
    bottleObj["vintage"] = bottles[i].vintage;
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
  bottles[0] = {"8410415520628", "Señorío de los Llanos", "Reserva", "Valdepeñas", "2018", 1, 1120.0, "N/A", false};
  bottles[1] = {"8420209033869", "Silanus", "Joven", "Ribera del Guadiana", "2022", 2, 900.0, "N/A", false};
  bottles[2] = {"8411543110132", "Viña Pomal", "Crianza", "Rioja", "2020", 3, 350.0, "N/A", false};
  bottles[3] = {"24008105", "Leche Desnadata", "Desnadata", "Prueba", "2025", 4, 1000.0, "N/A", false};
  bottles[4] = {"24011150", "Aceite Girasol", "Girasol", "Prueba", "2025", 5, 999.0, "N/A", false};
  bottles[5] = {"6789012345", "Bottle 6", "", "", "", 6, 0.0, "N/A", false};
  bottles[6] = {"7890123456", "Bottle 7", "", "", "", 7, 0.0, "N/A", false};
  bottles[7] = {"8901234567", "Bottle 8", "", "", "", 8, 0.0, "N/A", false};
  bottles[8] = {"9012345678", "Bottle 9", "", "", "", 9, 0.0, "N/A", false};
  
  // Initialize remaining bottles with placeholder data
  for (int i = 5; i < BOTTLE_COUNT; i++) {
    char barcode[11];
    sprintf(barcode, "%d000000000", i+1);
    char name[20];
    sprintf(name, "Bottle %d", i+1);
    bottles[i] = {String(barcode), String(name), "", "", "", i+1, 0.0, "N/A", false};
  }
  
  saveDatabase();
}

void emptyDatabase() {
  Serial.println("Please remove all bottles...");
  
  // Primero verificar los switches para determinar qué posiciones tienen botellas físicamente
  updateAllButtons();
  int bottlesInTray = 0;
  bool bottlePresent[BOTTLE_COUNT];
  
  // Revisar todas las posiciones para ver cuáles tienen botellas
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    // LOW significa que el botón está presionado (hay una botella)
    bottlePresent[i] = (bottleButtons[i]->getState() == LOW);
    if (bottlePresent[i]) {
      bottlesInTray++;
    }
  }
  
  // Mostrar cuántas botellas hay que retirar
  Serial.print("Please remove ");
  Serial.print(bottlesInTray);
  Serial.print(" of ");
  Serial.print(BOTTLE_COUNT);
  Serial.println(" bottles from the tray");
  
  // Si hay botellas, mostrar cuáles están presentes
  if (bottlesInTray > 0) {
    Serial.println("Bottles detected in positions:");
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      if (bottlePresent[i]) {
        Serial.print("Position ");
        Serial.print(i + 1);
        if (bottles[i].barcode != "") {
          Serial.print(": ");
          Serial.println(bottles[i].name);
        } else {
          Serial.println(": Unknown bottle");
        }
      }
    }
    
    // Esperar a que se retiren todas las botellas
    while (bottlesInTray > 0) {
      // Esperar a que se retire una botella
      int removedIndex = waitForBottleRemoval();
      
      if (removedIndex >= 0 && removedIndex < BOTTLE_COUNT) {
        if (bottlePresent[removedIndex]) {
          bottlePresent[removedIndex] = false;
          bottlesInTray--;
          
          // Mostrar información sobre la botella retirada
          Serial.print("Bottle removed from position ");
          Serial.println(removedIndex + 1);
          
          // Actualizar el contador
          Serial.print("Remaining bottles: ");
          Serial.print(bottlesInTray);
          Serial.print(" of ");
          Serial.println(BOTTLE_COUNT);
        }
      }
    }
    
    Serial.println("All bottles have been removed from the tray!");

    Serial.println("Performing scale tare (calibration to zero)...");
    tareScale();
    Serial.println("Scale calibration completed!");

  } else {
    Serial.println("No bottles detected in the tray!");
  }
  
  // Una vez que todas las botellas han sido retiradas, vaciar la base de datos
  for (int i = 0; i < BOTTLE_COUNT; i++) {
    bottles[i] = {"", "", "", "", "", i+1, 0.0, "N/A", false};
  }
  
  saveDatabase();
  Serial.println("Database emptied. All positions are now clear.");
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
    bottles[i].type = obj["type"].as<String>();
    bottles[i].region = obj["region"].as<String>();
    bottles[i].vintage = obj["vintage"].as<String>();
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

#include "SensorFunctions.h"
#endif // BOTTLE_DATABASE_H