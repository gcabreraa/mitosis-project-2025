#include <Arduino.h>
#include <WiFi.h>
#include <string.h>
#include "Adafruit_SHT4x.h"
#include "esp_adc_cal.h"
#include "esp_sleep.h"
#include <Wire.h>
#include <MPU6050.h>
#include <SPI.h>
#include <FS.h>
#include <SPIFFS.h>
#include <SparkFun_VEML7700_Arduino_Library.h> // Ambient light sensor
#include "Adafruit_MAX1704X.h"                 // Battery state of charge
#include <GxEPD2_BW.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>\

// Define constants
#define DEVICE_ID 1  // unique identifier for the system
#define DEVICE_LOC "zesiger_test" // where we place the system
#define RESPONSE_TIMEOUT 30000  // milliseconds to wait for HTTP response
// #define SLEEP_TIME_US 5 * 60 * 1000000ULL
#define SLEEP_TIME_US 10  * 1000000ULL
#define MAX_WIFI_RETRIES 5
<<<<<<< HEAD
#define BATTERY_THRESHOLD 2.0
#define I2C_SDA 5
#define I2C_SCL 6 // TESTING CHANGE
#define BAT 0
  #define EINK_CLK_PIN 8
  #define EINK_DIN_PIN 7
  #define EINK_CS_PIN  9
  #define EINK_DC_PIN  1
  #define EINK_RST_PIN 2
  #define EINK_BUSY_PIN 3
#define LED_RED 2
#define LED_GREEN 3
#define LED_BLUE 9
=======
// #define BATTERY_THRESHOLD 2.0
#define SOC_THRESHOLD 15.0
#define TXD1 6
#define RXD1 7
#define I2C_SDA 4
#define I2C_SCL 5
#define EINK_CS_PIN  9
#define EINK_DC_PIN  1
#define EINK_RST_PIN 2
#define EINK_BUSY_PIN 3
#define EINK_CLK_PIN 8
#define EINK_DIN_PIN 0
#define GxEPD2_DISPLAY_CLASS GxEPD2_BW
#define GxEDP2_DRIVER_CLASS GxEPD2_290_T94_V2
>>>>>>> 7ecc30465ec0ecb377a0f5ea692a9b12b1a3e6a2

#define MAX_RTC_READINGS 5
#define MAX_ERROR_LOGS 8
#define RECORD_SIZE sizeof(SensorData)
// #define UPLOAD_INTERVAL_SEC 3600
#define UPLOAD_INTERVAL_SEC 60 // TESTING CHANGE
#define MAX_FLASH_RECORDS 43690
RTC_DATA_ATTR int flashReadIndex = 0;
RTC_DATA_ATTR int flashWriteIndex = 0;

#define FLASH_FILE "/data.bin"

