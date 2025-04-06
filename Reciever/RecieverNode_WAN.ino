#include "Arduino.h"
#include "WiFi.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

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

// WiFi credentials
const char* ssid = "XXXXXXXX";       // Replace with your WiFi SSID
const char* password = "XXXXXXXXX"; // Replace with your WiFi password

// Server details (adjusted for SSH tunneling)
const char* serverName = "http://143.110.193.195/update"; // Localhost port forwarded via SSH tunnel


char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];
uint16_t session_id;

SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

typedef enum {
  LOWPOWER,
  STATE_RX,
  STATE_TX,
  STATE_FORWARD
} States_t;

int16_t txNumber = 0;
int16_t rxNumber = 0;
States_t state = STATE_RX;
int16_t Rssi, rxSize;
uint64_t chipid;
String chipIDString;
unsigned int counter = 0;
bool receiveflag = false;
long lastSendTime = 0;
int interval = 1000;

// Seen packet buffer to prevent loops
#define SEEN_BUFFER_SIZE 20
struct SeenPacket {
  String source;
  uint16_t session_id;
  uint16_t packet_id;
};
SeenPacket seenPackets[SEEN_BUFFER_SIZE];
int seenIndex = 0;

const char* ACK_MSG = "ACK";
const int TTL_MAX = 10; // Max hops

void OnTxDone(void) {
  Serial.println("TX done.");
  factory_display.clear();
  factory_display.drawString(0, 20, "Packet Sent");
  factory_display.display();
  state = STATE_RX;
  Radio.Rx(0);
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("TX Timeout, retrying...");
  factory_display.clear();
  factory_display.drawString(0, 20, "TX Timeout");
  factory_display.display();
  state = STATE_TX;
}

void sendToServer(String sensorId, float temperature, int mq9, int mq135) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverName);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<200> doc;
    doc["sensor_id"] = sensorId;
    doc["temperature"] = temperature;
    doc["mq9"] = mq9;
    doc["mq135"] = mq135;

    String jsonData;
    serializeJson(doc, jsonData);

    int httpResponseCode = http.POST(jsonData);
    if (httpResponseCode == 200) {
      Serial.println("Data successfully sent to server");
    } else {
      Serial.printf("HTTP POST failed, code: %d\n", httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr) {
  rxNumber++;
  Rssi = rssi;
  rxSize = size;

  if (size > BUFFER_SIZE - 1) {
    Serial.println("Received packet too large, discarding...");
    Radio.Sleep();
    state = STATE_RX;
    Radio.Rx(0);
    return;
  }

  memcpy(rxpacket, payload, size);
  rxpacket[size] = '\0';
  Radio.Sleep();

  Serial.printf("Received: \"%s\" RSSI: %d, SNR: %d, len: %d\n", rxpacket, rssi, snr, size);

  if (snr < -10 || rssi < -120) {
    Serial.println("Packet signal too weak, discarding...");
    state = STATE_RX;
    Radio.Rx(0);
    return;
  }

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rxpacket);
  if (error) {
    Serial.printf("deserializeJson() failed: %s\n", error.c_str());
    state = STATE_RX;
    Radio.Rx(0);
    return;
  }

  if (!doc.containsKey("source") || !doc.containsKey("dest") || !doc.containsKey("packet_id") || !doc.containsKey("ttl") || !doc.containsKey("session_id")) {
    Serial.println("Missing required fields, ignoring...");
    state = STATE_RX;
    Radio.Rx(0);
    return;
  }

  String source = doc["source"].as<String>();
  String dest = doc["dest"].as<String>();
  uint16_t packet_id = doc["packet_id"];
  uint16_t received_session_id = doc["session_id"];
  int ttl = doc["ttl"];
  const char* msg = doc["msg"];

  for (int i = 0; i < SEEN_BUFFER_SIZE; i++) {
    if (seenPackets[i].source == source && 
        seenPackets[i].session_id == received_session_id && 
        seenPackets[i].packet_id == packet_id) {
      Serial.println("Duplicate packet, ignoring...");
      state = STATE_RX;
      Radio.Rx(0);
      return;
    }
  }
  seenPackets[seenIndex] = {source, received_session_id, packet_id};
  seenIndex = (seenIndex + 1) % SEEN_BUFFER_SIZE;

  if (dest == chipIDString) {
    float temperature = doc["temperature"] | -1.0;
    int mq9 = doc["mq9"] | -1;
    int mq135 = doc["mq135"] | -1;
    Serial.println("Data packet received from " + source + ": Temp=" + String(temperature) + ", MQ9=" + String(mq9) + ", MQ135=" + String(mq135));
    factory_display.clear();
    factory_display.drawString(0, 10, "From: " + source);
    factory_display.drawString(0, 20, "Temp: " + String(temperature));
    factory_display.drawString(0, 30, "MQ9: " + String(mq9));
    factory_display.drawString(0, 40, "MQ135: " + String(mq135));
    factory_display.display();

    // Send data to the server
    sendToServer(source, temperature, mq9, mq135);

    StaticJsonDocument<256> ackDoc;
    ackDoc["source"] = chipIDString;
    ackDoc["dest"] = source;
    ackDoc["msg"] = ACK_MSG;
    ackDoc["session_id"] = session_id;
    ackDoc["packet_id"] = packet_id;
    ackDoc["ttl"] = TTL_MAX;
    serializeJson(ackDoc, txpacket, BUFFER_SIZE);
    Serial.printf("Sending ACK: \"%s\"\n", txpacket);
    state = STATE_TX;
  } else if (ttl > 0) {
    Serial.println("Forwarding packet from " + source + " to " + dest);
    doc["ttl"] = ttl - 1;
    serializeJson(doc, txpacket, BUFFER_SIZE);
    delay(random(50, 200));
    state = STATE_FORWARD;
  } else {
    Serial.println("TTL expired, dropping packet.");
    state = STATE_RX;
    Radio.Rx(0);
  }
  doc.clear();
}

