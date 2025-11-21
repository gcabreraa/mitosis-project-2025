#include <Arduino.h>
#include <string.h>
#include "Adafruit_SHT4x.h" // Temperature sensor library
#include "esp_adc_cal.h"
#include "esp_sleep.h"
#include <Wire.h>
#include <MPU6050.h> // IMU library
#include <SPI.h> 
#include <FS.h>
#include <SPIFFS.h> // Necessary for the flash
#include <SparkFun_VEML7700_Arduino_Library.h> // Ambient light sensor
#include "Adafruit_MAX1704X.h"                 // Battery state of charge
//E-Ink Libraries
#include <GxEPD2_BW.h> 
#include <icon_bitmaps.h> 
#include <Adafruit_GFX.h>
#include <Fonts/FreeMono9pt7b.h>
#include <Fonts/FreeMono12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold9pt7b.h>

// Define constants
#define SLEEP_TIME 10 * 1000000ULL // every five minutes
#define SOC_THRESHOLD 15.0
#define LORA_TX_TIMEOUT 8000UL
#define MAX_RTC_READINGS 5
#define MAX_ERROR_LOGS 50
#define FLASH_FILE "/data.bin"
#define RECORD_SIZE sizeof(SensorData)
#define MAX_FLASH_RECORDS 43690 // 150 days of data
RTC_DATA_ATTR int flashReadIndex = 0;
RTC_DATA_ATTR int flashWriteIndex = 0;
RTC_DATA_ATTR uint32_t lastMeasureTime = 0;
RTC_DATA_ATTR uint32_t loraStartTime = 0;

// PINS 
#define TXD1 6
#define RXD1 7
#define I2C_SDA 4
#define I2C_SCL 5
#define EINK_CS_PIN  3
#define EINK_DC_PIN  2
#define EINK_RST_PIN 1
#define EINK_BUSY_PIN 0
#define EINK_CLK_PIN 21
#define EINK_DIN_PIN 20
#define POWER_GATE 10

// E- Ink Bitmaps
extern const unsigned char lora_icon_bitmap[];     // 20x12
extern const unsigned char sensor_icon_bitmap[];   // 20x12
extern const unsigned char sun_bitmap[];           // 16x16
extern const unsigned char cloud_bitmap[];         // 16x16
extern const unsigned char snow_bitmap[];          // 16x16
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
	0xff, 0xff, 0xff, 0xf0};


// Sensor/IMU
Adafruit_SHT4x sht4 = Adafruit_SHT4x();
MPU6050 mpu;
GxEPD2_BW<GxEPD2_290_GDEY029T94, GxEPD2_290_GDEY029T94::HEIGHT> display(
  GxEPD2_290_GDEY029T94(EINK_CS_PIN, EINK_DC_PIN, EINK_RST_PIN, EINK_BUSY_PIN)
);
HardwareSerial mySerial(1);
RTC_DATA_ATTR bool einkInitialized = false; 
VEML7700 veml; // Light sensor
Adafruit_MAX17048 maxlipo; // UPDATE TO MAIN

unsigned long lora_tx_time = 0;
bool lora_done = true;

// RTC structs
RTC_DATA_ATTR struct SensorData {
  float temperature;
  float humidity;
  float lux;
  float soc;  // state of charge
  float timestamp; // RTC Index for now
} rtcBuffer[MAX_RTC_READINGS];

RTC_DATA_ATTR int rtcWriteIndex = 0;

RTC_DATA_ATTR struct ErrorLog {
  char type[8];
  char message[32];
} rtcErrorLogs[MAX_ERROR_LOGS];

RTC_DATA_ATTR int errorLogIndex = 0;
RTC_DATA_ATTR int loraSendCount = 0;
RTC_DATA_ATTR bool sentErrorLogs = false;

// State machine
enum SystemState { STATE_NORMAL, STATE_SENSOR_ERROR, STATE_SOC_LOW, STATE_LORA };
RTC_DATA_ATTR SystemState currentState = STATE_NORMAL;

// IMU Variables
const float shakeThreshold = 0.02; // Adjust this value based on sensitivity requirements (in g)
const int numReadings = 10; // Number of readings to average for smoothing