// 'efpi-21', 100x100px
const unsigned char epd_bitmap_efpi_21 [] PROGMEM = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 
	0xe0, 0x00, 0x03, 0xfc, 0x1f, 0x9f, 0x80, 0x0c, 0x00, 0x00, 0x7f, 0xf0, 0xff, 0xe0, 0x00, 0x03, 
	0xfc, 0x1f, 0x1f, 0x80, 0x1c, 0x00, 0x00, 0x7f, 0xf0, 0xff, 0xe2, 0xaa, 0xa3, 0xf2, 0xa7, 0xdd, 
	0x41, 0x0c, 0x55, 0x54, 0x7f, 0xf0, 0xff, 0xe7, 0xff, 0xe7, 0xe3, 0xe3, 0xfc, 0x73, 0x9c, 0x7f, 
	0xfc, 0x7f, 0xf0, 0xff, 0xe3, 0xff, 0xe3, 0xe3, 0xe3, 0xfc, 0x63, 0x8c, 0x7f, 0xfe, 0x7f, 0xf0, 
	0xff, 0xe7, 0x00, 0xe3, 0xe3, 0xff, 0x83, 0x9f, 0x9c, 0x70, 0x1c, 0x7f, 0xf0, 0xff, 0xe3, 0x00, 
	0xe3, 0xe7, 0xff, 0x03, 0x8f, 0x8c, 0x70, 0x0e, 0x7f, 0xf0, 0xff, 0xe7, 0x00, 0xe7, 0xe3, 0xff, 
	0x83, 0x9f, 0x1c, 0x60, 0x0c, 0x7f, 0xf0, 0xff, 0xe3, 0x00, 0xe3, 0xe0, 0x1c, 0x7f, 0xe0, 0x0c, 
	0x70, 0x1e, 0x7f, 0xf0, 0xff, 0xe7, 0x00, 0xe3, 0xe0, 0x1c, 0xff, 0xf0, 0x1c, 0x70, 0x0c, 0x7f, 
	0xf0, 0xff, 0xe3, 0x00, 0xe3, 0xe2, 0x94, 0xff, 0xa1, 0x5c, 0x60, 0x0e, 0x7f, 0xf0, 0xff, 0xe7, 
	0x00, 0xe7, 0xe3, 0xe3, 0xff, 0x83, 0xfc, 0x70, 0x1c, 0x7f, 0xf0, 0xff, 0xe3, 0x00, 0xe3, 0xe3, 
	0xe3, 0xff, 0x83, 0xfc, 0x70, 0x0e, 0x7f, 0xf0, 0xff, 0xe7, 0xff, 0xe3, 0x9c, 0xe3, 0xeb, 0xec, 
	0x1c, 0x7f, 0xfc, 0x7f, 0xf0, 0xff, 0xe3, 0xff, 0xe3, 0x1c, 0x63, 0xe3, 0xfc, 0x1c, 0x7f, 0xfc, 
	0x7f, 0xf0, 0xff, 0xe7, 0xff, 0xe7, 0x1c, 0xe3, 0xe3, 0xfc, 0x0c, 0x7f, 0xfe, 0x7f, 0xf0, 0xff, 
	0xe0, 0x00, 0x03, 0x9c, 0x63, 0x9c, 0x63, 0x9c, 0x00, 0x00, 0x7f, 0xf0, 0xff, 0xe0, 0x00, 0x03, 
	0x1c, 0xe3, 0x1c, 0x73, 0x8c, 0x00, 0x00, 0x7f, 0xf0, 0xff, 0xe2, 0x22, 0x27, 0x94, 0x6b, 0x9d, 
	0x63, 0x9e, 0xa4, 0x44, 0x7f, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x1c, 0x7f, 0x83, 0x8f, 0xff, 
	0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x1c, 0xff, 0x83, 0x9f, 0xff, 0xff, 0xff, 0xf0, 
	0xff, 0xef, 0xeb, 0x97, 0xad, 0xa5, 0x24, 0xab, 0xa7, 0xaf, 0xff, 0xff, 0xf0, 0xff, 0xe3, 0xe7, 
	0x03, 0x1f, 0xe3, 0x80, 0x7f, 0xf3, 0x8f, 0xff, 0xff, 0xf0, 0xff, 0xe7, 0xe3, 0x03, 0x9f, 0xe3, 
	0x00, 0x7f, 0xe3, 0x9f, 0xff, 0xff, 0xf0, 0xff, 0xe0, 0xfc, 0x1f, 0xe0, 0xfc, 0xfc, 0xff, 0x9c, 
	0x0c, 0x7e, 0x7f, 0xf0, 0xff, 0xe0, 0xf8, 0x1f, 0xe0, 0x7c, 0x7c, 0x7f, 0x8c, 0x0e, 0x7c, 0x7f, 
	0xf0, 0xff, 0xe0, 0xfc, 0x9f, 0xe0, 0xfc, 0x7c, 0x7f, 0x1e, 0x1c, 0x7c, 0x7f, 0xf0, 0xff, 0xe0, 
	0xe3, 0xe0, 0x63, 0x1c, 0x1c, 0x0c, 0x7f, 0x80, 0x03, 0xff, 0xf0, 0xff, 0xe0, 0xe7, 0xe0, 0xe3, 
	0x9c, 0x1c, 0x1c, 0x7f, 0x80, 0x01, 0xff, 0xf0, 0xff, 0xe0, 0x03, 0xaa, 0xa3, 0x1c, 0x5c, 0x5c, 
	0x75, 0x00, 0x03, 0xff, 0xf0, 0xff, 0xe0, 0x07, 0x1f, 0x03, 0x9c, 0x7c, 0x7c, 0x70, 0x00, 0x01, 
	0xff, 0xf0, 0xff, 0xe0, 0x03, 0x1f, 0x83, 0x1c, 0xfc, 0x7c, 0x60, 0x00, 0x03, 0xff, 0xf0, 0xff, 
	0xff, 0x00, 0xe0, 0xfc, 0x03, 0x00, 0x1f, 0x9c, 0x7c, 0x60, 0x7f, 0xf0, 0xff, 0xff, 0x00, 0xe0, 
	0x7c, 0x03, 0x80, 0x1f, 0x8c, 0x7c, 0x70, 0x7f, 0xf0, 0xff, 0xff, 0x00, 0xe0, 0xfc, 0x03, 0x00, 
	0x0f, 0x9c, 0x7c, 0x70, 0x7f, 0xf0, 0xff, 0xe3, 0xfc, 0x1f, 0x18, 0x1c, 0x1f, 0x80, 0x0f, 0x8f, 
	0x8f, 0xff, 0xf0, 0xff, 0xe7, 0xfc, 0x1f, 0x9c, 0x1c, 0x1f, 0x80, 0x1f, 0x8f, 0x8f, 0xff, 0xf0, 
	0xff, 0xe5, 0xf4, 0xaf, 0x1d, 0x08, 0x1f, 0x8a, 0x9e, 0xaf, 0x8d, 0xff, 0xf0, 0xff, 0xf8, 0xe3, 
	0xe3, 0x1f, 0x80, 0x1f, 0x9f, 0xfc, 0x71, 0x80, 0x7f, 0xf0, 0xff, 0xfc, 0xe7, 0xe3, 0x9f, 0x00, 
	0x1f, 0x0f, 0xfc, 0x73, 0x80, 0x7f, 0xf0, 0xff, 0xf8, 0xf4, 0x2c, 0x1f, 0x9f, 0xc3, 0x9f, 0x9f, 
	0x80, 0xe1, 0xff, 0xf0, 0xff, 0xfc, 0xf8, 0x1c, 0x1f, 0x1f, 0xe3, 0x9f, 0x8f, 0x80, 0x73, 0xff, 
	0xf0, 0xff, 0xf8, 0xfc, 0x18, 0x1f, 0x9f, 0xe3, 0x8f, 0x9f, 0x80, 0x71, 0xff, 0xf0, 0xff, 0xff, 
	0x00, 0x00, 0xe0, 0x1c, 0x73, 0x9f, 0x8f, 0xfc, 0x63, 0xff, 0xf0, 0xff, 0xff, 0x00, 0x00, 0xe0, 
	0x1c, 0x63, 0x8f, 0x9f, 0xfe, 0x73, 0xff, 0xf0, 0xff, 0xff, 0x22, 0x4a, 0x60, 0x14, 0x6a, 0x9f, 
	0x8d, 0xdc, 0x71, 0xff, 0xf0, 0xff, 0xf8, 0xff, 0xff, 0x00, 0xe0, 0x1c, 0x63, 0x80, 0x0f, 0xfe, 
	0x7f, 0xf0, 0xff, 0xfc, 0xff, 0xff, 0x80, 0x60, 0x1c, 0x73, 0x80, 0x1f, 0xfc, 0x7f, 0xf0, 0xff, 
	0xea, 0x97, 0xeb, 0x14, 0x5b, 0x1c, 0x43, 0x81, 0xa5, 0x74, 0x7f, 0xf0, 0xff, 0xe3, 0x03, 0xe3, 
	0x1c, 0x1f, 0x8c, 0x03, 0x83, 0xf0, 0x70, 0x7f, 0xf0, 0xff, 0xe7, 0x07, 0xe7, 0x9c, 0x1f, 0x1c, 
	0x03, 0x83, 0xf0, 0x70, 0x7f, 0xf0, 0xff, 0xff, 0xff, 0x18, 0x00, 0x03, 0x83, 0x9f, 0x03, 0x8f, 
	0xf0, 0x7f, 0xf0, 0xff, 0xff, 0xff, 0x1c, 0x00, 0x03, 0x83, 0x8f, 0x83, 0x8f, 0xe0, 0x7f, 0xf0, 
	0xff, 0xff, 0xff, 0x98, 0x00, 0x03, 0x03, 0x9f, 0x81, 0x9f, 0xf0, 0x7f, 0xf0, 0xff, 0xe3, 0x03, 
	0xe0, 0x03, 0x83, 0xff, 0xff, 0x80, 0x01, 0x8f, 0xff, 0xf0, 0xff, 0xe7, 0x07, 0xe0, 0x03, 0x03, 
	0xff, 0xff, 0x80, 0x03, 0x8f, 0xff, 0xf0, 0xff, 0xeb, 0xab, 0xea, 0x03, 0xc3, 0xbe, 0xff, 0x8a, 
	0xa3, 0x8a, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x83, 0xe3, 0x9c, 0x7f, 0x9f, 0xf3, 0x80, 0x7f, 
	0xf0, 0xff, 0xff, 0xff, 0xff, 0x03, 0xe3, 0x1c, 0x7f, 0x8f, 0xe1, 0x80, 0x7f, 0xf0, 0xff, 0xe0, 
	0x00, 0x07, 0xff, 0x9f, 0xfc, 0x1f, 0x9c, 0xf3, 0xe3, 0xff, 0xf0, 0xff, 0xe0, 0x00, 0x03, 0xff, 
	0x1f, 0xfc, 0x1f, 0x8c, 0x73, 0xf1, 0xff, 0xf0, 0xff, 0xe1, 0x24, 0x87, 0xff, 0x9f, 0xfc, 0x1f, 
	0x9c, 0x63, 0xf3, 0xff, 0xf0, 0xff, 0xe7, 0xff, 0xe3, 0x1f, 0x1f, 0x9f, 0xe3, 0x8f, 0xf1, 0x8c, 
	0x7f, 0xf0, 0xff, 0xe3, 0xff, 0xe3, 0x9f, 0x9f, 0x0f, 0xf3, 0x9f, 0xf3, 0x8e, 0x7f, 0xf0, 0xff, 
	0xe7, 0xb6, 0xe7, 0x5f, 0x1f, 0xbe, 0xcb, 0x8b, 0x43, 0xac, 0x7f, 0xf0, 0xff, 0xe3, 0x00, 0xe3, 
	0xff, 0x9f, 0xfc, 0x1f, 0x80, 0x03, 0xfe, 0x7f, 0xf0, 0xff, 0xe7, 0x00, 0xe3, 0xff, 0x1f, 0xfc, 
	0x0f, 0x80, 0x03, 0xfc, 0x7f, 0xf0, 0xff, 0xe3, 0x00, 0xe3, 0x92, 0xdf, 0xff, 0x1c, 0x6c, 0x60, 
	0x73, 0x7f, 0xf0, 0xff, 0xe7, 0x00, 0xe7, 0x00, 0x7f, 0xff, 0x9c, 0x7c, 0x70, 0x73, 0xff, 0xf0, 
	0xff, 0xe3, 0x00, 0xe3, 0x00, 0xff, 0xff, 0x8c, 0xfc, 0x70, 0x71, 0xff, 0xf0, 0xff, 0xe7, 0x00, 
	0xe3, 0xe0, 0x00, 0x1f, 0x1f, 0x9f, 0xe0, 0x7e, 0x7f, 0xf0, 0xff, 0xe3, 0x00, 0x63, 0xe0, 0x00, 
	0x1f, 0x9f, 0x8f, 0xf0, 0x7c, 0x7f, 0xf0, 0xff, 0xe7, 0xa4, 0xe7, 0xe0, 0x12, 0x4e, 0x8f, 0x1f, 
	0xd0, 0xfc, 0x7f, 0xf0, 0xff, 0xe3, 0xff, 0xe3, 0xe0, 0xff, 0xe0, 0x1c, 0x0c, 0x03, 0xf3, 0xff, 
	0xf0, 0xff, 0xe7, 0xff, 0xe3, 0xe0, 0x7f, 0xe0, 0x0c, 0x1c, 0x03, 0xe1, 0xff, 0xf0, 0xff, 0xe1, 
	0x55, 0x43, 0xad, 0xab, 0xf8, 0x1e, 0x85, 0x00, 0xf3, 0xff, 0xf0, 0xff, 0xe0, 0x00, 0x07, 0x1f, 
	0x03, 0xfc, 0x1f, 0x83, 0x80, 0x71, 0xff, 0xf0, 0xff, 0xe0, 0x00, 0x03, 0x9f, 0x83, 0xfc, 0x0f, 
	0x83, 0x80, 0x73, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xf0
};


