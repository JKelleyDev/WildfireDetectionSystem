#include "Arduino.h"
#include "WiFi.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <ArduinoJson.h>
#include <math.h>

#define RF_FREQUENCY        915000000 // Hz
#define TX_OUTPUT_POWER     10        // dBm
#define LORA_BANDWIDTH      0         // 125 kHz
#define LORA_SPREADING_FACTOR 7       // SF7
#define LORA_CODINGRATE     1         // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE    1000      // ms
#define BUFFER_SIZE         256

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
uint16_t session_id; // 16-bit session identifier

SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Pin definitions
#define TEMP_SENSOR_PIN 4
#define MQ9_SENSOR_PIN   6
#define MQ135_SENSOR_PIN 5

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
float readTemperature();
int readMQ9();
int readMQ135();

typedef enum {
  LOWPOWER,
  STATE_RX,
  STATE_TX,
  STATE_WAIT_ACK,
  STATE_FORWARD
} States_t;

int16_t txNumber = 0;
int16_t rxNumber = 0;
States_t state = LOWPOWER;
int16_t Rssi, rxSize;
uint64_t chipid;
String chipIDString;
unsigned int counter = 0;
bool receiveflag = false;
long lastSendTime = 0;
int interval = 5000; // Increased interval for mesh stability
uint16_t last_packet_id = 0;

// Seen packet buffer to prevent loops (simple circular buffer)
#define SEEN_BUFFER_SIZE 20
struct SeenPacket {
  String source;
  uint16_t session_id; // New field
  uint16_t packet_id;
};
SeenPacket seenPackets[SEEN_BUFFER_SIZE];
int seenIndex = 0;

const char* ACK_MSG = "ACK";
unsigned long ackWaitStart = 0;
const unsigned long ackTimeout = 1500;
int ackRetries = 0;
const int maxRetries = 3;
const int TTL_MAX = 10; // Max hops to prevent infinite loops

void OnTxDone(void) {
  Serial.println("TX done.");
  factory_display.clear();
  factory_display.drawString(0, 20, "Packet Sent");
  factory_display.display();
  if (state == STATE_TX) {
    ackWaitStart = millis();
    state = STATE_WAIT_ACK;
  }
  Radio.Rx(RX_TIMEOUT_VALUE);
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("TX Timeout, retrying...");
  factory_display.clear();
  factory_display.drawString(0, 20, "TX Timeout");
  factory_display.display();
  state = STATE_TX;
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  rxNumber++;
  Rssi = rssi;
  rxSize = size;

  if (size > BUFFER_SIZE - 1) {
    Serial.println("Received packet too large, discarding...");
    Radio.Sleep();
    state = LOWPOWER;
    return;
  }

  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Radio.Sleep();

  Serial.printf("Received: \"%s\" RSSI: %d, SNR: %d, len: %d\n", rxpacket, rssi, snr, size);

  if (snr < -10 || rssi < -120) {
    Serial.println("Packet signal too weak, discarding...");
    state = LOWPOWER;
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rxpacket);
  if (error) {
    Serial.printf("deserializeJson() failed: %s\n", error.c_str());
    state = LOWPOWER;
    return;
  }

  if (!doc.containsKey("source") || !doc.containsKey("dest") || !doc.containsKey("packet_id") || !doc.containsKey("ttl") || !doc.containsKey("session_id")) {
    Serial.println("Missing required fields, ignoring...");
    state = LOWPOWER;
    return;
  }

  String source = doc["source"].as<String>();
  String dest = doc["dest"].as<String>();
  uint16_t packet_id = doc["packet_id"];
  uint16_t received_session_id = doc["session_id"]; // Extract session_id
  int ttl = doc["ttl"];
  const char* msg = doc["msg"];

  // Check if packet was already seen (using session_id)
  for (int i = 0; i < SEEN_BUFFER_SIZE; i++) {
    if (seenPackets[i].source == source && 
        seenPackets[i].session_id == received_session_id && 
        seenPackets[i].packet_id == packet_id) {
      Serial.println("Duplicate packet, ignoring...");
      state = LOWPOWER;
      return;
    }
  }
  // Add to seen buffer
  seenPackets[seenIndex] = {source, received_session_id, packet_id};
  seenIndex = (seenIndex + 1) % SEEN_BUFFER_SIZE;

  if (dest == chipIDString) { // Packet is for this node
    if (strcmp(msg, ACK_MSG) == 0) {
      if (packet_id == last_packet_id) {
        Serial.println("Valid ACK received for packet #" + String(packet_id));
        factory_display.clear();
        factory_display.drawString(20, 20, "ACK received!");
        factory_display.display();
        ackRetries = 0;
        state = LOWPOWER;
      } else {
        Serial.printf("ACK packet_id mismatch: expected %d, got %d\n", last_packet_id, packet_id);
      }
    } else {
      Serial.println("Data packet received: " + String(msg));
      factory_display.clear();
      factory_display.drawString(0, 20, "Data: " + String(msg));
      factory_display.display();
    }
  } else if (ttl > 0) { // Forward the packet
    Serial.println("Forwarding packet from " + source + " to " + dest);
    doc["ttl"] = ttl - 1; // Decrement TTL
    serializeJson(doc, txpacket, BUFFER_SIZE);
    delay(random(50, 200)); // Random backoff to avoid collisions
    state = STATE_FORWARD;
  } else {
    Serial.println("TTL expired, dropping packet.");
    state = LOWPOWER;
  }
  doc.clear();
}

