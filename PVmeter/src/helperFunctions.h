#include <ArduinoJson.h>
#include <SPIFFS.h>

void printSavedModules() {
  if (SPIFFS.exists("/modules.json")) {
    File file = SPIFFS.open("/modules.json", FILE_READ);
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, file);
    file.close();
    Serial.println("Saved Modules:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  } else {
    Serial.println("No modules saved.");
  }
}

void printEmailParams() {
  if (SPIFFS.exists("/email.json")) {
    File file = SPIFFS.open("/email.json", FILE_READ);
    StaticJsonDocument<1024> doc;
    deserializeJson(doc, file);
    file.close();
    Serial.println("Email Params:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  } else {
    Serial.println("No email params.");
  }
}

void listSPIFFSFiles() {
  File root = SPIFFS.open("/");
  File file = root.openNextFile();

  while (file) {
    if (file.isDirectory()) {
      Serial.print("DIR : ");
      Serial.println(file.name());
    } else {
      Serial.print("FILE: ");
      Serial.print(file.name());
      Serial.print("\tSIZE: ");
      Serial.println(file.size());
    }
    file = root.openNextFile();
  }
}