// Sensor/IMU
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
MPU6050 mpu;
VEML7700 veml;
GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> display(
  GxEPD2_290_GDEY029T94(EINK_CS_PIN, EINK_DC_PIN, EINK_RST_PIN, EINK_BUSY_PIN)
);
HardwareSerial mySerial(1);
RTC_DATA_ATTR bool einkInitialized = false;
Adafruit_MAX17048 maxlipo; 

// Wi-Fi
const char* server = "efpi-21.mit.edu";

// Structure for allowed networks
struct WifiNetwork {
  const char* ssid;
  const char* password;
};

// List of allowed networks (no password or we know the password)
WifiNetwork allowedNetworks[] = {
  {"EECS_Labs", ""},
  {"608_24G", "608g2020"},
  {"StataCenter", ""},
  {"MIT", "mnY$2#[sFY"},
  {"RLE", ""}
};
const int allowedNetworksCount = sizeof(allowedNetworks) / sizeof(allowedNetworks[0]);

// Global variables to store the selected network's info
String targetSSID = "";
String targetPassword = "";
uint8_t targetBSSID[6] = {0};
int targetChannel = 0;
int bestRSSI = -999;  // Initialized with a very low value

// RTC structs - 5 * 4 bytes (float) + 4 bytes (uint32_t) = 24 bytes 
RTC_DATA_ATTR struct SensorData {
  float temperature;
  float humidity;
  // float battery;
  float lux;
  float soc;  // state of charge
  float timestamp;
} rtcBuffer[MAX_RTC_READINGS];