// Function Starters
String encodeSensorData(const SensorData& data);
void checkSerialCommands();
uint8_t char_append(char* buff, char c, uint16_t buff_size);
float read_analog_voltage(int gpio_pin, int num_average);
void enterDeepSleep();
void logError(const char* type, const char* msg);
bool isShaken();
void measureAndStoreData();
void writeRTCBufferToFlash();
String encodeErrorLogs();
void printFlashContents();
void dumpFlashToSerial();
void dumpErrorsToSerial();
void updateEInkDisplay(const SensorData& data, bool showSensors, bool showLora); 
void sendToLoRa(bool onlyErrors);

void setup() {

  Serial.begin(115200); //begin serial
  Serial.print("System Starting...");

  Wire.begin(I2C_SDA, I2C_SCL);
  SPIFFS.begin(true);
  pinMode(POWER_GATE, OUTPUT);
  digitalWrite(POWER_GATE, 1); // power off lora
  SPI.begin(EINK_CLK_PIN, -1, EINK_DIN_PIN, EINK_CS_PIN);


  // delay(10000);

  // while (!Serial) delay(10);  // Wait for USB serial connection

  // if (Serial) {
  //   Serial.println("---DEBUG_MODE---");
  //   dumpFlashToSerial();
  //   dumpErrorsToSerial();
  //   Serial.println("---DEBUG_DONE---");
  //   while (true);  // Prevent deep sleep
  // }

  if (!einkInitialized) {
    Serial.println("eink not work");
    display.init(0, true, 2, false);  // full init with reset and full refresh
    einkInitialized = true;
  } else {
    display.init(0, false, 2, false);
  }

  // mpu.initialize();

  // if (!mpu.testConnection()) {
  //   // setLEDColor(true, true, false);  // Yellow for sensor error
  //   Serial.print("MPU FAILED");
  //   logError("SENSOR", "MPU6050 failed");
  //   currentState = STATE_SENSOR_ERROR;
  //   return;
  // }
  // Serial.println("MPU6050 connected");

  if (!sht4.begin()) {
    // setLEDColor(true, true, false);
    Serial.print("sht4X FAILED");
    logError("SENSOR", "SHT4x failed");
    currentState = STATE_SENSOR_ERROR;
    return;
  }

  Serial.println("Found SHT4x sensor");

  while (!veml.begin()) {
    Serial.print("VEML Failed");
    logError("SENSOR", "VEML7700 failed");
    // setLEDColor(true, true, false);  // Yellow LED
    currentState = STATE_SENSOR_ERROR;
    return;
  }
  Serial.println("VEML7700 connected");

  while (!maxlipo.begin()) {
    Serial.print("MAXLIPO FAILED");
    logError("SENSOR", "MAX17048 failed");
    // setLEDColor(true, true, false);
    currentState = STATE_SENSOR_ERROR;
    return;
  }
  Serial.println("MAX17048 connected");
  Serial.print("Battery %: "); Serial.println(maxlipo.cellPercent());

  // You can have 3 different precisions, higher precision takes longer
  sht4.setPrecision(SHT4X_HIGH_PRECISION);
  switch (sht4.getPrecision()) {
     case SHT4X_HIGH_PRECISION:
       Serial.println("High precision");
       break;
     case SHT4X_MED_PRECISION:
       Serial.println("Med precision");
       break;
     case SHT4X_LOW_PRECISION:
       Serial.println("Low precision");
       break;
    
  }

   // You can have 6 different heater settings
  // higher heat and longer times uses more power
  // and reads will take longer too!
  sht4.setHeater(SHT4X_NO_HEATER);
  switch (sht4.getHeater()) {
     case SHT4X_NO_HEATER:
       Serial.println("No heater");
       break;
     case SHT4X_HIGH_HEATER_1S:
       Serial.println("High heat for 1 second");
       break;
     case SHT4X_HIGH_HEATER_100MS:
       Serial.println("High heat for 0.1 second");
       break;
     case SHT4X_MED_HEATER_1S:
       Serial.println("Medium heat for 1 second");
       break;
     case SHT4X_MED_HEATER_100MS:
       Serial.println("Medium heat for 0.1 second");
       break;
     case SHT4X_LOW_HEATER_1S:
       Serial.println("Low heat for 1 second");
       break;
     case SHT4X_LOW_HEATER_100MS:
       Serial.println("Low heat for 0.1 second");
       break;
  }

  float soc = maxlipo.cellPercent(); // Get state of charge in %
  // float soc = 95;
  Serial.printf("State of Charge: %.2f%%\n", soc);
  
  if (soc < SOC_THRESHOLD) {
    Serial.println("Low SoC detected. Entering deep sleep...");
    currentState = STATE_SOC_LOW;
    return;
  }  
  

  if (currentState != STATE_SOC_LOW && currentState != STATE_SENSOR_ERROR && currentState != STATE_LORA) {
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

  static bool firstLoop = true;
  unsigned long now = millis();
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

  switch (currentState) {
    case STATE_NORMAL:
      Serial.println("In STATE_NORMAL");
      // IF IT HAS IMU
      // if (isShaken()) {
      //   logError("SHAKE", "Device shaken");
      // }
      if (firstLoop || wakeup_reason == ESP_SLEEP_WAKEUP_TIMER ||
          now - lastMeasureTime > SLEEP_TIME/1000){
            firstLoop = false;
            lastMeasureTime = now;
            measureAndStoreData();
          }
      if (currentState == STATE_NORMAL){
        enterDeepSleep();
      }
      break;
    
    case STATE_LORA:
      if (loraStartTime == 0) {
        Serial.println("In STATE_LORA, starting LoRa transmission...");
        sendToLoRa(false);
        loraStartTime = now;
        lastMeasureTime = now;
      }

      if (now - loraStartTime < LORA_TX_TIMEOUT) {
        if (now - lastMeasureTime > SLEEP_TIME/1000) {
          measureAndStoreData();
          lastMeasureTime = now;
        }
      } else{
        Serial.println("LoRa transmission delay done, entering deep sleep...");
        digitalWrite(POWER_GATE, 1); // Turn off LORA
        SPIFFS.remove(FLASH_FILE);
        currentState = STATE_NORMAL;
        loraStartTime = 0;
        enterDeepSleep();
      }
      break;

    case STATE_SENSOR_ERROR:
      Serial.println("STATE_SENSOR_ERROR - sending error log via LoRa");
      digitalWrite(POWER_GATE, 0);
      delay(1000);
      sendToLoRa(true);
      updateEInkDisplay(rtcBuffer[(rtcWriteIndex == 0 ? MAX_RTC_READINGS : rtcWriteIndex) - 1], false, true);
      currentState = STATE_NORMAL;
      enterDeepSleep();
      break;

    case STATE_SOC_LOW:
      Serial.println("STATE_SOC_LOW - sending low battery log via LoRa");
      digitalWrite(POWER_GATE, 0);
      delay(1000);
      sendToLoRa(true);
      currentState = STATE_NORMAL;
      enterDeepSleep();
      break;

    default:
      enterDeepSleep();
      break;
  }

}

//////////////////////////////////////////////
////////////// Functions /////////////////////
/////////////////////////////////////////////// 

// Add this function to dump flash data over Serial
void dumpFlashToSerial() {
  File file = SPIFFS.open(FLASH_FILE, FILE_READ);
  if (!file || file.size() == 0) {
    Serial.println("---FLASH_EMPTY---");
    return;
  }

  Serial.println("---FLASH_DUMP_BEGIN---");
  while (file.available()) {
    char c = file.read();
    Serial.write(c);  // Send raw bytes
  }
  file.close();
  Serial.println("\n---FLASH_DUMP_END---");
}

// Add this function to dump error logs over Serial
void dumpErrorsToSerial() {
  Serial.println("---ERROR_LOG_BEGIN---");
  for (int i = 0; i < errorLogIndex; i++) {
    Serial.printf("{\"ts\": %lu, \"type\": \"%s\", \"msg\": \"%s\"}\n",
                  rtcErrorLogs[i].type,
                  rtcErrorLogs[i].message);
  }
  Serial.println("---ERROR_LOG_END---");
}

void measureAndStoreData() {
  // float lux = 454;
  float lux = veml.getLux();
  float soc = maxlipo.cellPercent();
  // float soc = 86.5;
  sensors_event_t humidity, temp;
  sht4.getEvent(&humidity, &temp);

  if (isnan(temp.temperature) || isnan(humidity.relative_humidity)) {
    logError("SENSOR", "Failed reading");
    currentState = STATE_SENSOR_ERROR;
    return;
  }

  float battery = maxlipo.cellVoltage();

  rtcBuffer[rtcWriteIndex] = { temp.temperature, humidity.relative_humidity, lux, soc, (float)rtcWriteIndex };

  Serial.printf("RTC[%d] = T: %.2f, H: %.2f, B: %.2f, Lux: %.2f, SoC: %.2f\n",
    rtcWriteIndex, rtcBuffer[rtcWriteIndex].temperature, rtcBuffer[rtcWriteIndex].humidity, rtcBuffer[rtcWriteIndex].lux, rtcBuffer[rtcWriteIndex].soc);

  rtcWriteIndex++;
  display.init(0, true, 2, false);  // full init with reset and full refresh

  // E-Ink updates every 3 cycles
  if (rtcWriteIndex % 3) {
    updateEInkDisplay(rtcBuffer[rtcWriteIndex-1], true, true); // display latest reading

  }

  if (rtcWriteIndex >= MAX_RTC_READINGS) {
    digitalWrite(POWER_GATE, 0); // Turn on LORA
    rtcWriteIndex = 0;
    Serial.println("RTC full! Dumping buffer to flash and sending to LoRa");

    writeRTCBufferToFlash();
    currentState = STATE_LORA;
  }
}


void writeRTCBufferToFlash() {
  Serial.println("Writing RTC buffer to flash (FIFO style)...");

  // Ensure the file exists and has at least 1 byte to seek into
  if (!SPIFFS.exists(FLASH_FILE)) {
    File initFile = SPIFFS.open(FLASH_FILE, FILE_WRITE);
    if (!initFile) {
      Serial.println("Failed to create flash file!");
      return;
    }
    // Pre-fill with 0s to ensure seek works later
    for (int i = 0; i < MAX_FLASH_RECORDS; i++) {
      SensorData empty = {};
      initFile.write((uint8_t*)&empty, sizeof(SensorData));
    }
    initFile.close();
    Serial.println("Pre-filled flash file with empty records.");
  }

  // Now reopen in read+write mode
  File file = SPIFFS.open(FLASH_FILE, "r+");  // Read+write
  if (!file) {
    Serial.println("Failed to open flash file for writing!");
    return;
  }

  for (int i = 0; i < MAX_RTC_READINGS; i++) {
    size_t offset = flashWriteIndex * sizeof(SensorData);
    if (!file.seek(offset, SeekSet)) {
      Serial.printf("Failed to seek to offset %u\n", (unsigned int)offset);
      file.close();
      return;
    }

    size_t bytesWritten = file.write((uint8_t*)&rtcBuffer[i], sizeof(SensorData));
    if (bytesWritten != sizeof(SensorData)) {
      Serial.printf("Failed to write full record at index %d\n", flashWriteIndex);
    } else {
      Serial.printf("  -> Written RTC[%d] to flash index %d\n", i, flashWriteIndex);
    }

    flashWriteIndex = (flashWriteIndex + 1) % MAX_FLASH_RECORDS;
  }

  file.close();
  Serial.println("RTC buffer successfully archived to flash.");
}


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
    Serial.printf("[%02d] T: %.2f, H: %.2f, Lux: %.2f, SoC: %.2f, TS: %.0f\n",
      idx++, data.temperature, data.humidity, data.lux, data.soc, data.timestamp);
}
  file.close();
}

