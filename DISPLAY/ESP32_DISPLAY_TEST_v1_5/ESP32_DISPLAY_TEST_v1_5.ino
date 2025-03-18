/**
 * WineRackDisplay.ino
 * Main file for the Wine Rack Display for CrowPanel ESP32-S3 7-inch Display
 */

#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <esp_now.h>
#include <time.h>

#include "gfx_conf.h"
#include "config.h"
#include "DataStructures.h"
#include "BottleManager.h"
#include "CommunicationManager.h"
#include "DisplayManager.h"

// WiFi configuration
#define WIFI_SSID "Bareas_WIFI"
#define WIFI_PASSWORD "bareaescobar"
#define WIFI_TIMEOUT 10000  // 10 second timeout for connection

// NTP configuration
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;      // GMT+1
const int daylightOffset_sec = 3600;  // Adjust for DST

// Global variables
uint32_t lastTouchTime = 0;
bool ledState = false;
uint32_t lastBlink = 0;
bool timeInitialized = false;

// External objects
extern CommunicationManager commManager;
extern BottleManager bottleManager;
DisplayManager* displayManager = NULL;

// Function to sync time
bool syncTimeWithNTP() {
  Serial.println("Setting up time with NTP...");
  
  // Add struct tm declaration here
  struct tm timeinfo;
  
  // Configure NTP
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  
  // Wait for time to be set
  unsigned long startAttempt = millis();
  while (!getLocalTime(&timeinfo) && millis() - startAttempt < 5000) {
    delay(100);
  }
  
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return false;
  }
  
  char timeStringBuf[50];
  strftime(timeStringBuf, sizeof(timeStringBuf), "%A, %B %d %Y %H:%M:%S", &timeinfo);
  Serial.print("Current time: ");
  Serial.println(timeStringBuf);
  return true;
}

// Setup WiFi and time
void setupWiFiAndTime() {
  Serial.println("Connecting to WiFi for time sync...");
  
  // Store current WiFi mode
  wifi_mode_t currentMode = WiFi.getMode();
  
  // Connect to WiFi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  unsigned long startAttempt = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < WIFI_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    
    // Sync time
    timeInitialized = syncTimeWithNTP();
    
    // Disconnect but keep WiFi hardware initialized
    WiFi.disconnect(true, false);
  } else {
    Serial.println("\nWiFi connection failed");
  }
  
  // Return to original mode (or STA for ESP-NOW)
  WiFi.mode(WIFI_STA);
  delay(100);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n===== Wine Rack Display - CrowPanel ESP32 =====");
  
  // Initialize display
  tft.begin();
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(MEDIUM_FONT);
  
  // Test display with color sequence
  tft.fillScreen(TFT_BLUE);
  delay(500);
  tft.fillScreen(TFT_YELLOW);
  delay(500);
  tft.fillScreen(TFT_GREEN);
  delay(500);
  tft.fillScreen(TFT_WHITE);
  delay(500);
  tft.fillScreen(TFT_BLACK);
  
  // Create display manager first
  displayManager = new DisplayManager(commManager.getIncomingData());
  
  // Show initial welcome screen
  displayManager->showWelcomeScreen("Initializing...");
  
  // Setup WiFi and time - do this before ESP-NOW
  setupWiFiAndTime();
  
  // Update display manager with time status
  displayManager->setTimeInitialized(timeInitialized);
  
  // Initialize ESP-NOW after WiFi
  displayManager->showWelcomeScreen("Setting up communication...");
  
  // Initialize bottle positions
  bottleManager.initializeBottlePositions();
  
  // Initialize ESP-NOW
  if (!commManager.initESPNow()) {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_RED);
    tft.setTextSize(MEDIUM_FONT);
    tft.setCursor(20, 100);
    tft.print("Error initializing ESP-NOW!");
    return;
  }
  
  Serial.println("Ready to receive data from Wine Rack");
  
  // Show final welcome screen
  displayManager->showWelcomeScreen("Waiting for connection...");
}

void loop() {
  // Update time display periodically
  static unsigned long lastTimeUpdate = 0;
  if (timeInitialized && millis() - lastTimeUpdate > 60000) {
    lastTimeUpdate = millis();
    if (displayManager && !displayManager->isProcessingMessage()) {
      displayManager->updateTimeDisplay();
    }
  }
  
  // Process any pending messages
  commManager.processBufferedMessages();
  
  // Show activity indicator
  if (millis() - lastBlink > 1000) {
    lastBlink = millis();
    ledState = !ledState;
    
    // Draw indicator
    tft.fillCircle(screenWidth - 20, 20, 10, 
                  ledState ? TFT_GREEN : TFT_DARKGREEN);
    
    // Hide update indicator if needed
    if (displayManager && displayManager->shouldHideUpdateIndicator()) {
      displayManager->hideUpdateIndicator();
    }
    
    // Status log every 10 seconds
    static int statusCounter = 0;
    statusCounter++;
    if (statusCounter >= 10) {
      statusCounter = 0;
      Serial.println("Display running... Waiting for ESP-NOW data");
    }
  }
  
  // Process touch input
  uint16_t touchX, touchY;
  bool touched = tft.getTouch(&touchX, &touchY);
  
  if (touched && (millis() - lastTouchTime > TOUCH_DEBOUNCE)) {
    lastTouchTime = millis();
    Serial.printf("Touch at X:%d Y:%d\n", touchX, touchY);
    
    // Process touch
    displayManager->handleTouch(touchX, touchY);
  }
  
  delay(10);
}