RTC_DATA_ATTR int rtcWriteIndex = 0;
RTC_DATA_ATTR uint32_t lastUploadTime = 0;

RTC_DATA_ATTR struct ErrorLog {
  uint32_t timestamp;
  char type[16];
  char message[48];
} rtcErrorLogs[MAX_ERROR_LOGS];

RTC_DATA_ATTR int errorLogIndex = 0;

// State machine
enum SystemState { STATE_NORMAL, STATE_WIFI_UPLOAD, STATE_SENSOR_ERROR, STATE_SOC_LOW, STATE_LORA };
RTC_DATA_ATTR SystemState currentState = STATE_NORMAL;

// Buffers
const uint16_t IN_BUFFER_SIZE = 400;
const uint16_t OUT_BUFFER_SIZE = 400;
char request_buffer[IN_BUFFER_SIZE];
char response_buffer[OUT_BUFFER_SIZE];

// IMU Variables
const float shakeThreshold = 0.02; // Adjust this value based on sensitivity requirements (in g)
const int numReadings = 10; // Number of readings to average for smoothing

// Global Variables
// float batteryVoltage = 0;
int wifiRetries = 0;

// Function Starters
String encodeSensorData(const SensorData& data);
void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial);
uint8_t char_append(char* buff, char c, uint16_t buff_size);
float read_analog_voltage(int gpio_pin, int num_average);
bool connectWiFi();
void enterDeepSleep();
void logError(const char* type, const char* msg);
bool isShaken();
// void setLEDColor(bool red, bool green, bool blue);
void measureAndStoreData();
void writeRTCBufferToFlash();
void uploadFlashToServer();
void uploadErrorLogs();
void printFlashContents();
void dumpRTCBuffer(); 
void dumpFlashData();
void dumpRTCErrors();
void sendToLoRa(const SensorData& data);
void updateEInkDisplay(const SensorData& data);
void sendToLoRa();

void setup() {
  Serial.begin(115200); //begin serial
  delay(2000);
  Serial.print("System Starting...");
  SPI.begin(EINK_CLK_PIN, -1, EINK_DIN_PIN, EINK_CS_PIN);

  if (!einkInitialized) {
    display.init(0, true, 2, false);  // full init with reset and full refresh
    einkInitialized = true;
  } else {
    display.init(0, false, 2, false);
  }

  Wire.begin(I2C_SDA, I2C_SCL);
  SPIFFS.begin(true);

  // mySerial.begin(115200, SERIAL_8N1, RXD1, TXD1);

  // esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  // if (wakeup_reason != ESP_SLEEP_WAKEUP_TIMER) {
  //   currentState = STATE_NORMAL;
  //   // printFlashContents();
  // }

  // Initialize MPU6050 // TESTING CHANGE
  // mpu.initialize();
  // if (!mpu.testConnection()) {
  //   Serial.println("IMU failure");
  //   // setLEDColor(true, true, false);  // Yellow for sensor error
  //   logError("SENSOR", "MPU6050 failed");
  //   currentState = STATE_SENSOR_ERROR;
  //   return;
  // }
  // Serial.println("MPU6050 connected");

  if (!sht4.begin()) {
    Serial.println("SHT4x failure");
    // setLEDColor(true, true, false);
    logError("SENSOR", "SHT4x failed");
    currentState = STATE_SENSOR_ERROR;
    return;
  }

  Serial.println("Found SHT4x sensor");

  while (!veml.begin()) {
    Serial.println("VEMLfailure");
    logError("SENSOR", "VEML7700 failed");
    // setLEDColor(true, true, false);  // Yellow LED
    currentState = STATE_SENSOR_ERROR;
    return;
  }
  Serial.println("VEML7700 connected");

  // TO-DO: Fuel gauge currently not working, so commented out for testing
  while (!maxlipo.begin()) {
    Serial.println("Fuel gauge failure");
    logError("SENSOR", "MAX17048 failed");
    // setLEDColor(true, true, false);
    currentState = STATE_SENSOR_ERROR;
    return;
  }
  Serial.println("MAX17048 connected");
  Serial.print("SOC (%): "); Serial.println(maxlipo.cellPercent());

  // You can have 3 different precisions, higher precision takes longer
  sht4.setPrecision(SHT4X_HIGH_PRECISION);

  // read battery voltage
  // batteryVoltage = read_analog_voltage(BAT, 10) * 1.68;
  float soc = maxlipo.cellPercent();


  // Low battery check
  // if (soc < SOC_THRESHOLD) {
  //   Serial.println("Low SoC detected. Entering deep sleep...");
  //   currentState = STATE_SOC_LOW;
  //   return;
  // }  
  
  // uint32_t now = millis() / 1000; // TESTING CHANGE
  // if ((now - lastUploadTime) > UPLOAD_INTERVAL_SEC) {
  // if (rtcWriteIndex >= MAX_RTC_READINGS) {
  //   currentState = STATE_WIFI_UPLOAD;
  //   rtcWriteIndex = 0;
  // }

  if (currentState != STATE_SOC_LOW && currentState != STATE_SENSOR_ERROR && currentState != STATE_WIFI_UPLOAD) {
    currentState = STATE_NORMAL;
  }
  
  if (currentState == STATE_NORMAL) {
    Serial.println("Setup complete. Ready to begin loop.");
  }

}