void updateEInkDisplay(const SensorData& data, bool showSensors = true, bool showLora = true) {
  display.setRotation(1);
  display.setTextColor(GxEPD_BLACK);

  // Layout: QR + icons on left, text on right
  const int leftX = 0, leftY = 0, leftW = 100;

  // --- Left side: QR + icons ---
  display.setPartialWindow(leftX, leftY, leftW, display.height());
  display.firstPage();

  do {
    display.fillScreen(GxEPD_WHITE);

    // QR Code
    display.drawBitmap(leftX, 5, epd_bitmap_efpi_21, 100, 100, GxEPD_BLACK);

    if (showSensors) {
      // Disk icon
      display.drawBitmap(10, 110, sensor_icon_bitmap, 24, 24, GxEPD_BLACK);
    }
    if (showLora) {
       // LoRa icon (simplified as double arcs)
       display.drawBitmap(40, 110, lora_icon_bitmap, 24, 24, GxEPD_BLACK);

    }
        
    // Battery icon (horizontal)
    int batX = 70, batY = 115, batW = 24, batH = 10;
    display.drawRect(batX, batY, batW, batH, GxEPD_BLACK);
    display.fillRect(batX + batW, batY + 2, 2, batH - 4, GxEPD_BLACK); // tip
    int lvl = constrain((int)data.soc, 0, 100);
    int fillW = map(lvl, 0, 100, 0, batW - 2);
    display.fillRect(batX + 1, batY + 1, fillW, batH - 2, GxEPD_BLACK);
  } while (display.nextPage());

  // --- Right side: text info ---
  const int textX = 110, textY = 0;
  const int textW = display.width() - textX;
  display.setPartialWindow(textX, textY, textW, display.height());
  display.firstPage();
  do {
    display.fillRect(textX, textY, textW, display.height(), GxEPD_WHITE);
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(textX, 20);
    display.print("MITOSIS PROJECT:)");
    display.setFont(&FreeMonoBold12pt7b);
    float tC = data.temperature;
    float tF = tC * 9.0 / 5.0 + 32.0;
    display.setCursor(textX, 50);
    display.printf("%.1fC / %.1fF", tC, tF);

    float RH = data.humidity;
    float HI_F = -42.379 + 2.04901523*tF + 10.14333127*RH - 0.22475541*tF*RH - 0.00683783*tF*tF
                 - 0.05481717*RH*RH + 0.00122874*tF*tF*RH + 0.00085282*tF*RH*RH - 0.00000199*tF*tF*RH*RH;
    float HI_C = (HI_F - 32.0) * 5.0 / 9.0;
    display.setFont(&FreeMono9pt7b);
    display.setCursor(textX, 80);
    display.print("Feels like:");
    display.setCursor(textX, 100);
    display.printf("%.1fC/%.1fF", HI_C, HI_F);

    // --- Icon ---
    if (HI_C >= 25 && HI_C <= 38) {
      display.drawBitmap(textX + 140, 85, sun_bitmap, 30, 30, GxEPD_BLACK);
    } else if (HI_C >= 15 && HI_C < 25) {
      display.drawBitmap(textX + 140, 85, cloud_bitmap, 30, 30, GxEPD_BLACK);
    } else {
      display.drawBitmap(textX + 140, 85, snow_bitmap, 20, 20, GxEPD_BLACK);
    }

  } while (display.nextPage());

  delay(100);
  display.hibernate();
}


