#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <Crypto.h>
#include <AES.h>

// Pin Definitions
#define SS            18
#define RST           14
#define DIO0          26
#define ONE_WIRE_BUS  4
#define MQ9_PIN       34
#define MQ135_PIN     35
#define LORA_FREQUENCY 915E6 // United States Standard 

// Timings
unsigned long lastSensorRead = 0;
unsigned long lastSend       = 0;
const unsigned long readInterval = 2UL * 60UL * 1000UL;  // 2 minutes
const unsigned long sendInterval = 10UL * 60UL * 1000UL; // 10 minutes

// OneWire + DallasTemperature
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

String NODE_ID = "NODE_1"; // Unique sensor ID

// Thresholds for Wildfire Detection
const float TEMP_THRESHOLD   = 50.0; // Example: 50°C as a dangerous temperature
const float MQ9_THRESHOLD    = 200.0;
const float MQ135_THRESHOLD  = 300.0;

// AES-128 Encryption Key (16 bytes)
byte aesKey[16] = {
  0x1A, 0x2B, 0x3C, 0x4D,
  0x5E, 0x6F, 0x70, 0x81,
  0x92, 0xA3, 0xB4, 0xC5,
  0xD6, 0xE7, 0xF8, 0x09
};

// Function to encrypt message using AES-128
String encryptMessage(String message) {
  AES128 aes;
  aes.setKey(aesKey, 16);

  byte plaintext[16]   = {0};
  byte ciphertext[16]  = {0};

  // Convert String to byte array (up to 15 chars + null terminator)
  message.getBytes(plaintext, 16);

  // Encrypt in-place
  aes.encryptBlock(ciphertext, plaintext);

  // Convert encrypted bytes back to hex string
  String encryptedMsg = "";
  for (int i = 0; i < 16; i++) {
    // Each byte as two hex digits
    if (ciphertext[i] < 0x10) encryptedMsg += "0"; 
    encryptedMsg += String(ciphertext[i], HEX);
  }

  return encryptedMsg;
}

// Helper to send data over LoRa
void sendLoRaMessage(String type, float temperature, int mq9_value, int mq135_value) {
  // Create JSON
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["node"]         = NODE_ID;
  jsonDoc["type"]         = type;            // "FLASH" or "UPDATE"
  jsonDoc["temperature"]  = temperature;
  jsonDoc["mq9"]          = mq9_value;
  jsonDoc["mq135"]        = mq135_value;

  String jsonString;
  serializeJson(jsonDoc, jsonString);

  // Encrypt JSON
  String encryptedMessage = encryptMessage(jsonString);

  // Send encrypted data over LoRa
  LoRa.beginPacket();
  LoRa.print(encryptedMessage);
  LoRa.endPacket();

  // Debug output
  Serial.println("---------------------------------------------------");
  Serial.print("Message Type: ");
  Serial.println(type);
  Serial.println("JSON (unencrypted):");
  Serial.println(jsonString);
  Serial.println("Encrypted Data (HEX):");
  Serial.println(encryptedMessage);
  Serial.println("---------------------------------------------------");
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Initializing Secure Sensor Node...");

  // Initialize LoRa
  LoRa.setPins(SS, RST, DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed!");
    while (true);
  }

  // Initialize Temperature Sensor
  sensors.begin();

  Serial.println("Sensor Node Ready.");
}

void loop() {
  unsigned long currentMillis = millis();

  // Check if it's time to read the sensors (every 2 minutes)
  if (currentMillis - lastSensorRead >= readInterval) {
    lastSensorRead = currentMillis;

    // Read sensor values
    sensors.requestTemperatures();
    float temperature = sensors.getTempCByIndex(0);
    int mq9_value     = analogRead(MQ9_PIN);
    int mq135_value   = analogRead(MQ135_PIN);

    // Check thresholds
    bool thresholdExceeded = false;
    if (temperature >= TEMP_THRESHOLD ||
        mq9_value    >= MQ9_THRESHOLD  ||
        mq135_value  >= MQ135_THRESHOLD) 
    {
      thresholdExceeded = true;
      // Send FLASH message immediately
      sendLoRaMessage("FLASH", temperature, mq9_value, mq135_value);
    }

    // If threshold not exceeded, we still store these new sensor values 
    // but do NOT send right away. We'll send them in the "regular update"
    // at 10-minute intervals. So let's keep them around in some variables:
    static float lastTemperature = 0.0;
    static int lastMQ9 = 0;
    static int lastMQ135 = 0;

    lastTemperature = temperature;
    lastMQ9         = mq9_value;
    lastMQ135       = mq135_value;

    // Now check if it's time for the regular 10-minute update
    if (!thresholdExceeded) {
      // Only send a regular message if the threshold was NOT exceeded
      if (currentMillis - lastSend >= sendInterval) {
        lastSend = currentMillis;
        // Send "UPDATE" message with the latest data
        sendLoRaMessage("UPDATE", lastTemperature, lastMQ9, lastMQ135);
      }
    }
  }
}
