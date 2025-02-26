#include "Arduino.h"
#include "WiFi.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
#include <ArduinoJson.h>

#define RF_FREQUENCY        915000000 // Hz
#define TX_OUTPUT_POWER     10        // dBm
#define LORA_BANDWIDTH      0         // 125 kHz
#define LORA_SPREADING_FACTOR 7     // SF12 giving maxium range with a RX sensititvy of -137 dBm
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
float readTemperature();
int readMQ9();
int readMQ135();

typedef enum {
  LOWPOWER,
  STATE_RX,
  STATE_TX,
  STATE_WAIT_ACK
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
int interval = 1000;
uint16_t last_packet_id = 0; // Track last sent packet ID

const char* ACK_MSG = "ACK";
unsigned long ackWaitStart = 0;
const unsigned long ackTimeout = 1500; // 3.5 second
int ackRetries = 0;
const int maxRetries = 3;

#define TEMP_SENSOR_PIN  34
#define MQ9_SENSOR_PIN   35
#define MQ135_SENSOR_PIN 32

void OnTxDone(void) {
  Serial.println("TX done, awaiting ACK...");
  factory_display.clear();
  factory_display.drawString(0, 20, "Packet Sent");
  factory_display.display();
  ackWaitStart = millis();
  state = STATE_WAIT_ACK;
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

  if (!doc.containsKey("msg") || !doc.containsKey("sender") || !doc.containsKey("packet_id")) {
    Serial.println("Missing required fields, ignoring...");
    state = LOWPOWER;
    return;
  }

  const char* msg = doc["msg"];
  const char* sender = doc["sender"];
  uint16_t received_packet_id = doc["packet_id"];

  if (strcmp(msg, ACK_MSG) == 0 && strcmp(sender, chipIDString.c_str()) == 0) {
    if (received_packet_id == last_packet_id) {
      Serial.println("Valid ACK received for packet #" + String(received_packet_id));
      factory_display.clear();
      factory_display.drawString(20, 20, "ACK received!");
      factory_display.display();
      ackRetries = 0;
      state = LOWPOWER;
    } else {
      Serial.printf("ACK packet_id mismatch: expected %d, got %d\n", last_packet_id, received_packet_id);
    }
  } else {
    Serial.println("Not a valid ACK, ignoring...");
    state = LOWPOWER;
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
  state = LOWPOWER;
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

float readTemperature() { return 25.5; } // Simulated
int readMQ9() { return 123; }
int readMQ135() { return 456; }

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

  factory_display.drawString(0, 10, "Chip ID: " + chipIDString);
  factory_display.drawString(0, 20, "Sensor Ready");
  factory_display.display();
  delay(5000);
  factory_display.clear();

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

unsigned long lastSensorRead = 0;
const unsigned long sensorInterval = 5000;

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
    doc["chipid"] = chipIDString;
    doc["temperature"] = temperature;
    doc["mq9"] = mq9;
    doc["mq135"] = mq135;
    doc["packet_id"] = packet_id++;
    size_t jsonSize = serializeJson(doc, txpacket, BUFFER_SIZE);
    if (jsonSize >= BUFFER_SIZE) {
      Serial.println("JSON too large, skipping...");
      return;
    }

    Serial.printf("Sending packet #%d: %s\n", last_packet_id, txpacket);
    state = STATE_TX;
  }

  switch (state) {
    case STATE_TX:
      txNumber++;
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      state = LOWPOWER;
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