/*-----------------------------------
  Switches through different possible states of our system.
*/
void loop() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();

    if (command == "DUMP_RTC") {
      dumpRTCBuffer();
    } else if (command == "DUMP_FLASH") {
      dumpFlashData();
    } else if (command == "CLEAR_FLASH") {
      SPIFFS.remove(FLASH_FILE);
      Serial.println("Flash cleared.");
    } else if (command == "DUMP_ERRORS") {
      dumpRTCErrors();
    } else if (command == "CLEAR_ERRORS") {
      errorLogIndex = 0;
      Serial.println("RTC error buffer cleared.");
    } else {
      Serial.println("Unknown command.");
    }
  }

  switch (currentState) {
    case STATE_NORMAL:
      Serial.println("In STATE_NORMAL");
      // TESTING CHANGE
      // if (isShaken()) {
      //   logError("SHAKE", "Device shaken");
      // }
      measureAndStoreData();
      // delay(1000*60);
      enterDeepSleep();
      break;

    case STATE_WIFI_UPLOAD:
      Serial.println("In STATE_WIFI_UPLOAD");
      if (connectWiFi()) {
        Serial.println("Uploading flash data to server...");	
        uploadFlashToServer();
        Serial.println("attempted uploading flash data to server");
        uploadErrorLogs();
        SPIFFS.remove(FLASH_FILE);
        lastUploadTime = millis() / 1000; // TO-DO: needs to be fixed to persist over reset/deep sleep
        currentState = STATE_NORMAL;
      } else {
        Serial.println("Failed to upload");
        logError("WIFI", "Upload failed");
        // setLEDColor(false, false, true);  // Blue
      }
      enterDeepSleep();
      break;

    case STATE_SENSOR_ERROR:
      Serial.println("In STATE_SENSOR_ERROR");
      // setLEDColor(true, true, false);  // Yellow
      uploadErrorLogs();
      enterDeepSleep();
      break;

    case STATE_SOC_LOW:
      Serial.println("In STATE_LOW_BATTERY");
      // setLEDColor(true, false, false);  // Red
      uploadErrorLogs();
      enterDeepSleep();
      break;

    case STATE_LORA:
      Serial.println("In STATE_LORA");
      mySerial.println(1235);
      delay(1000);
      sendToLoRa();
      printFlashContents();
      break;
  }

}

//////////////////////////////////////////////
////////////// Functions /////////////////////
/////////////////////////////////////////////// 

// UART DUMP Functions
/******************************************************************************
 * Dump the contents of the RTC buffer to the serial monitor.
 * 
 ******************************************************************************/
void dumpRTCBuffer() {
  Serial.println("Dumping RTC buffer:");
  for (int i = 0; i < MAX_RTC_READINGS; i++) {
    SensorData data = rtcBuffer[i];
    Serial.printf("RTC[%02d] = T: %.2f, H: %.2f, SoC: %.2f, TS: %lu\n",
      i, data.temperature, data.humidity, data.soc, data.timestamp);
  }
}

/******************************************************************************
 * Dump the contents of the flash file to the serial monitor.
 * 
 ******************************************************************************/
void dumpFlashData() {
  File file = SPIFFS.open(FLASH_FILE, FILE_READ);
  if (!file || file.size() == 0) {
    Serial.println("No data in flash.");
    return;
  }

  int index = 0;
  while (file.available()) {
    SensorData data;
    file.readBytes((char*)&data, sizeof(SensorData));
    Serial.printf("[%02d] T: %.2f, H: %.2f, SoC: %.2f, TS: %lu\n",
                  index++, data.temperature, data.humidity, data.soc, data.timestamp);
  }
  file.close();
}

/******************************************************************************
 * Dump the RTC error logs to the serial monitor.
 * 
 ******************************************************************************/
void dumpRTCErrors() {
  Serial.println("Dumping RTC error logs:");
  for (int i = 0; i < errorLogIndex; i++) {
    Serial.printf("Error[%02d] = [%s] %s @ TS: %lu\n",
      i, rtcErrorLogs[i].type, rtcErrorLogs[i].message, rtcErrorLogs[i].timestamp);
  }
}

/******************************************************************************
 * Measure and store data from each of the sensors. The data is stored in the
 * rtcBuffer array. If the buffer is full, it is written to flash memory.
 * 
 * Data stored includes:
 * - Temperature (in C) from SHT4x
 * - Humidity (in %) from SHT4x
 * - Lux (in Lux) from VEML7700
 * - Battery voltage (in V) from MAX17048
 *  
 ******************************************************************************/
