#ifndef CSV_H_
#define CSV_H_

#include <esp_LittleFS.h>
#include <Arduino.h>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <LittleFS.h>
#include <HTTPClient.h>

void readCSVtoArray(const char* filePath, float dataArray[], int maxRows, String dataStartMarker, int columnIndex);
void saveArrayToCSV(const char* filename, float* array, size_t arraySize);
float* allocateFloatArray(size_t arraySize);

// Function to read a CSV file and store it in arrays (only first two columns)
void readCSVtoArray(const char* filePath, float dataArray[], int maxRows, String dataStartMarker, int columnIndex) {
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
  bool dataStart = false;  // Flag to indicate when the actual data starts

  // Read each line from the file
  while (file.available() && rowIndex < maxRows) {
    line = file.readStringUntil('\n'); // Read a line

    // Check if the current line matches the start of the data
    if (!dataStart) {
      if (line.indexOf(dataStartMarker) != -1) {
        dataStart = true;  // Data starts after this line
      }
      continue;  // Skip lines until we find the marker
    }

    // Split the line into columns based on commas
    int commaIndex = -1;
    int previousIndex = 0;
    int columnCounter = 0;
    String columnValue = "";
    
    while (columnCounter <= columnIndex) {
      commaIndex = line.indexOf(',', previousIndex);
      
      // If it's the column we're interested in, extract the value
      if (columnCounter == columnIndex) {
        if (commaIndex == -1) {
          // If no more commas are found, this is the last column
          columnValue = line.substring(previousIndex);
        } else {
          columnValue = line.substring(previousIndex, commaIndex);
        }
        break;
      }

      // Move to the next column
      previousIndex = commaIndex + 1;
      columnCounter++;
      
      // If no more commas and we haven't reached the desired column, break
      if (commaIndex == -1) {
        break;
      }
    }

    // Convert the extracted value to float and store it in the dataArray
    if (columnValue.length() > 0) {
      dataArray[rowIndex] = columnValue.toFloat();
      rowIndex++;
    }
  }

  file.close();
  
  // // Print CSV data from arrays (for debugging)
  // for (int i = 0; i < rowIndex; i++) {
  //   Serial.print("Row ");
  //   Serial.print(i);
  //   Serial.print(": ");
  //   Serial.println(dataArray[i]);
  // }

  return;
}

void saveArrayToCSV(const char* filename, float* array, size_t arraySize) {
  // Check if LittleFS is mounted
  Serial.println("Save CSV to LittleFS");
  if (!LittleFS.begin()) {
    Serial.println("Failed to mount LittleFS");
    return;
  }
  
  // Open the file for writing
  File file = LittleFS.open(filename, FILE_WRITE);
  
  // Check if the file opened successfully
  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }
  
  // Write array elements in CSV format
  for (size_t i = 0; i < arraySize; i++) {
    file.print(String(array[i]));
    
    // Add a comma between elements except the last one
    if (i < arraySize - 1) {
      file.print(",");
    }
  }
  
  file.println(); // New line after writing the array
  
  // Close the file after writing
  file.close();
  
  // Success message
  Serial.println("Array saved to CSV file successfully");
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
      // Initialize the allocated memory to 0
      memset(array, 0, requiredMemory); // Set all bytes to 0
      
      Serial.println("Float array allocated and initialized to 0 successfully.");
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



#endif