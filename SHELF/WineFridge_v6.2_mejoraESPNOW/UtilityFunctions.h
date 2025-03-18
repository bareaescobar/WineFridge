// ==========================================================
//                UTILITY FUNCTIONS
// ==========================================================
#ifndef UTILITY_FUNCTIONS_H
#define UTILITY_FUNCTIONS_H

#include <Arduino.h>

String getTimeStamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "Time not set";
  }
  char timeString[20];
  strftime(timeString, sizeof(timeString), "%d/%m/%Y %H:%M:%S", &timeinfo);
  return String(timeString);
}

int readIntFromSerial() {
  while (Serial.available() == 0) {
    delay(100);
  }
  String input = Serial.readStringUntil('\n');
  input.trim();
  return input.toInt();
}

void printMenu() {
  Serial.println("\n------ WINE FRIDGE MENU ------");
  Serial.println("0 - Configuration and testing");
  Serial.println("1 - Scan bottle and place");
  Serial.println("2 - Show bottle database");
  Serial.println("3 - Automatic bottle removal/return");
  Serial.println("4 - Swap bottle positions");
  Serial.println("--------------------------");
  Serial.println("6 - Individual sequential loading");
  Serial.println("7 - Unloading bottle by region");
  Serial.println("8 - Swap bottle positions");
  Serial.println("9 - Used bottles returned");
  Serial.print("Enter option: ");
}

#endif // UTILITY_FUNCTIONS_H