void measureAndStoreData() {
  float lux = veml.getLux();
  float soc = maxlipo.cellPercent();  // state of charge
  // float soc = 0;
  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp);
  if (isnan(temp.temperature) || isnan(humidity.relative_humidity)) {
    logError("SENSOR", "Failed reading");
    currentState = STATE_SENSOR_ERROR;
    return;
  }

  // float battery = read_analog_voltage(BAT, 10) * 1.68;
  float battery = maxlipo.cellVoltage();
  // float battery = 0;

  rtcBuffer[rtcWriteIndex] = { temp.temperature, humidity.relative_humidity, lux, soc, (float)rtcWriteIndex }; // TESTING CHANGE
  Serial.printf("RTC[%d] = T: %.2f, H: %.2f, Lux: %.2f, SoC: %.2f\n",
    rtcWriteIndex, rtcBuffer[rtcWriteIndex].temperature, rtcBuffer[rtcWriteIndex].humidity, rtcBuffer[rtcWriteIndex].lux,
    rtcBuffer[rtcWriteIndex].soc);
  rtcWriteIndex++;
  //E-INK UPDATING
  // updateEInkDisplay(rtcBuffer[rtcWriteIndex-1]);

  if (rtcWriteIndex >= MAX_RTC_READINGS) {
    // rtcWriteIndex = 0;
    Serial.println("RTC full! Dumping buffer:");
    for (int i = 0; i < MAX_RTC_READINGS; i++) {
      Serial.printf("  [%02d] T: %.2f, H: %.2f, Lux: %.2f, SoC: %.2f", i, rtcBuffer[i].temperature, rtcBuffer[i].humidity, rtcBuffer[i].lux, rtcBuffer[i].soc);
    }
    writeRTCBufferToFlash();
    display.init(0, true, 2, false);  // full init with reset and full refresh
    updateEInkDisplay(rtcBuffer[MAX_RTC_READINGS - 1]); // display latest reading
  }
}

/******************************************************************************
 * Write the RTC buffer to flash memory.
 * 
 ******************************************************************************/
void writeRTCBufferToFlash() {
  Serial.println("Writing RTC buffer to flash (archival only, FIFO style)...");

  File file = SPIFFS.open(FLASH_FILE, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open flash file!");
    return;
  }

  for (int i = 0; i < MAX_RTC_READINGS; i++) {
    size_t offset = flashWriteIndex * sizeof(SensorData);
    if (!file.seek(offset, SeekSet)) {
      Serial.printf("Failed to seek to offset %d\n", offset);
      file.close();
      return;
    }

    file.write((uint8_t*)&rtcBuffer[i], sizeof(SensorData));
    Serial.printf("  -> Written RTC[%d] to flash index %lu\n", i, flashWriteIndex);

    flashWriteIndex = (flashWriteIndex + 1) % MAX_FLASH_RECORDS;
  }

  file.close();
  Serial.println("RTC buffer archived to flash (FIFO style).");
}

/******************************************************************************
 * Print the contents of the flash file to the serial monitor.
 ******************************************************************************/
void printFlashContents() {
  File file = SPIFFS.open(FLASH_FILE, FILE_READ);
  if (!file || file.size() == 0) {
    Serial.println("No data in flash to read.");
    return;
  }

  Serial.println("Reading flash contents...");
  int idx = 0;
  while (file.available()) {
    SensorData data;
    file.readBytes((char*)&data, sizeof(SensorData));
    Serial.printf("[%02d] T: %.2f, H: %.2f, L:%.2f, SoC: %.2f\n", idx++, data.temperature, data.humidity, data.lux, data.soc);
  }
  file.close();
}

/******************************************************************************
 * Update the E-Ink display with the sensor data.
 * 
 * @param data: The sensor data to display.
 * 
 ******************************************************************************/
void updateEInkDisplay(const SensorData& data) {
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);

  // QR Code on left
  display.setPartialWindow(0, 14, 100, 100);
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.drawBitmap(0, 14, epd_bitmap_efpi_21, 100, 100, GxEPD_BLACK);
  } while (display.nextPage());

  // Title + sun icon
  display.setPartialWindow(110, 0, display.width()-100, 30);
  display.firstPage();
  do {
    display.fillRect(110, 0, 130, 30, GxEPD_WHITE);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(115, 15);
    display.print("MITOS Project '25 :)");
    // display.drawBitmap(display.width()-50, 5, sun_bitmap, 8, 8, GxEPD_BLACK);
  } while (display.nextPage());

  // Temp + Feels like
  display.setPartialWindow(110, 30, display.width() - 110, display.height()-30);
  display.firstPage();
  do {
    display.fillRect(110, 40, display.width() - 110, display.height()-30, GxEPD_WHITE);
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(115, 55);
    display.printf("%.1f C | %.1f F", data.temperature, data.temperature * 9 / 5 + 32);
    display.setCursor(115, 85);
    display.setFont(&FreeMono9pt7b);
    display.print("Feels like:");
    display.setCursor(115, 100);
    display.printf("%.1f C", data.temperature); // Approx
  } while (display.nextPage());
  delay(100);
  display.hibernate();
}

/******************************************************************************
 * Send the sensor data to the LoRa module.
 * 
 * @param data: The sensor data to send.
 * 
 ******************************************************************************/
void sendToLoRa() {
  Serial.println("Sending all RTC buffer contents to LoRa...");
  for (int i = 0; i < MAX_RTC_READINGS; i++) {
    String encodedStr = encodeSensorData(rtcBuffer[i]);
    encodedStr.trim();
    mySerial.print(encodedStr);
    Serial.println("LoRa Sent: " + encodedStr);
    delay(5000);
  }
  Serial.println("All RTC data sent to LoRa.");
  currentState = STATE_NORMAL;
}

String encodeSensorData(const SensorData& data) {
  auto encodeFloat = [](float val, float minVal, float maxVal, int offset = 0) -> String {
    int intPart = constrain((int)val, (int)minVal, (int)maxVal);
    int decPart = constrain((int)((val - intPart) * 10), 0, 9);
    char intChar = (char)(intPart + offset);
    char decChar = (char)(decPart + '0');
    return String(intChar) + decChar;
  };

  String encoded = "";
  encoded += encodeFloat(data.temperature, -50, 99, 100);  // offset ensures printable ASCII
  encoded += encodeFloat(data.humidity, 0, 100, 32);       // ASCII 32-132 range
  encoded += encodeFloat(data.lux, 0, 2000, 30);           // map lux in some range
  encoded += encodeFloat(data.soc, 0, 100, 32);            // SoC in %
  
  // Timestamp: index 0–100 as two-digit padded base-10 string (e.g. "00", "07", "42")
  int tsIndex = constrain((int)data.timestamp, 0, 99);
  if (tsIndex < 10) encoded += "0";
  encoded += String(tsIndex);

  return encoded;
}

