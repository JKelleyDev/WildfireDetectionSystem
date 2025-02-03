#include <SPI.h>
#include <LoRa.h>
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <AESLib.h>
#include "config.h"  // Include pinnout and freq definitions

String NODE_ID = "NODE_1"; // Unique sensor ID

AESLib aes;  // Declare globally to avoid scope issues

// AES-128 Encryption Key (16 bytes)
byte aesKey[16] = {
  0x1A, 0x2B, 0x3C, 0x4D,
  0x5E, 0x6F, 0x70, 0x81,
  0x92, 0xA3, 0xB4, 0xC5,
  0xD6, 0xE7, 0xF8, 0x09
};

String encryptMessage(String message) {
    int messageLength = message.length();
    int paddedLength = ((messageLength / 16) + 1) * 16;

    byte plaintext[paddedLength];
    byte ciphertext[paddedLength];

    memset(plaintext, 0, paddedLength);
    message.getBytes(plaintext, paddedLength);

    // Generate a random IV
    byte aesIV[16];
    for (int i = 0; i < 16; i++) {
        aesIV[i] = random(0, 256);
    }

    aes.encrypt(plaintext, paddedLength, ciphertext, aesKey, 128, aesIV);

    // Convert IV + Ciphertext to HEX
    String encryptedMsg = "";
    for (int i = 0; i < 16; i++) {  // Append IV first
        if (aesIV[i] < 0x10) encryptedMsg += "0";
        encryptedMsg += String(aesIV[i], HEX);
    }
    for (int i = 0; i < paddedLength; i++) {  // Append Ciphertext
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

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);


void setup() {
    Serial.begin(115200);
    while (!Serial);

    delay(2000); // Wait 2 seconds 

    Serial.println("Initializing Secure Sensor Node...");

    // Initialize SPI with correct pins
    SPI.begin(SCK, MISO, MOSI, SS);

    delay(2000); 

    // Initialize LoRa with explicit SPI object
    LoRa.setPins(SS, RST, DIO0);

    delay(2000); 

    if (!LoRa.begin(LORA_FREQUENCY)) {
        Serial.println("LoRa init failed!");
        while (true); // Halt if initialization fails
    }

    Serial.println("LoRa initialized successfully.");

    // Initialize Temperature Sensor
    sensors.begin();

    delay(2000); 

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