// Update sendToLoRa() function
void sendToLoRa(bool onlyErrors = false) {
  mySerial.begin(115200, SERIAL_8N1, RXD1, TXD1);
  delay(1500);

  if (!onlyErrors) {
    Serial.println("Sending all RTC buffer contents to LoRa...");
    String encodedStr = "";
    for (int i = 0; i < MAX_RTC_READINGS; i++) {
      String dataPacket = encodeSensorData(rtcBuffer[i]);
      dataPacket.trim();
      encodedStr += dataPacket;
      Serial.println("Encoded string: " + encodedStr);
    }
    mySerial.print(encodedStr);
    Serial.println("Encoded sensor data: " + encodedStr);
  }

  // Every other send, also send error logs
  if ((onlyErrors || ((loraSendCount % 2 == 0) && errorLogIndex > 0 && !sentErrorLogs))) {
    Serial.println("Sending encoded error logs over LoRa...");
    String errorPayload = encodeErrorLogs();
    mySerial.print("ERR_" + errorPayload);
    Serial.println("Encoded errors: ERR_" + errorPayload);
    sentErrorLogs = true;
  }

  mySerial.flush();
  mySerial.end();

  if (!onlyErrors) loraSendCount++;

  Serial.println("All RTC data sent to LoRa.");
}



