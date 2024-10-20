#ifndef CONFIG_H
#define CONFIG_H

#include "Sensor.h"
#include <sml/sml_file.h>

# define MY_TEST 0                 // use test mode, no sensor data used, only test data is sent to VZ_UUID_TEST
#define MY_TEST_SEND_UPDATE 5000  // ms

// sensor config
static const SensorConfig SENSOR_CONFIGS[] = {
    {.pin = 3,                                 // input pin
     .name = "yourMeterName",                   // name of meter for debug and MQTT
     .numeric_only = false,
     .interval = 5}};                           // read out interval in sec, 0=no wait
const uint8_t NUM_OF_SENSORS = sizeof(SENSOR_CONFIGS) / sizeof(SensorConfig);


// build in LED is inverted for Wemos D1 mini
#define LED_BUILTIN_ON  0
#define LED_BUILTIN_OFF 1

// Special Heart Beat values
#define HEART_BEAT_RESET 1
#define HEART_BEAT_WIFI_CONFIG 2

// Verbose Level
#define VERBOSE_LEVEL_WLAN  1
#define VERBOSE_LEVEL_HTTP  0
#define VERBOSE_LEVEL_MeterData  1
#define VERBOSE_LEVEL_MeterProtocol  0
#define VERBOSE_LEVEL_Setup  1
#define VERBOSE_LEVEL_Loop 0
#define VERBOSE_LEVEL_TIME 0

#define DATE_UPDATE_INTERVAL 60000      // in ms; for Dash Board

// http transfer to data base
#define VZ_SERVER           "yourVolkszaehlerServer_name_or_IP"
#define VZ_MIDDLEWARE       "middleware.php"
#define VZ_DATA_JSON        "data.json"
#define VZ_UUID_NO_SEND     "null"        // use this uuid if you do not want to transmit data for a channel

// SMLReader channels: replace by your UUIDs created in VZ frontend
#define VZ_UUID_POWER_IN            "power-in"                              // 3 
#define VZ_UUID_ENERGY_OUT          "energy-out"                        	// 4
#define VZ_UUID_ENERGY_IN	        "energy-in"                            	// 5
#define VZ_UUID_TEST                "test"                                  // 13 for test
#define VZ_UUID_SML_HEART_BEAT      "sml-heart-beat"                        // 16 debug channel

// OBIS identifier for used channels
#define OBIS_ID_ENERGY_IN   "1-0:1.8.0*255"
#define OBIS_ID_ENERGY_OUT  "1-0:2.8.0*255"
#define OBIS_ID_POWER_IN    "1-0:16.7.0*255"

#endif