/******************************************************************************
 * Turns on the LED with the specified color.
 * 
 * @param red: true to turn on the red LED, false to turn it off.
 * @param green: true to turn on the green LED, false to turn it off.
 * @param blue: true to turn on the blue LED, false to turn it off.
 * 
 ******************************************************************************/
// void setLEDColor(bool red, bool green, bool blue) {
//   digitalWrite(LED_RED, red ? HIGH : LOW);
//   digitalWrite(LED_GREEN, green ? HIGH : LOW);
//   digitalWrite(LED_BLUE, blue ? HIGH : LOW);
// }

/******************************************************************************
 * Calculate the average acceleration over a number of readings, specified
 * by global variable numReadings
 * 
 * @return the average acceleration in 'g'.
 ******************************************************************************/
float readAverageAcceleration() {
  float sum = 0;
  for (int i = 0; i < numReadings; i++) {
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);
    // Convert raw values to 'g'
    float x = ax / 16384.0;
    float y = ay / 16384.0;
    float z = az / 16384.0;
    // Calculate the magnitude of the acceleration vector
    float magnitude = sqrt(x * x + y * y + z * z);
    sum += magnitude;
    delay(10); // Short delay between readings
  }
  return sum / numReadings;
}

/******************************************************************************
 * Check if the device has been shaken based on the average acceleration.
 * 
 * Used for theft or falling detection.
 * 
 * @return true if shaken, false otherwise.
 ******************************************************************************/
bool isShaken() {
  float acceleration = readAverageAcceleration();
  // Subtract 1g to account for gravity
  float netAcceleration = fabs(acceleration - 1.0);
  // Serial.println(netAcceleration);
  return netAcceleration > shakeThreshold;
}



/******************************************************************************
 * Uploads the data stored in the flash file to the server.
 * Reads the data from the flash file and sends it to the server using HTTP POST.
 * 
 ******************************************************************************/
void uploadFlashToServer() {
  File file = SPIFFS.open(FLASH_FILE, FILE_READ);
  if (!file) return;

  while (file.available()) {
    Serial.println("parsing data from flash file");
    SensorData data;
    file.readBytes((char*)&data, sizeof(SensorData));
    
    char tempStr[10], humStr[10], batStr[10], tsStr[12];
    char luxStr[10], socStr[10];
    
    sprintf(tempStr, "%.2f", data.temperature);
    sprintf(humStr, "%.2f", data.humidity);
    // sprintf(batStr, "%.2f", data.battery);
    sprintf(tsStr, "%lu", data.timestamp);  // Send the timestamp (RTC counter value)
    sprintf(luxStr, "%.2f", data.lux);
    sprintf(socStr, "%.2f", data.soc);
    sprintf(request_buffer, "POST /logger?sensor_id=%d&loc=%s&temp=%s&rh=%s&bat=%s&lux=%s&ts=%s HTTP/1.1\r\n", DEVICE_ID, DEVICE_LOC, tempStr, humStr, socStr, luxStr, tsStr);
    
    strcat(request_buffer, "Host: efpi-21.mit.edu\r\n"); //add more to the end
    strcat(request_buffer, "\r\n"); //add blank line!
    
    do_http_GET("efpi-21.mit.edu",request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);    
    delay(100);
  }

  file.close();
}

/******************************************************************************
 * Upload the error logs stored in the RTC to the server.
 * 
 ******************************************************************************/
void uploadErrorLogs() {
  // if (!connectWiFi()) return;
  WiFiClient client;
  if (!client.connect(server, 80)) return;

  for (int i = 0; i < errorLogIndex; i++) {
    String json = "{\"error_type\":\"" + String(rtcErrorLogs[i].type) +
                  "\",\"error_msg\":\"" + String(rtcErrorLogs[i].message) + "\"}";

    String req = "POST /tunnel_gpcs/error_logger HTTP/1.1\r\n";
    req += "Host: " + String(server) + "\r\n";
    req += "Content-Type: application/json\r\n";
    req += "Content-Length: " + String(json.length()) + "\r\n\r\n" + json;

    client.print(req);
    delay(200);
  }

  errorLogIndex = 0;
}

/******************************************************************************
 * Connect the ESP32 to the best available WiFi network from the allowed list.
 * 
 * Scans for available networks, selects the best one based on signal strength,
 * and attempts to connect to it.
 * 
 * @return true if connected, false otherwise.
 ******************************************************************************/
bool connectWiFi() {
  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();

  // Do a scan for available networks and choose the best one
  if (n == 0) {
    Serial.println("No networks found");
    return false;
  } else {
    Serial.printf("%d networks found\n", n);
    // Reset bestRSSI (signal strength) for this scan
    bestRSSI = -999;
    // Loop through each found network
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      int rssi = WiFi.RSSI(i);
      int channel = WiFi.channel(i);
      
      // Print network details
      Serial.printf("%d: %s, Ch:%d (%ddBm) ", i + 1, ssid.c_str(), channel, rssi);
      uint8_t* bssidPtr = WiFi.BSSID(i);
      for (int k = 0; k < 6; k++) {
        Serial.printf("%02X", bssidPtr[k]);
        if (k < 5) Serial.print(":");
      }
      Serial.println();
      
      // Check if the network is in our allowed list
      for (int j = 0; j < allowedNetworksCount; j++) {
        if (ssid == allowedNetworks[j].ssid) {
          // If allowed and stronger than our current candidate, store it
          if (rssi > bestRSSI) {
            bestRSSI = rssi;
            targetSSID = ssid;
            targetPassword = String(allowedNetworks[j].password);
            targetChannel = channel;
            memcpy(targetBSSID, bssidPtr, 6);
          }
        }
      }
    }
  }

  // If an allowed candidate was found, print its info and connect
  if (targetSSID != "") {
    Serial.println("Best candidate found:");
    Serial.printf("SSID: %s\n", targetSSID.c_str());
    Serial.printf("Password: %s\n", targetPassword.c_str());
    Serial.printf("Channel: %d\n", targetChannel);
    Serial.printf("Signal Strength: %ddBm\n", bestRSSI);
    
    Serial.print("BSSID: ");
    for (int i = 0; i < 6; i++) {
      Serial.printf("%02X", targetBSSID[i]);
      if (i < 5) Serial.print(":");
    }
    Serial.println();

    // Connect using the specific channel and BSSID parameters.
    WiFi.begin(targetSSID.c_str(), targetPassword.c_str(), targetChannel, targetBSSID);
    
    // Wait for connection with a timeout
    Serial.print("Connecting");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      Serial.print(".");
      attempts++;
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
      Serial.printf("Connected! IP address: %s\n", WiFi.localIP().toString().c_str());
      return true;
    } else {
      Serial.println("Failed to connect to WiFi.");
    }
  } else {
    Serial.println("No allowed networks found in scan results.");
  }
  return false;
}

