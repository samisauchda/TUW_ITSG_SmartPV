#ifndef HELPERFUNCTIONS_H_
#define HELPERFUNCTIONS_H_

#include <ArduinoJson.h>
//#include <LittleFS.h>
#include <esp_LittleFS.h>


void printSavedModules() {
  if (LittleFS.exists("/modules.json")) {
    File file = LittleFS.open("/modules.json", FILE_READ);
    JsonDocument doc;
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
  if (LittleFS.exists("/email.json")) {
    File file = LittleFS.open("/email.json", FILE_READ);
    JsonDocument doc;
    deserializeJson(doc, file);
    file.close();
    Serial.println("Email Params:");
    serializeJsonPretty(doc, Serial);
    Serial.println();
  } else {
    Serial.println("No email params.");
  }
}

void listLittleFSFiles() {
  File root = LittleFS.open("/");
  File file = root.openNextFile();

  if(!file) {
    Serial.println("No files on LittleFS");
    return;
  }
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


#endif