String encodeSensorData(const SensorData& data) {
  auto encodeFloat = [](float val, float minVal, float maxVal, int asciiOffset = 32) -> String {
    int intPart = constrain((int)val, (int)minVal, (int)maxVal);
    int decPart = constrain((int)((val - intPart) * 10), 0, 9);
    char intChar = (char)(constrain(intPart + asciiOffset, 32, 126));
    char decChar = (char)(decPart + '0');
    return String(intChar) + decChar;
  };

  auto encodeLux = [](int lux) -> String {
    int high = constrain(lux / 100, 0, 99);
    int low  = constrain(lux % 100, 0, 99);
    char c1 = (char)(high + 32);
    char c2 = (char)(low + 32);
    return String(c1) + c2;
  };

  String encoded = "";
  encoded += encodeFloat(data.temperature, -27, 42, 60);  // 2 chars
  encoded += encodeFloat(data.humidity, 0, 100, 32);       // 2 chars
  encoded += encodeLux((int)data.lux);                    // 2 chars
  encoded += encodeFloat(data.soc, 0, 100, 32);            // 2 chars

  int tsIndex = constrain((int)data.timestamp, 0, 99);
  if (tsIndex < 10) encoded += "0";
  encoded += String(tsIndex);                             // 2 chars

  // Calculate simple XOR checksum of the payload
  char checksum = 0;
  for (int i = 0; i < encoded.length(); i++) {
    checksum ^= encoded[i];
  }
  if (checksum < 32) checksum += 32;  // Make printable
  if (checksum > 126) checksum = 126; // Clamp to printable
  encoded += (char)checksum;          // Add as final character

  return encoded;  // Total = 11 characters
}


