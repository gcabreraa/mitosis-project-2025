#include <Arduino.h>
#include <WiFi.h> //Connect to WiFi Network
#include <string.h>  //used for some string handling and processing.
#include "Adafruit_SHT4x.h"
#include <SparkFun_VEML7700_Arduino_Library.h> // Click here to get the library: http://librarymanager/All#SparkFun_VEML7700
#include "Adafruit_MAX1704X.h"
#include "ICM42670P.h"
#include "esp_adc_cal.h"

#define I2C_SDA1 4
#define I2C_SCL1 5


Adafruit_SHT4x sht4 = Adafruit_SHT4x();
VEML7700 veml;
Adafruit_MAX17048 maxlipo;
ICM42670 IMU(Wire,0);

volatile bool wake_up = false;

void irq_handler(void) {
  wake_up = true;
}

void do_http_GET(char* host, char* request, char* response, uint16_t response_size, uint16_t response_timeout, uint8_t serial);
uint8_t char_append(char* buff, char c, uint16_t buff_size);

float read_analog_voltage(int gpio_pin, int num_average) {
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

float read_wind_voltage(int gpio_pin, int num_average) {
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

const int RESPONSE_TIMEOUT = 30000; //ms to wait for response from host
const int GETTING_PERIOD = 5000000; //periodicity of getting a number fact.
const uint16_t IN_BUFFER_SIZE = 1000; //size of buffer to hold HTTP request
const uint16_t OUT_BUFFER_SIZE = 1000; //size of buffer to hold HTTP response
char request_buffer[IN_BUFFER_SIZE]; //char array buffer to hold HTTP request
char response_buffer[OUT_BUFFER_SIZE]; //char array buffer to hold HTTP response

uint32_t last_time=0; //used for timing

//wifi network credentials for 6.08 Lab (this is a decent 2.4 GHz router that we have set up...try to only use for our ESP32s)
//char network[] = "MIT";
//char password[] = "dYx3xq3]vt";

char network[] = "EECS_Labs";
char password[] = "";

void setup() {

  Serial.begin(115200);
  while (!Serial) delay(10);
  //while(Serial == false){};  // Wait for the serial connection to startup
  //Serial.println("SHT40 Example 1 - Basic Readings");    // Title
  
  pinMode(10,OUTPUT);
  digitalWrite(10,1);

  Wire.begin(I2C_SDA1, I2C_SCL1);
  //Serial.println(F("\nAdafruit MAX17048 simple demo"));

  while (!maxlipo.begin()) {
    Serial.println(F("Couldnt find Adafruit MAX17048"));
    delay(2000);
  }/** */
  while (!sht4.begin()) {
    Serial.println(F("Couldnt find SHT40"));
    delay(2000);
  }
  while (!veml.begin()) {
    Serial.println(F("Couldnt find VEML"));
    delay(2000);
  }
  /*
  while (!IMU.begin()) {
    Serial.println(F("Couldnt find VEML"));
    delay(2000);
  }
  IMU.startWakeOnMotion(2,irq_handler);
  Serial.println("Going to sleep");
  */
/** */
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


  
  
}

void test_message(){
  WiFi.begin(network, password);
  //if using channel/mac specification for crowded bands use the following:
  //WiFi.begin(network, password, channel, bssid);
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count < 6) { //can change this to more attempts
    delay(500);
    Serial.print(".");
    count++;
  }
  //delay(2000);  //acceptable since it is in the setup function.
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    delay(500);
    //formulate GET request...first line:
    sprintf(request_buffer, "GET http://numbersapi.com/%d/trivia HTTP/1.1\r\n", random(200));
    strcat(request_buffer, "Host: numbersapi.com\r\n"); //add more to the end
    strcat(request_buffer, "\r\n"); //add blank line!
    //submit to function that performs GET.  It will return output using response_buffer char array
    do_http_GET("numbersapi.com", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Will try next time.");
    Serial.println(WiFi.status());
    //ESP.restart(); // restart the ESP (proper way)
  }
}

/*-----------------------------------
  Generate a request to the numbersapi server for a random number
  Display the response both on the TFT and in the Serial Monitor
*/
/*
void loop() {
  WiFi.begin(network, password);
  //if using channel/mac specification for crowded bands use the following:
  //WiFi.begin(network, password, channel, bssid);
  uint8_t count = 0; //count used for Wifi check times
  Serial.print("Attempting to connect to ");
  Serial.println(network);
  while (WiFi.status() != WL_CONNECTED && count < 6) { //can change this to more attempts
    delay(500);
    Serial.print(".");
    count++;
  }
  delay(2000);  //acceptable since it is in the setup function.
  if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
    Serial.println("CONNECTED!");
    digitalWrite(8,1);
    delay(1000);
    digitalWrite(8,0);
    delay(500);
    
    randomSeed(analogRead(A0)); //"seed" random number generator
  
    sensors_event_t humidity, temp;
    sensors_event_t humidity2, temp2;

    uint32_t timestamp = millis();
    sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
    float lux = veml.getLux();

    sprintf(request_buffer, "POST http://efpi-10.mit.edu/tunnel_raulb456/loggertemp?temp=%f&rh=%f&lux=%f&bat=%f HTTP/1.1\r\n", 
          temp.temperature, humidity.relative_humidity, lux, read_analog_voltage(4, 10) * 2);
    strcat(request_buffer, "Host: efpi-10.mit.edu\r\n"); //add more to the end
    strcat(request_buffer, "\r\n"); //add blank line!
      //submit to function that performs GET.  It will return output using response_buffer char array
    do_http_GET("efpi-10.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
    Serial.println(response_buffer); //print to serial monitor

    esp_sleep_enable_timer_wakeup(GETTING_PERIOD); // how long to sleep in us
    esp_deep_sleep_start();

  } else { //if we failed to connect just Try again.
    Serial.println("Failed to Connect :/  Going to restart");
    for (int i=0; i<3; i++){
      digitalWrite(8,1);
      delay(500);
      digitalWrite(8,0);
      delay(500);
    }
    Serial.println(WiFi.status());
  }
  
}
*/

void loop() {
  sensors_event_t humidity, temp;
  
  uint32_t timestamp = millis();
  sht4.getEvent(&humidity, &temp);// populate temp and humidity objects with fresh data
  float lux = veml.getLux();
  float cellVoltage = maxlipo.cellVoltage();


  if (isnan(cellVoltage)) {
    Serial.println("Failed to read cell voltage, check battery is connected!");
    delay(2000);
    return;
  }

  timestamp = millis() - timestamp;

  test_message();

  Serial.print("Temperature SHT40: "); Serial.print(temp.temperature); Serial.println(" degrees C");
  Serial.print("Humidity SHT40: "); Serial.print(humidity.relative_humidity); Serial.println("% rH");
  
  Serial.print("Lux: "); Serial.print(lux); Serial.println(" lx");

  Serial.print(F("Batt Voltage: ")); Serial.print(cellVoltage, 3); Serial.println(" V");
  Serial.print(F("Batt Percent: ")); Serial.print(maxlipo.cellPercent(), 1); Serial.println(" %");
  Serial.println();

  if(wake_up){
    Serial.println("MOVED");
    wake_up = false;
    Serial.println("Going to sleep");
  } else {
    Serial.println("NOT MOVED");
  }


  Serial.print("Read duration (ms): ");
  Serial.println(timestamp);

  digitalWrite(10,0);
  Serial.println("LORA ON");
  delay(5000);
  digitalWrite(10,1);
  Serial.println("LORA OFF");
  delay(5000);
  
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
   do_http_GET Function:
   Arguments:
      char* host: null-terminated char-array containing host to connect to
      char* request: null-terminated char-arry containing properly formatted HTTP GET request
      char* response: char-array used as output for function to contain response
      uint16_t response_size: size of response buffer (in bytes)
      uint16_t response_timeout: duration we'll wait (in ms) for a response from server
      uint8_t serial: used for printing debug information to terminal (true prints, false doesn't)
   Return value:
      void (none)
*/
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