/******************************************************************************
 * Log an error message to the RTC error log buffer with a timestamp.
 * 
 ******************************************************************************/
void logError(const char* type, const char* msg) {
  if (errorLogIndex >= MAX_ERROR_LOGS) errorLogIndex = 0;
  strncpy(rtcErrorLogs[errorLogIndex].type, type, sizeof(rtcErrorLogs[errorLogIndex].type) - 1);
  strncpy(rtcErrorLogs[errorLogIndex].message, msg, sizeof(rtcErrorLogs[errorLogIndex].message) - 1);
  rtcErrorLogs[errorLogIndex].timestamp = millis() / 1000; // GIULI
  errorLogIndex++;
}

/******************************************************************************
 * Enter deep sleep mode for a specified duration.
 * 
 ******************************************************************************/
void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  esp_sleep_enable_timer_wakeup(SLEEP_TIME_US);
  Serial.println("Going to deep sleep now...");
  esp_deep_sleep_start();
}

/******************************************************************************
 * Appends a character to a buffer and null-terminates it.
 * 
 * @param buff: pointer to character array which we will append a char c:
 * @param c: character to append
 * @param buff_size: size of buffer buff
 * 
 * @return true if character appended, false if not appended (indicating buffer full)
 ******************************************************************************/
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
  int len = strlen(buff);
  if (len > buff_size) return false;
  buff[len] = c;
  buff[len + 1] = '\0';
  return true;
}

/******************************************************************************
 * Performs an HTTP GET request to a specified host and request string.
 * 
 * @param host: null-terminated char-array containing host to connect to
 * @param request: null-terminated char-arry containing properly formatted HTTP GET request
 * @param response: char-array used as output for function to contain response
 * @param response_size: size of response buffer (in bytes)
 * @param response_timeout: duration we'll wait (in ms) for a response from server
 * @param serial: used for printing debug information to terminal (true prints, false doesn't)
 * 
 ******************************************************************************/
void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial) {
  WiFiClient client; //instantiate a client object
  if (client.connect(host, 80)) { //try to connect to host on port 80
    if (serial) Serial.print(request);//Can do one-line if statements in C without curly braces
    client.print(request);
    memset(response, 0, response_size); //Null out (0 is the value of the null terminator '\0') entire buffer
    uint32_t count = millis();
    while (client.connected()) { //while we remain connected read out data coming back
      client.readBytesUntil('\n', response, response_size);
      if (serial) Serial.println(response);
      if (strcmp(response, "\r") == 0) { //found a blank line! (end of response header)
        break;
      }
      memset(response, 0, response_size);
      if (millis() - count > response_timeout) break;
    }
    memset(response, 0, response_size);  //empty in prep to store body
    count = millis();
    while (client.available()) { //read out remaining text (body of response)
      char_append(response, client.read(), OUT_BUFFER_SIZE);
    }
    if (serial) Serial.println(response);
    client.stop();
    if (serial) Serial.println("-----------");
  } else {
    if (serial) Serial.println("connection failed :/");
    if (serial) Serial.println("wait 0.5 sec...");
    client.stop();
  }
}

/******************************************************************************
 * Reads the analog voltage from a specified GPIO pin and returns the average
 * 
 * @param gpio_pin: GPIO pin number from which to read the analog value
 * @param num_average: Number of readings to average for smoothing the output
 * 
 * @return Returns the average voltage reading from the specified GPIO pin in volts
 * 
 ******************************************************************************/
float read_analog_voltage(int gpio_pin, int num_average)
{
  esp_adc_cal_characteristics_t adc_chars; // Structure to store ADC calibration characteristics
  int adc_readings_raw = 0;                // Variable to accumulate raw ADC readings

  // Loop to collect multiple ADC readings for averaging
  for (int i = 0; i < num_average; i++)
  {
    adc_readings_raw += analogRead(gpio_pin); // Add current reading to total
    delayMicroseconds(10);                    // Tiny delay to allow for ADC settling
  }
  adc_readings_raw = adc_readings_raw / num_average; // Calculate average of readings

  // Determine the ADC unit based on the GPIO pin number
  adc_unit_t current_adc = ADC_UNIT_1; // Default ADC unit
  if (gpio_pin == 5)
  {
    current_adc = ADC_UNIT_2; // Change ADC unit for specific pins, if necessary
  }

  // Characterize the ADC at 11 dB attenuation and 12-bit width for accurate voltage conversion
  esp_adc_cal_characterize(current_adc, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);

  // Convert the average raw reading to voltage in volts, using calibration characteristics
  return (esp_adc_cal_raw_to_voltage(adc_readings_raw, &adc_chars) / 1000.0);
}