// Add this helper function
String encodeErrorLogs() {
  String encoded = "";
  for (int i = 0; i < errorLogIndex; i++) {
    char typeCode;
    if (strcmp(rtcErrorLogs[i].type, "MPU") == 0) typeCode = 'A';
    else if (strcmp(rtcErrorLogs[i].type, "SHT4") == 0) typeCode = 'B';
    else if (strcmp(rtcErrorLogs[i].type, "VEML") == 0) typeCode = 'C';
    else if (strcmp(rtcErrorLogs[i].type, "MAX17048") == 0) typeCode = 'D';
    else if (strcmp(rtcErrorLogs[i].type, "LORA") == 0) typeCode = 'E';
    else if (strcmp(rtcErrorLogs[i].type, "WIFI") == 0) typeCode = 'F';
    else if (strcmp(rtcErrorLogs[i].type, "THEFT") == 0) typeCode = 'G';
    else typeCode = 'H';

    int msgHash = 0;
    for (size_t j = 0; j < strlen(rtcErrorLogs[i].message); j++) {
      msgHash += rtcErrorLogs[i].message[j];
    }
    msgHash %= 100;

    char chunk[5];
    snprintf(chunk, sizeof(chunk), "%c%02d%02d", typeCode, msgHash);

    if (encoded.length() + strlen(chunk) <= 256) {
      encoded += chunk;
    } else {
      break;
    }
  }
  return encoded;
}


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

bool isShaken() {
  float acceleration = readAverageAcceleration();
  // Subtract 1g to account for gravity
  float netAcceleration = fabs(acceleration - 1.0);
  // Serial.println(netAcceleration);
  return netAcceleration > shakeThreshold;
}


void logError(const char* type, const char* msg) {
  if (errorLogIndex >= MAX_ERROR_LOGS) errorLogIndex = 0;
  strncpy(rtcErrorLogs[errorLogIndex].type, type, sizeof(rtcErrorLogs[errorLogIndex].type) - 1);
  strncpy(rtcErrorLogs[errorLogIndex].message, msg, sizeof(rtcErrorLogs[errorLogIndex].message) - 1);
  errorLogIndex++;
  sentErrorLogs = false;
}

// Enter deep sleep mode
void enterDeepSleep() {
  Serial.println("Entering deep sleep...");
  esp_sleep_enable_timer_wakeup(SLEEP_TIME);  // every five minutes
  Serial.println("Going to deep sleep now...");
  // Serial.flush();     // Flush USB debug
  mySerial.flush();   // Flush LoRa output
  esp_deep_sleep_start();
}


/*----------------------------------
   char_append Function:
   Arguments:
      char* buff: pointer to character array which we will append a
      char c:
      uint16_t buff_size: size of buffer buff
   Return value:
      boolean: True if character appended, False if not appended (indicating buffer full)
*/
uint8_t char_append(char* buff, char c, uint16_t buff_size) {
  int len = strlen(buff);
  if (len > buff_size) return false;
  buff[len] = c;
  buff[len + 1] = '\0';
  return true;
}

/*----------------------------------
   read_analog_voltage Function:
   Arguments:
      int gpio_pin: GPIO pin number from which to read the analog value
      int num_average: Number of readings to average for smoothing the output
   Return value:
      float: Returns the average voltage reading from the specified GPIO pin in volts
*/
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

