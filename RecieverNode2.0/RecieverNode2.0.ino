#include "Arduino.h"
#include "WiFi.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <ArduinoJson.h>

#define RF_FREQUENCY        915000000 // Hz
#define TX_OUTPUT_POWER     10        // dBm
#define LORA_BANDWIDTH      0         // 125 kHz
#define LORA_SPREADING_FACTOR 7      // SF12 giving maxium range with a RX sensititvy of -137 dBm
#define LORA_CODINGRATE     1         // 4/5
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE    1000      // ms
#define BUFFER_SIZE         256

char txpacket[BUFFER_SIZE];
char rxpacket[BUFFER_SIZE];

SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

static RadioEvents_t RadioEvents;
void OnTxDone(void);
void OnTxTimeout(void);
void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);

typedef enum {
  LOWPOWER,
  STATE_RX,
  STATE_TX
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

String ACK_MSG = "ACK";
const char* ReceivedChipID;

void OnTxDone(void) {
  Serial.println("ACK sent successfully.");
  factory_display.clear();
  factory_display.drawString(20, 20, "ACK Sent");
  factory_display.display();
  state = STATE_RX;
  Radio.Rx(0);
}

void OnTxTimeout(void) {
  Radio.Sleep();
  Serial.println("ACK TX Timeout, retrying...");
  factory_display.clear();
  factory_display.drawString(10, 20, "ACK Failed");
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
    state = STATE_RX;
    Radio.Rx(0);
    return;
  }

  // Copy and null-terminate the received packet
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

  // Deserialize the incoming packet
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rxpacket);
  if (error || !doc.containsKey("chipid") || !doc.containsKey("packet_id")) {
    Serial.printf("deserializeJson() failed or missing fields: %s\n", error.c_str());
    state = STATE_RX;
    Radio.Rx(0);
    return;
  }

  // Safely extract chipid as a String
  String receivedChipIDStr = doc["chipid"].as<String>();
  uint16_t received_packet_id = doc["packet_id"];

  // Display received data
  factory_display.clear();
  factory_display.drawString(0, 10, "RX from: " + String(receivedChipIDStr));
  factory_display.drawString(0, 20, "Packet #" + String(received_packet_id));
  factory_display.display();

  // Clear txpacket to prevent residual data
  memset(txpacket, 0, BUFFER_SIZE);

  // Prepare ACK
  doc.clear();
  doc["chipid"] = chipIDString; // Receiver's chip ID
  doc["sender"] = receivedChipIDStr; // Sensor's chip ID, clean String
  doc["msg"] = ACK_MSG;
  doc["packet_id"] = received_packet_id;
  size_t jsonSize = serializeJson(doc, txpacket, BUFFER_SIZE);
  if (jsonSize >= BUFFER_SIZE) {
    Serial.println("ACK JSON too large, skipping...");
    state = STATE_RX;
    Radio.Rx(0);
    return;
  }

  // Log the ACK packet
  Serial.printf("Sending ACK: \"%s\"\n", txpacket);
  Serial.println("Raw bytes: ");
  for (size_t i = 0; i < strlen(txpacket); i++) {
    Serial.printf("%02X ", (uint8_t)txpacket[i]);
  }
  Serial.println();

  state = STATE_TX;
  Radio.Send((uint8_t *)txpacket, strlen(txpacket));
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

  attachInterrupt(0, interrupt_GPIO0, FALLING);
  lora_init();
  Radio.Rx(0);

  factory_display.drawString(0, 10, "Receiver Ready");
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
    pinMode(LORA_MOSI,ANALOG);
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