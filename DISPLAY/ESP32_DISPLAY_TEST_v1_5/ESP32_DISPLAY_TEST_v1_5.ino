/**
 * WineRackDisplay.ino
 * Main file for the Wine Rack Display for CrowPanel ESP32-S3 7-inch Display
 * 
 * This code implements ESP-NOW communication to display Wine Rack information
 * on a CrowPanel 7-inch display with ESP32-S3.
 */

//ESP32s3 DEV MODULE
//COM8
//PSRAM: OPI PSRAM
//PARTITION SCHEME: HUGE APP
//LovyanGFX lib

#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>
#include <time.h>

/*******************************************************************************
   Config the display panel and touch panel in gfx_conf.h
 ******************************************************************************/
#include "gfx_conf.h"
#include "config.h"
#include "DataStructures.h"
#include "BottleManager.h"
#include "CommunicationManager.h"
#include "DisplayManager.h"

// WiFi configuration - using same credentials as the Wine Shelf
#define WIFI_SSID "Bareas_WIFI"
#define WIFI_PASSWORD "bareaescobar"

// NTP server configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // GMT+1
const int daylightOffset_sec = 3600;  // Adjust for DST

// Define global variables
uint32_t lastTouchTime = 0;
bool ledState = false;
uint32_t lastBlink = 0;
bool timeInitialized = false;

// External objects - declared elsewhere, used here
extern CommunicationManager commManager;
extern BottleManager bottleManager;
DisplayManager* displayManager = NULL; // Will be initialized in setup()

// Function to connect to WiFi and initialize time
void setupWiFiAndTime() {
  Serial.println("Connecting to WiFi to sync time...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. Setting up time...");
    
    // Configure time with NTP
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
      char timeStringBuf[50];
      strftime(timeStringBuf, sizeof(timeStringBuf), "%A, %B %d %Y %H:%M:%S", &timeinfo);
      Serial.print("Current time: ");
      Serial.println(timeStringBuf);
      timeInitialized = true;
    } else {
      Serial.println("Failed to obtain time");
    }
    
    // Disconnect from WiFi but keep station mode for ESP-NOW
    WiFi.disconnect(false);  // Disconnect but keep WiFi hardware initialized
  } else {
    Serial.println("\nFailed to connect to WiFi for time sync");
  }
  
  // Set WiFi to station mode for ESP-NOW
  WiFi.mode(WIFI_STA);
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n===== Wine Rack Display - CrowPanel ESP32 =====");
  
  // Initialize display
  Serial.println("Initializing display...");
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(MEDIUM_FONT);
  
  // Test color sequence
  Serial.println("Testing display...");
  tft.fillScreen(TFT_BLUE);
  delay(500);
  tft.fillScreen(TFT_YELLOW);
  delay(500);
  tft.fillScreen(TFT_GREEN);
  delay(500);
  tft.fillScreen(TFT_WHITE);
  delay(500);
  tft.fillScreen(TFT_BLACK);
  
  // Setup WiFi and time before ESP-NOW
  setupWiFiAndTime();
  
  // Initialize the DisplayManager with the communication data
  displayManager = new DisplayManager(commManager.getIncomingData());
  displayManager->setTimeInitialized(timeInitialized);
  
  // Initialize ESP-NOW communication
  Serial.println("Setting up ESP-NOW...");
  if (!commManager.initESPNow()) {
    // Show error on display
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(MEDIUM_FONT);
    tft.setCursor(20, 100);
    tft.print("Error initializing ESP-NOW!");
    return;
  }
  
  // Initialize bottle positions
  bottleManager.initializeBottlePositions();
  
  Serial.println("Ready to receive data from Wine Rack");
  
  // Show welcome screen
  Serial.println("Showing welcome screen...");
  displayManager->showWelcomeScreen();
}

void loop() {
  // Update time every minute
  static unsigned long lastTimeUpdate = 0;
  if (timeInitialized && millis() - lastTimeUpdate > 60000) { // Update time every minute
    lastTimeUpdate = millis();
    
    // Only update the time display if we're showing a view (not processing a message)
    if (displayManager && !displayManager->isProcessingMessage()) {
      displayManager->updateTimeDisplay();
    }
  }

  // Show activity indicator
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    ledState = !ledState;
    
    // Draw a small indicator circle to show the system is running
    tft.fillCircle(screenWidth - 20, 20, 10, 
                  ledState ? TFT_GREEN : TFT_DARKGREEN);
                  
    // Check if we need to hide update indicator
    if (displayManager && displayManager->shouldHideUpdateIndicator()) {
      displayManager->hideUpdateIndicator();
    }
    
    // Process any incoming messages that might be buffered
    commManager.processBufferedMessages();
    
    // Every 10 seconds, print a status message to help with debugging
    static int statusCounter = 0;
    statusCounter++;
    if (statusCounter >= 10) {
      statusCounter = 0;
      Serial.println("Display is running... Waiting for ESP-NOW data.");
    }
  }
  
  // Get touch input and process it
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY);
  
  if (touched && (millis() - lastTouchTime > TOUCH_DEBOUNCE)) {
    lastTouchTime = millis();
    Serial.print("Touch detected at X:");
    Serial.print(touchX);
    Serial.print(" Y:");
    Serial.println(touchY);
    
    // Process touch input
    displayManager->handleTouch(touchX, touchY);
  }
  
  delay(10);
}