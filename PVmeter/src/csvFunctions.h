#ifndef CSV_H_
#define CSV_H_


#include <esp_LittleFS.h>
#include <Arduino.h>
#include <WiFi.h>
#include <list>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ESP32Time.h>
#include <FS.h>
#include <LittleFS.h>

#define MAX_ROWS 8769
#define MAX_LINE_LENGTH 1024
const int maxRows = 10;     // Maximum number of rows to read
const int maxCols = 10;     // Maximum number of columns per row
const char *csvFilePath = "/hourly_data.csv";

String data[maxRows][maxCols]; // Array to store the CSV content
String dataWeekly[24*7][3]; // array for measured values for each hour of every day in one week, yyyymmdd:hhmm, power, energy


void writeToCSV(const String& timestamp, float power, float energy);
void parseCSVLine(String line, int row);
void readCSV(const char *path);


void writeToCSV(const String& timestamp, float power, float energy) {
    File file = LittleFS.open("/power_data.csv", FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(timestamp);
    file.print(", ");
    file.print(power);
    file.print(", ");
    file.println(energy);
    file.close();
}

void parseCSVLine(String line, int row) {
  int col = 0;
  int lastIndex = 0;
  int currentIndex = 0;
  while (currentIndex != -1 && col < maxCols) {
    currentIndex = line.indexOf(',', lastIndex);
    if (currentIndex == -1) {
      data[row][col] = line.substring(lastIndex);
    } else {
      data[row][col] = line.substring(lastIndex, currentIndex);
    }
    lastIndex = currentIndex + 1;
    col++;
  }
}



void readCSV(const char *path) {
  File file = LittleFS.open(path, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  Serial.println("Reading CSV file...");

  int row = 0;
  while (file.available() && row < maxRows) {
    String line = file.readStringUntil('\n');
    parseCSVLine(line, row);
    row++;
  }

  file.close();
}


#endif