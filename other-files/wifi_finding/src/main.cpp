#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>  // Library for deep sleep functions

// Structure for allowed networks
struct WifiNetwork {
  const char* ssid;
  const char* password;
};

// List your allowed networks here
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

// Function to scan for WiFi networks, select the best candidate, and connect
void WifiSetup() {
  Serial.println("Scanning for WiFi networks...");
  int n = WiFi.scanNetworks();
  
  if (n == 0) {
    Serial.println("No networks found");
    return;
  } else {
    Serial.printf("%d networks found\n", n);
    // Reset bestRSSI for this scan
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
    } else {
      Serial.println("Failed to connect to WiFi.");
    }
  } else {
    Serial.println("No allowed networks found in scan results.");
  }
}

void setup() {
  Serial.begin(9600);
  delay(100);  // Allow time for the serial monitor to start
  WifiSetup();

  // Wait a bit to allow you to see the output before sleeping
  Serial.println("Entering deep sleep for 15 seconds...");
  delay(2000);

  // Configure deep sleep for 15 seconds (15,000,000 microseconds)
  esp_sleep_enable_timer_wakeup(15 * 1000000LL);
  esp_deep_sleep_start();
}

void loop() {
  // Not used in deep sleep scenario.
  Serial.println("Running...");
  delay(1000);
}
