// ==========================================================
//                WINE FRIDGE MAIN
// ==========================================================

#include <Arduino.h>
#include "Config.h"
#include "BottleDatabase.h"
#include "WineCatalog.h"
#include "SensorFunctions.h"
#include "ButtonFunctions.h"
#include "UtilityFunctions.h"
#include "MenuFunctions.h"
#include "HardwareSetup.h"
#include "LEDFunctions.h"
#include "ESPNowFunctions.h"

// Definición de variables globales
Bottle bottles[BOTTLE_COUNT];  // Definición de la variable global para la base de datos de botellas
Preferences preferences;       // Definición de la variable global para las preferencias

unsigned long lastUpdateTime = 0;
const unsigned long UPDATE_INTERVAL = 30000; // 30 seconds in milliseconds
bool systemBusy = false; // Flag to indicate if system is currently executing a command


ezButton* bottleButtons[BOTTLE_COUNT];  // Definición de la variable global para los botones
HX711 scale;                            // Definición de la variable global para la balanza
M5UnitQRCodeI2C qrcode;                 // Definición de la variable global para el escáner QR
Adafruit_SHT31 sht31;                   // Definición de la variable global para el sensor de temperatura
CRGB leds[NUM_LEDS];                    // Definición de la variable global para los LEDs

// ==========================================================
//                    SETUP & LOOP
// ==========================================================

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10);
  }
  Serial.println("\nWine Fridge Management System");
  
  // ===== Depuración extendida =====
  Serial.println("====== Extended Debug =====");
  
  // Reiniciar componentes WiFi/ESP-NOW completamente
  WiFi.disconnect(true);
  delay(500);
  WiFi.mode(WIFI_OFF);
  delay(500);
  WiFi.mode(WIFI_STA);
  delay(500);
  
  Serial.print("WiFi MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("WiFi Mode: ");
  Serial.println(WiFi.getMode() == WIFI_STA ? "STATION" : "OTHER");
  
  // Setup hardware components
  setupButtons();
  setupSensors();
  setupLEDs();
  
  // Load or initialize database
  loadDatabase();
  
  // Setup ESP-NOW después de limpiar WiFi
  Serial.println("Initializing ESP-NOW...");
  setupESPNow();
  
  // Tiempo de NTP (no necesitamos conectarnos a WiFi para esto)
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  
  // Show welcome sequence on LEDs
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 0, 255);  // Blue color
    FastLED.show();
    delay(30);
  }
  delay(500);
  clearAllLEDs();
  
  // Intentar una comunicación simple con el display
  sendSimpleTestMessage();
  
  // Send initial menu to display
  sendMenuToDisplay();
  
  printMenu();       // Declarada en UtilityFunctions.h
}

void loop() {
  // Get current time
  unsigned long currentTime = millis();
  
  // Process menu options
  if (Serial.available() > 0) {
    systemBusy = true; // Set busy flag when executing a command
    int option = readIntFromSerial();  // Declared in UtilityFunctions.h
    
    // Clear any previous LED states before starting a new operation
    clearAllLEDs();
    
    // Send status update to display
    char statusMsg[250];
    snprintf(statusMsg, sizeof(statusMsg), "Selected option: %d", option);
    sendStatusUpdateToDisplay(statusMsg);
    
    switch (option) {
      case 0:
        configurationAndTestingMenu();
        break;
      case 1:
        scanBottleAndPlace();  // Declared in MenuFunctions.h
        break;
      case 2:
        showBottleDatabase();  // Declared in MenuFunctions.h
        // Send bottle database to display
        sendBottleDatabaseToDisplay();
        break;
      case 3:
        handleAutomaticBottleEvent();  // Declared in MenuFunctions.h
        break;
      case 4:
        swapBottlePositions();  // Declared in MenuFunctions.h
        break;
      case 6:
        sequentialBottleLoading();  // Declared in MenuFunctions.h
        break;
      case 7:
        unloadBottleByRegion();
        break;
      case 8:
        swapBottlePositions();
        break;
      case 9:
        scanBottleAndPlace();  // Declared in MenuFunctions.h
        break;
      default:
        Serial.println("Invalid option");
        sendErrorToDisplay("Invalid menu option selected");
        break;
    }

    // Limpiar el búfer de entrada después de ejecutar una opción
    while (Serial.available() > 0) {
      Serial.read();
    }
    
    // Clear LEDs after operation
    clearAllLEDs();
    
    // Send updated menu to display
    sendMenuToDisplay();
    
    printMenu();  // Declared in UtilityFunctions.h
    
    // Update time and reset busy flag after completing the command
    lastUpdateTime = millis();
    systemBusy = false;
  }
  
  // Check if it's time to send a periodic update when system is idle
  if (!systemBusy && (currentTime - lastUpdateTime >= UPDATE_INTERVAL)) {
    Serial.println("Sending automatic bottle database update...");
    
    // Send a status message first
    sendStatusUpdateToDisplay("Automatic inventory update in progress...");
    
    // Send the database
    sendBottleDatabaseToDisplay();
    
    // Update the timer
    lastUpdateTime = currentTime;
    
    // Send the menu again after the update
    sendMenuToDisplay();
  }
  
  // Keep button states updated
  updateAllButtons();  // Declared in ButtonFunctions.h
  
  delay(50);
}