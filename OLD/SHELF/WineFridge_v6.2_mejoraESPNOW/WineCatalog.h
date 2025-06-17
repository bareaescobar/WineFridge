// ==========================================================
//              WINE CATALOG DATABASE
// ==========================================================
#ifndef WINE_CATALOG_H
#define WINE_CATALOG_H

#include <Arduino.h>

// Structure for wine catalog entries
struct WineInfo {
  String barcode;
  String name;
  String type;
  String region;
  String vintage;
};

// Wine catalog database
// Contains barcode and wine information for recognition
const WineInfo wineCatalog[] = {
  {"8410415520628", "Señorío de los Llanos", "Reserva", "Valdepeñas", "2018"},
  {"8420209033869", "Silanus", "Joven", "Ribera del Guadiana", "2022"},
  {"8411543110132", "Viña Pomal", "Crianza", "Rioja", "2020"},
  {"8413913022508", "Marqués de Riscal", "Reserva", "Rioja", "2016"},
  {"8410702031204", "Protos", "Crianza", "Ribera del Duero", "2018"}, 
  {"8436585840016", "Rolland Galarreta", "Tempranillo", "Rioja", "2017"},
  {"8437005880023", "Carmelo Rodero", "Crianza", "Ribera del Duero", "2019"},
  {"8410451008115", "Martín Códax", "Albariño", "Rías Baixas", "2022"},
  {"8437003819193", "Pago de los Capellanes", "Crianza", "Ribera del Duero", "2019"},
  {"4004732001219", "Blue Nun", "Liebfraumilch", "Rheinhessen", "2021"},
  {"8426998270012", "Muga", "Reserva", "Rioja", "2019"},
  {"8412655401018", "Pesquera", "Crianza", "Ribera del Duero", "2019"},
  {"8413336000179", "Ramón Bilbao", "Crianza", "Rioja", "2019"},
  {"3760040433638", "Minuty", "Rosé", "Provence", "2023"},
  {"8411608001189", "Marques de Caceres", "Crianza", "Rioja", "2019"},
  {"24011150", "Aceite", "Girasol", "Prueba", "2025"},
  {"24008105", "Leche", "Desnadata", "Prueba", "2025"}
};

const int CATALOG_SIZE = sizeof(wineCatalog) / sizeof(wineCatalog[0]);

// Find wine information by barcode
WineInfo* findWineInCatalog(const String& barcode) {
  for (int i = 0; i < CATALOG_SIZE; i++) {
    if (wineCatalog[i].barcode == barcode) {
      return const_cast<WineInfo*>(&wineCatalog[i]);
    }
  }
  return NULL;
}

// Get formatted wine name from catalog
String getFormattedWineName(const String& barcode) {
  WineInfo* wine = findWineInCatalog(barcode);
  
  if (wine != NULL) {
    return wine->name + " - " + wine->region + " - " + wine->type + " " + wine->vintage;
  }
  
  return "Unknown Wine";
}

// Print catalog for debugging
void printWineCatalog() {
  Serial.println("WINE CATALOG:");
  for (int i = 0; i < CATALOG_SIZE; i++) {
    Serial.print(wineCatalog[i].barcode);
    Serial.print(": ");
    Serial.print(wineCatalog[i].name);
    Serial.print(" ");
    Serial.print(wineCatalog[i].type);
    Serial.print(" (");
    Serial.print(wineCatalog[i].region);
    Serial.print(", ");
    Serial.print(wineCatalog[i].vintage);
    Serial.println(")");
  }
}

#endif // WINE_CATALOG_H