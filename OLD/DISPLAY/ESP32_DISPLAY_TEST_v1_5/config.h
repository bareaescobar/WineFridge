/**
 * config.h
 * Configuration settings and constants for the Wine Rack Display
 */

#ifndef CONFIG_H
#define CONFIG_H

// Message types
#define MSG_TYPE_MENU 0
#define MSG_TYPE_BOTTLE_DB 1
#define MSG_TYPE_BOTTLE_INFO 2
#define MSG_TYPE_STATUS 3
#define MSG_TYPE_ERROR 4

// Define colors
#define BACKGROUND TFT_BLACK
#define TEXT_COLOR TFT_WHITE
#define HEADER_COLOR 0x0018       // Dark Blue
#define ERROR_COLOR TFT_RED
#define SUCCESS_COLOR 0x0540      // Dark Green
#define HIGHLIGHT_COLOR TFT_YELLOW
#define WINE_COLOR 0x7800         // Dark Red
#define EMPTY_COLOR 0x4208        // Gray
#define WAITING_COLOR 0xFD60      // Orange

// Font sizes
#define LARGE_FONT 3
#define MEDIUM_FONT 2
#define SMALL_FONT 1

// View states
#define VIEW_MAIN 0
#define VIEW_DETAIL 1

// Bottle status
#define BOTTLE_NONE 0     // No bottle information
#define BOTTLE_EMPTY 1    // Empty slot in the rack
#define BOTTLE_WAITING 2  // Waiting for the bottle to return
#define BOTTLE_PRESENT 3  // Bottle in position

// Define maximum number of bottles
#define BOTTLE_COUNT 9

// Touch debounce time in ms
#define TOUCH_DEBOUNCE 300

#endif // CONFIG_H