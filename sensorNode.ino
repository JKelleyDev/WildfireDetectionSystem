#include <AES.h>
#include <BlockCipher.h>
#include <Crypto.h>
#include <ProgMemUtil.h>
#include <SPI.h>
#include <LoRa.h> // Used to transmit over LoRa 
#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h> // Used to create the JSON doc 
#include "config.h"  // Include pinnout and freq definitions
#include <vector>
   
#include "driver/spi_master.h"
String NODE_ID = "NODE_1"; // Unique sensor ID
using namespace std; 
/* 
  Purpose: Encrypts a message stored in a byte sized vector, uses AES 128 protocal and IBM PKCS padding 
    @Link: https://www.ibm.com/docs/en/zos/2.4.0?topic=rules-pkcs-padding-method 
    
    @Param: vector<unit8_t> input - Raw string to be encrypted, stored in a byte sized vector 
    @Return: vector<unit_8> encrypted message in a byte sized vector 

*/
vector<uint8_t> encryptMessage(vector<uint8_t> input){
  
    AES128 aes; // Creates an AES object 

    // Unique key for encryption, must be 16 bytes 
    uint8_t myKey[] = { 0x2b, 0x7e, 0x15, 0x16, 0x28, 0xae, 0xd2, 0xa6,
                          0xab, 0xf7, 0x4d, 0x9f, 0x52, 0x55, 0x24, 0x6a};


    // Debug if key sets correctly or not (Checks to see if its 16 bytes) 
    if (aes.setKey(myKey, sizeof(myKey))) {
        Serial.println("Key set successfully!");
    } else {
        Serial.println ("Failed to set key!");
        
    }

    static size_t blockSize = 16;  // AES block size, ALWAYS 16 DO NOT CHANGE

    /* Apply PKCS7 padding
        for any block that has less that 16 bytes of data to encrypt, it will pad it IAW IBM PKCS7 
    */ 
    //aes.padPKCS7(input, blockSize);

    size_t padding = blockSize - (input.size() % blockSize);
    for (size_t i = 0; i < padding; i++) {
        input.push_back(static_cast<uint8_t>(padding));
    }
    
    size_t numBlocks = input.size() / blockSize; // Determines how many blocks need encrypted 
    vector<uint8_t> encrypted(input.size(), 0); // Vector to store encrypted bytes 

    // Encrypt each block, for the number of blocks to be done 
    for (size_t i = 0; i < numBlocks; i++) {
        aes.encryptBlock(&encrypted[i * blockSize], &input[i * blockSize]);
    }

    // Print encrypted output as hex (Debuggin)
    Serial.println("Encrypted Data (HEX):");
    for (uint8_t b : encrypted) {
        if (b < 16) Serial.print("0");
        Serial.print(b, HEX);
        Serial.print(" ");
    }
    Serial.println();
    return encrypted; 
}

/*
  Purpose: Sends a message between two nodes using the LoRa protical 

    Param: String type - The type of message "FLASH" or "UPDATE" 
    Param: float temperature - Temperature in Celsius, formated xx.xx 
    Param: int mq9_value - Reading of the CO2 levels 
    Param: int mq135_value - Reading of the air quality levels 
*/
void sendLoRaMessage(String type, float temperature, int mq9_value, int mq135_value) {
  // Create JSON
  StaticJsonDocument<128> jsonDoc;
  jsonDoc["node"]         = NODE_ID;
  jsonDoc["type"]         = type;          
  jsonDoc["temperature"]  = temperature;
  jsonDoc["mq9"]          = mq9_value;
  jsonDoc["mq135"]        = mq135_value;

  String jsonString;
  serializeJson(jsonDoc, jsonString); // Converts json document into a full string 

  // Convert JSON string to byte array
  vector<uint8_t> input(jsonString.begin(), jsonString.end());

  // Encrypt JSON string array 
  vector<uint8_t> encryptedMessage = encryptMessage(input);

  // Send encrypted data over LoRa
  LoRa.beginPacket(); // Begins the packet 
  LoRa.write(encryptedMessage.data(), encryptedMessage.size()); // Writes the data into packets 
  LoRa.endPacket(); // Finishes writing and sends off the data

  // Debug output
  Serial.println("---------------------------------------------------");
  Serial.print("Message Type: ");
  Serial.println(type);
  Serial.println("JSON (unencrypted):");
  Serial.println(jsonString);
  Serial.println("Encrypted Data (HEX):");
  for (uint8_t b : encryptedMessage) {
      if (b < 16) Serial.print("0");
      Serial.print(b, HEX);
      Serial.print(" ");
  }
  Serial.println();
  Serial.println("---------------------------------------------------");
}

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

void setup() {
  // Start Serial for debugging
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for serial port connection
  }

  // Slow startup delay
  delay(2000);
  Serial.println("Initializing Secure Sensor Node...");
  SPI.begin(SCK, MISO, MOSI, SS);
  delay(2000); // Allow time for SPI bus to settle

  // Initialize LoRa with explicit SPI object
  LoRa.setPins(SS, RST, DIO1);
  delay(2000); // Give the LoRa module time to power up
  LoRa.begin(LORA_FREQUENCY);
  

  Serial.println("LoRa initialized successfully.");

  // Uncomment if using temperature sensor
  // sensors.begin();

  Serial.println("Sensor Node Ready.");
}

void loop() {
  // unsigned long currentMillis = millis();

  // // Check if it's time to read the sensors (every 2 minutes)
  // if (currentMillis - lastSensorRead >= readInterval) {
  //   lastSensorRead = currentMillis;

  //   // Read sensor values
  //   sensors.requestTemperatures();
  //   float temperature = sensors.getTempCByIndex(0);
  //   int mq9_value     = analogRead(MQ9_PIN);
  //   int mq135_value   = analogRead(MQ135_PIN);

  //   // Check thresholds
  //   bool thresholdExceeded = false;
  //   if (temperature >= TEMP_THRESHOLD ||
  //       mq9_value    >= MQ9_THRESHOLD  ||
  //       mq135_value  >= MQ135_THRESHOLD) 
  //   {
  //     thresholdExceeded = true;
  //     // Send FLASH message immediately
  //     sendLoRaMessage("FLASH", temperature, mq9_value, mq135_value);
  //   }

  //   // If threshold not exceeded, we still store these new sensor values 
  //   // but do NOT send right away. We'll send them in the "regular update"
  //   // at 10-minute intervals. So let's keep them around in some variables:
  //   static float lastTemperature = 0.0;
  //   static int lastMQ9 = 0;
  //   static int lastMQ135 = 0;

  //   lastTemperature = temperature;
  //   lastMQ9         = mq9_value;
  //   lastMQ135       = mq135_value;

  //   // Now check if it's time for the regular 10-minute update
  //   if (!thresholdExceeded) {
  //     // Only send a regular message if the threshold was NOT exceeded
  //     if (currentMillis - lastSend >= sendInterval) {
  //       lastSend = currentMillis;
  //       // Send "UPDATE" message with the latest data
  //       sendLoRaMessage("UPDATE", lastTemperature, lastMQ9, lastMQ135);
  //     }
  //   }
  // }

    sendLoRaMessage("Update", 50.0, 500, 500); 
    
}
