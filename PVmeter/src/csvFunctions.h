#ifndef CSV_H_
#define CSV_H_


#include <esp_LittleFS.h>
#include <Arduino.h>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <LittleFS.h>





void readCSVtoArray(const char* filePath, float dataArray[], int maxRows);
float* allocateFloatArray(size_t arraySize);

// Function to read a CSV file and store it in arrays (only first two columns)
void readCSVtoArray(const char* filePath, float dataArray[], int maxRows) {
  // Initialize LittleFS
  if (!LittleFS.begin()) {
    Serial.println("An error has occurred while mounting LittleFS");
    return;
  }

  // Open the file from LittleFS
  File file = LittleFS.open(filePath, "r");
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  String line = "";
  int rowIndex = 0;

  // Read each line from the file
  while (file.available() && rowIndex < maxRows) {
    line = file.readStringUntil('\n'); // Read a line
    // Serial.println(line);  // Print line to Serial (optional for debugging)

    // Extract timestamp (first column)
    int firstCommaIndex = line.indexOf(',');
    if (firstCommaIndex != -1) {
      // timeArray[rowIndex] = line.substring(0, firstCommaIndex);  // Store the timestamp

      // Extract the value from the second column
      int secondCommaIndex = line.indexOf(',', firstCommaIndex + 1);
      if (secondCommaIndex != -1) {
        String secondValue = line.substring(firstCommaIndex + 1, secondCommaIndex);
        dataArray[rowIndex] = secondValue.toFloat();  // Convert to float and store
      }
    }
    rowIndex++;
  }

  file.close();
  
  // Print CSV data from arrays (for debugging)
  for (int i = 0; i < rowIndex; i++) {
    // Serial.print(timeArray[i]);
    Serial.print("\t");
    Serial.println(dataArray[i]);
  }
}


// Function to allocate memory for a float array
float* allocateFloatArray(size_t arraySize) {
  // Calculate the required memory in bytes
  size_t requiredMemory = arraySize * sizeof(float);

  // Get available heap memory
  size_t freeHeap = ESP.getFreeHeap();
  
  Serial.print("Free heap available: ");
  Serial.println(freeHeap);

  // Check if there's enough memory to allocate the array
  if (freeHeap >= requiredMemory) {
    float* array = (float*) malloc(requiredMemory);
    if (array != NULL) {
      Serial.println("Float array allocated successfully.");
      return array; // Return the allocated array
    } else {
      Serial.println("Failed to allocate memory for the float array.");
      return NULL;
    }
  } else {
    Serial.println("Not enough free heap to allocate the float array.");
    return NULL;
  }
}

void modifyFile(const char* filepath) {
  File file = LittleFS.open(filepath, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading");
    return;
  }

  // Read the file line by line and ignore header lines (modify this based on your file format)
  String line;
  bool dataStart = false;
  String modifiedData = "";

  while (file.available()) {
    line = file.readStringUntil('\n');
    if (line.startsWith("Year")) { // Assuming 'Year' marks the start of data
      dataStart = true;
    }
    if (dataStart) {
      modifiedData += line + "\n";
    }
  }
  file.close();

  // Save modified data back to file
  file = LittleFS.open(filepath, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  file.print(modifiedData);
  file.close();

  Serial.println("File modified successfully");
}




#endif