void lora_init(void) {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init(&RadioEvents);
  Radio.SetChannel(RF_FREQUENCY);
  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, 3000);
  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
  state = LOWPOWER;
}

void VextON(void) { pinMode(Vext, OUTPUT); digitalWrite(Vext, LOW); }
void VextOFF(void) { pinMode(Vext, OUTPUT); digitalWrite(Vext, HIGH); }

float readTemperature() {
  int adcValue = analogRead(TEMP_SENSOR_PIN);
  Serial.print("ADC Value: ");
  Serial.println(adcValue);
  float milliVolt = (adcValue * 3300.0) / 4095.0;
  Serial.print("Millivolts: ");
  Serial.println(milliVolt);
  float temperature = (milliVolt - 500.0) / 10.0;
  Serial.print("Temperature: ");
  Serial.println(temperature);
  return round(temperature * 100) / 100; // Rounds to two decimal places
}

int readMQ9() { return analogRead(MQ9_SENSOR_PIN); }
int readMQ135() { return analogRead(MQ135_SENSOR_PIN); }

void setup() {
  Serial.begin(115200);
  VextON();
  delay(100);
  factory_display.init();
  factory_display.clear();
  factory_display.display();

  delay(2000);

  chipid = ESP.getEfuseMac();
  chipIDString = String((uint16_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
  Serial.println("ESP32 ChipID: " + chipIDString);

  // Generate a random session_id on startup
  session_id = random(0, 65535); // 16-bit random value
  Serial.println("Session ID: " + String(session_id));

  lora_init();

  factory_display.drawString(0, 10, "Chip ID: " + chipIDString);
  factory_display.drawString(0, 20, "Mesh Node Ready");
  factory_display.display();
  delay(5000);
  factory_display.clear();

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 5000;

void loop() {
  if (millis() - lastSensorRead >= sensorInterval && state == LOWPOWER) {
    lastSensorRead = millis();
    float temperature = readTemperature();
    int mq9 = readMQ9();
    int mq135 = readMQ135();

    factory_display.clear();
    factory_display.drawString(0, 10, "Temp: " + String(temperature));
    factory_display.drawString(0, 20, "MQ-9: " + String(mq9));
    factory_display.drawString(0, 30, "MQ-135: " + String(mq135));
    factory_display.display();

    StaticJsonDocument<256> doc;
    static uint16_t packet_id = 0;
    last_packet_id = packet_id;
    String receiverID = "24253f43ca48"; // Master Receiver Node
    doc["source"] = chipIDString;
    doc["dest"] = receiverID;
    doc["msg"] = "SENSOR_DATA";
    doc["temperature"] = temperature;
    doc["mq9"] = mq9;
    doc["mq135"] = mq135;
    doc["session_id"] = session_id; // Add session_id to packet
    doc["packet_id"] = packet_id++;
    doc["ttl"] = TTL_MAX;
    serializeJson(doc, txpacket, BUFFER_SIZE);

    Serial.printf("Sending packet #%d: %s\n", last_packet_id, txpacket);
    state = STATE_TX;
  }

  switch (state) {
    case STATE_TX:
    case STATE_FORWARD:
      txNumber++;
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      state = (state == STATE_TX) ? LOWPOWER : LOWPOWER; // Reset state after forwarding
      break;

    case STATE_WAIT_ACK:
      if (millis() - ackWaitStart >= ackTimeout) {
        ackRetries++;
        if (ackRetries < maxRetries) {
          Serial.println("No ACK, retrying TX...");
          factory_display.clear();
          factory_display.drawString(0, 20, "Retrying...");
          factory_display.display();
          state = STATE_TX;
        } else {
          Serial.println("Max retries reached, dropping packet.");
          factory_display.clear();
          factory_display.drawString(0, 20, "TX Failed");
          factory_display.display();
          ackRetries = 0;
          state = LOWPOWER;
        }
      }
      break;

    case LOWPOWER:
      Radio.IrqProcess();
      break;

    default:
      break;
  }
}