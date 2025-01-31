#include <SPI.h>
#include <LoRa.h>
#include <Crypto.h>
#include <AES.h>
#include <ArduinoJson.h>

// LoRa pins (adjust to match your hardware)
#define LORA_SS   18
#define LORA_RST  14
#define LORA_DIO0 26

// LoRa frequency
#define LORA_FREQUENCY 915E6  // US Standard 

// Same AES-128 Key as the Sensor Node
byte aesKey[16] = {
  0x1A, 0x2B, 0x3C, 0x4D,
  0x5E, 0x6F, 0x70, 0x81,
  0x92, 0xA3, 0xB4, 0xC5,
  0xD6, 0xE7, 0xF8, 0x09
};

// Function to decrypt a 32-char HEX string back to the original 16-byte plaintext
String decryptMessage(const String& encryptedHex) {
  // 1) Convert from 32-char hex string to 16-byte ciphertext
  byte ciphertext[16];
  for (int i = 0; i < 16; i++) {
    // Each byte is 2 hex characters
    String byteString = encryptedHex.substring(2 * i, 2 * i + 2);
    ciphertext[i] = (byte) strtol(byteString.c_str(), NULL, 16);
  }

  // 2) Decrypt using AES-128 (single block)
  AES128 aes;
  aes.setKey(aesKey, 16);

  byte plaintext[16];
  aes.decryptBlock(plaintext, ciphertext);

  // 3) Convert plaintext bytes to a String
  //    Stop if we encounter a null terminator (0x00)
  String decrypted;
  for (int i = 0; i < 16; i++) {
    if (plaintext[i] == 0) break; 
    decrypted += (char)plaintext[i];
  }

  return decrypted;
}

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Initializing LoRa Receiver...");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("LoRa init failed. Check your connections.");
    while(true);
  }

  Serial.println("LoRa Receiver Ready.");
}

void loop() {
  // Check if a packet is available
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    // Read the entire encrypted message as a hex string
    String encryptedHex = LoRa.readString();
    Serial.println("========================");
    Serial.println("Received Encrypted HEX:");
    Serial.println(encryptedHex);

    // Decrypt
    String decryptedJSON = decryptMessage(encryptedHex);
    Serial.println("Decrypted String:");
    Serial.println(decryptedJSON);

    // Parse JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, decryptedJSON);

    if (error) {
      Serial.println("JSON parse error!");
      Serial.println("========================");
      return;
    }

    // Successfully parsed JSON
    // Access the fields (depending on what the node sends)
    String node         = doc["node"];
    String msgType      = doc["type"];    // "FLASH" or "UPDATE"
    float temperature   = doc["temperature"];
    int   mq9_value     = doc["mq9"];
    int   mq135_value   = doc["mq135"];

    // Print them out
    Serial.println("Parsed JSON Data:");
    Serial.print("Node: ");
    Serial.println(node);

    Serial.print("Type: ");
    Serial.println(msgType);

    Serial.print("Temperature: ");
    Serial.println(temperature);

    Serial.print("MQ9: ");
    Serial.println(mq9_value);

    Serial.print("MQ135: ");
    Serial.println(mq135_value);
    Serial.println("========================");
  }
}