void lora_init(void) {
  Mcu.begin(HELTEC_BOARD, SLOW_CLK_TPYE);
  txNumber = 0;
  Rssi = 0;
  rxNumber = 0;
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
  state = STATE_RX;
}

bool resendflag = false;
bool deepsleepflag = false;
bool interrupt_flag = false;
void interrupt_GPIO0() { interrupt_flag = true; }
void interrupt_handle(void) {
  if (interrupt_flag) {
    interrupt_flag = false;
    if (digitalRead(0) == 0) {
      if (rxNumber <= 2) resendflag = true;
      else deepsleepflag = true;
    }
  }
}

void VextON(void) { pinMode(Vext, OUTPUT); digitalWrite(Vext, LOW); }
void VextOFF(void) { pinMode(Vext, OUTPUT); digitalWrite(Vext, HIGH); }

void setup() {
  Serial.begin(115200);
  VextON();
  delay(100);
  factory_display.init();
  factory_display.clear();
  factory_display.display();

  chipid = ESP.getEfuseMac();
  chipIDString = String((uint16_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
  Serial.println("ESP32 ChipID: " + chipIDString);

  session_id = random(0, 65535);
  Serial.println("Session ID: " + String(session_id));

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  attachInterrupt(0, interrupt_GPIO0, FALLING);
  lora_init();
  Radio.Rx(0);

  factory_display.drawString(0, 10, "Mesh Receiver Ready");
  factory_display.drawString(0, 20, chipIDString);
  factory_display.display();
  delay(5000);
  factory_display.clear();

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

void loop() {
  interrupt_handle();
  if (deepsleepflag) {
    VextOFF();
    Radio.Sleep();
    SPI.end();
    pinMode(RADIO_DIO_1, ANALOG);
    pinMode(RADIO_NSS, ANALOG);
    pinMode(RADIO_RESET, ANALOG);
    pinMode(RADIO_BUSY, ANALOG);
    pinMode(LORA_CLK, ANALOG);
    pinMode(LORA_MISO, ANALOG);
    pinMode(LORA_MOSI, ANALOG);
    esp_sleep_enable_timer_wakeup(600 * 1000 * (uint64_t)1000);
    esp_deep_sleep_start();
  }

  if (resendflag) {
    state = STATE_TX;
    resendflag = false;
  }

  switch (state) {
    case STATE_RX:
      Radio.Rx(0);
      state = LOWPOWER;
      break;

    case STATE_TX:
    case STATE_FORWARD:
      txNumber++;
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      state = LOWPOWER;
      break;

    case LOWPOWER:
      Radio.IrqProcess();
      break;

    default:
      break;
  }
}
