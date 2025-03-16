// ==========================================================
//                DEFINITIONS & CONSTANTS
// ==========================================================
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pins configuration
#define HX711_DT_PIN 19
#define HX711_SCK_PIN 18

// Array of button pins
const uint8_t BUTTON_PINS[9] = {4, 5, 23, 14, 27, 26, 12, 33, 32};

// Network configuration
#define WIFI_SSID "Bareas_WIFI"
#define WIFI_PASSWORD "bareaescobar"

const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 3600;      // GMT+1
const int DAYLIGHT_OFFSET_SEC = 3600;  // Adjust for DST

// Constants
const int BOTTLE_COUNT = 9;
const char* STORAGE_NAMESPACE = "storage";
const char* DB_KEY = "db";
const int RETURN_TIMEOUT_MS = 5000;  // 5 seconds for bottle return

#endif // CONFIG_H