// ==========================================================
//                LED FUNCTIONS
// ==========================================================
#ifndef LED_FUNCTIONS_H
#define LED_FUNCTIONS_H

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"

// LED strip configuration
#define LED_PIN     13
#define NUM_LEDS    33
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define BRIGHTNESS  150

// LED colors
#define LED_COLOR_OFF   CRGB(0, 0, 0)
#define LED_COLOR_BLUE  CRGB(0, 0, 255)
#define LED_COLOR_GREEN CRGB(0, 255, 0)
#define LED_COLOR_RED   CRGB(255, 0, 0)

// Array to map bottle positions (1-9) to LED positions (0-32)
const uint8_t bottleToLed[BOTTLE_COUNT] = {
  32, // Position 1 = LED 33 (index 32)
  28, // Position 2 = LED 29 (index 28)
  24, // Position 3 = LED 25 (index 24)
  20, // Position 4 = LED 21 (index 20)
  16, // Position 5 = LED 17 (index 16)
  12, // Position 6 = LED 13 (index 12)
  8,  // Position 7 = LED 9 (index 8)
  4,  // Position 8 = LED 5 (index 4)
  0   // Position 9 = LED 1 (index 0)
};

extern CRGB leds[NUM_LEDS];

// Declaración de las funciones (forward declarations) para evitar errores de orden
void clearAllLEDs();
void setBottleLED(int position, CRGB color);
void showSuccessFeedback(int position);
void showErrorFeedback(int position);
void highlightPosition(int position);
void testLEDs();

void setupLEDs() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  // Turn off all LEDs at startup
  clearAllLEDs();
  
  Serial.println("LED strip initialized");
}

void clearAllLEDs() {
  fill_solid(leds, NUM_LEDS, LED_COLOR_OFF);
  FastLED.show();
}

// Set LED for a specific bottle position (1-9)
void setBottleLED(int position, CRGB color) {
  if (position >= 1 && position <= BOTTLE_COUNT) {
    leds[bottleToLed[position - 1]] = color;
    FastLED.show();
  }
}

// Show success feedback for a bottle position (green for 2 seconds)
void showSuccessFeedback(int position) {
  if (position >= 1 && position <= BOTTLE_COUNT) {
    leds[bottleToLed[position - 1]] = LED_COLOR_GREEN;
    FastLED.show();
    delay(2000);
    leds[bottleToLed[position - 1]] = LED_COLOR_OFF;
    FastLED.show();
  }
}

// Show error feedback for a bottle position (red for 2 seconds)
void showErrorFeedback(int position) {
  if (position >= 1 && position <= BOTTLE_COUNT) {
    leds[bottleToLed[position - 1]] = LED_COLOR_RED;
    FastLED.show();
    delay(2000);
    leds[bottleToLed[position - 1]] = LED_COLOR_OFF;
    FastLED.show();
  }
}

// Highlight a position with blue LED
void highlightPosition(int position) {
  if (position >= 1 && position <= BOTTLE_COUNT) {
    leds[bottleToLed[position - 1]] = LED_COLOR_BLUE;
    FastLED.show();
  }
}

// Función para probar los LEDs
void testLEDs() {
  Serial.println("\nTesting LEDs...");
  
  // 1. Secuencia de encendido (azul)
  Serial.println("1. Blue light sequence - all positions");
  for (int i = 1; i <= BOTTLE_COUNT; i++) {
    Serial.print("Position ");
    Serial.print(i);
    Serial.println(" - Blue");
    highlightPosition(i);
    delay(1000);
    setBottleLED(i, LED_COLOR_OFF);
  }
  
  // 2. Todos los LEDs en verde
  Serial.println("2. All positions - Green (success)");
  for (int i = 1; i <= BOTTLE_COUNT; i++) {
    setBottleLED(i, LED_COLOR_GREEN);
  }
  FastLED.show();
  delay(2000);
  
  // 3. Todos los LEDs en rojo
  Serial.println("3. All positions - Red (error)");
  for (int i = 1; i <= BOTTLE_COUNT; i++) {
    setBottleLED(i, LED_COLOR_RED);
  }
  FastLED.show();
  delay(2000);
  
  // 4. Efecto arcoíris
  Serial.println("4. Rainbow effect");
  for (int hue = 0; hue < 255; hue++) {
    for (int i = 0; i < BOTTLE_COUNT; i++) {
      leds[bottleToLed[i]] = CHSV(hue + i * 30, 255, 255);
    }
    FastLED.show();
    delay(20);
  }
  
  // 5. Apagar todos los LEDs
  Serial.println("5. Turn off all LEDs");
  clearAllLEDs();
  
  Serial.println("LED testing completed");
}

#endif // LED_FUNCTIONS_H