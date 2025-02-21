#include "Arduino.h"
#include "WiFi.h"
#include "LoRaWan_APP.h" // For the LoRa Module 
#include <Wire.h>
#include "HT_SSD1306Wire.h" // For OLED Screen 
#include <ArduinoJson.h>  // For JSON formatting

/********************************* lora  *********************************************/
#define RF_FREQUENCY                                915000000 // Hz
#define TX_OUTPUT_POWER                             10        // dBm
#define LORA_BANDWIDTH                              0         // [0: 125 kHz,
                                                              //  1: 250 kHz,
                                                              //  2: 500 kHz,
                                                              //  3: Reserved]
#define LORA_SPREADING_FACTOR                       7         // [SF7..SF12]
#define LORA_CODINGRATE                             1         // [1: 4/5,
                                                              //  2: 4/6,
                                                              //  3: 4/7,
                                                              //  4: 4/8]
#define LORA_PREAMBLE_LENGTH                        8         // Same for Tx and Rx
#define LORA_SYMBOL_TIMEOUT                         0         // Symbols
#define LORA_FIX_LENGTH_PAYLOAD_ON                  false
#define LORA_IQ_INVERSION_ON                        false
#define RX_TIMEOUT_VALUE                            1000
#define BUFFER_SIZE                                 256 // Define the payload size here

char txpacket[BUFFER_SIZE]; // Packet size of transmit 
char rxpacket[BUFFER_SIZE]; // Packet size of RX 

/********************************* Method Declarations  *****************************/
static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );
float readTemperature();
int readMQ9();
int readMQ135();

/********************************* State Definitions  *****************************/
typedef enum
{
  LOWPOWER,
  STATE_RX,
  STATE_TX,
  STATE_WAIT_ACK
} States_t;

/********************************* Global Variables  *****************************/
int16_t txNumber; // 16 Byte sized int 
int16_t rxNumber; // 16 Byte sized int 
States_t state;   // Variable to reference state 
int16_t Rssi, rxSize; // 16 Byte sized int(s)

unsigned int counter = 0; 
bool receiveflag = false; // Flag: new LoRa message received
long lastSendTime = 0; 
int interval = 1000; 
uint64_t chipid;  // Unique chip ID for each node 
String chipIDString; // Global chipID string storage 
int16_t RssiDetection = 0; 

/********************************* ACK Global Variables  *****************************/
const char* ACK_MSG = "ACK"; // ACK message definition 
unsigned long ackWaitStart = 0;
const unsigned long ackTimeout = 10000; // 10 seconds timeout for ACK
int ackRetries = 0;
const int maxRetries = 3;

/********************************* OLED display initialization *********************************************/
SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

/********************************* Sesnor PIN Definitions  *****************************/
#define TEMP_SENSOR_PIN       34  // Example ADC pin for temperature sensor
#define MQ9_SENSOR_PIN        35  // Example ADC pin for MQ-9 sensor
#define MQ135_SENSOR_PIN      32  // Example ADC pin for MQ-135 sensor

/********************************* TX Methods  ****************************************/
void OnTxDone(void)
{
  factory_display.clear();
  factory_display.drawString(0, 20, "Packet Sent, awaiting ACK...");
  factory_display.display(); 
  Serial.println("TX done, switching to RX for ACK..."); // After TX, we wait for an ACK.
  ackWaitStart = millis(); //Record the time when ACK waiting begins 
  state = STATE_WAIT_ACK; // Set state to waiting for ACK 
  Radio.Rx(RX_TIMEOUT_VALUE); //Start RX mode with a timeout.
}

void OnTxTimeout(void)
{
  Radio.Sleep(); // Sleep the radio, TX failed 
  Serial.println("TX Timeout, retrying transmission...");
  state = STATE_TX;  // Optionally, implement a retry mechanism
}


/********************************* RX Methods  ****************************************/
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr )
{
  rxNumber++; // Increment the recieve number variable 
  Rssi = rssi; 
  rxSize = size; // Size of recieving packet 
  memcpy(rxpacket, payload, size); 
  rxpacket[size] = '\0'; 
  Radio.Sleep(); 
  Serial.printf("Received packet: \"%s\" with RSSI: %d, length: %d\n", rxpacket, Rssi, rxSize);
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, rxpacket);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;  // Handle error appropriately
  }

  if(strcmp(doc["sender"].as<const char*>(), chipIDString.c_str()) == 0)
  {
    // Strings are equal—ACK received.
    Serial.println("ACK received.");
    factory_display.clear();
    factory_display.drawString(20, 20, "ACK received!");
    factory_display.display(); 
    ackRetries = 0;   // Reset retry count for the next transmission 
    state = LOWPOWER; // ACK received—transmission confirmed.
  } else {
    state = LOWPOWER;
  }
  // Check if the received message is an ACK.
  // if (strcmp(rxpacket, ACK_MSG) == 0) {
  //   Serial.println("ACK received.");
  //   factory_display.clear();
  //   factory_display.drawString(20, 20, "ACK recieved!");
  //   factory_display.display(); 
  //   ackRetries = 0;   // Reset retry count for the next transmission 
  //   state = LOWPOWER; // ACK received—transmission confirmed.
  // } else {
  //   // Handle other received data or ignore.
  //   state = LOWPOWER;
  // }
}

/********************************* LoRa Initialization ***********************************/
void lora_init(void)
{
  Mcu.begin(HELTEC_BOARD,SLOW_CLK_TPYE);
  txNumber=0; // Initialize to 0
  Rssi=0; // Initialize to 0
  rxNumber = 0; // Initialize to 0
  RadioEvents.TxDone = OnTxDone;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxDone = OnRxDone;

  Radio.Init( &RadioEvents );
  Radio.SetChannel( RF_FREQUENCY );
  Radio.SetTxConfig( MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                                 LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                                 LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                                 true, 0, 0, LORA_IQ_INVERSION_ON, 3000 );

  Radio.SetRxConfig( MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                                 LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                                 LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                                 0, true, 0, 0, LORA_IQ_INVERSION_ON, true );
	state=STATE_TX;
}

/********************************* Interupt Handling *********************************************/

bool resendflag=false;
bool deepsleepflag=false;
bool interrupt_flag = false;
void interrupt_GPIO0()
{
	interrupt_flag = true;
}
void interrupt_handle(void)
{
	if(interrupt_flag)
	{
		interrupt_flag = false;
		if(digitalRead(0)==0)
		{
			if(rxNumber <=2)
			{
				resendflag=true;
			}
			else
			{
				deepsleepflag=true;
			}
		}
	}
}

void VextON(void)
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, LOW);
  
}

void VextOFF(void) //Vext default OFF
{
  pinMode(Vext,OUTPUT);
  digitalWrite(Vext, HIGH);
}

/********************************* Sensor Reading Functions *********************************************/
// float readTemperature() {
//   // Example for an analog temperature sensor (e.g., LM35) on TEMP_SENSOR_PIN:
//   int sensorValue = analogRead(TEMP_SENSOR_PIN);
//   // Convert ADC value to voltage (assuming 3.3V reference and 12-bit ADC) then to temperature.
//   float voltage = sensorValue * (3.3 / 4095.0);
//   float temperature = voltage * 100.0; // Adjust conversion according to your sensor
//   return temperature;
// }

// int readMQ9() {
//   // Read analog value from MQ-9 sensor
//   return analogRead(MQ9_SENSOR_PIN);
// }

// int readMQ135() {
//   // Read analog value from MQ-135 sensor
//   return analogRead(MQ135_SENSOR_PIN);
// }

// TEMPORARY TEST CODE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
float readTemperature() {
  return 25.5;  // Simulated temperature value
}

int readMQ9() {
  return 123;   // Simulated MQ-9 reading
}

int readMQ135() {
  return 456;   // Simulated MQ-135 reading
}

/********************************* Setup and Loop *********************************************/
void setup()
{
  Serial.begin(115200);
  VextON();
  delay(100);
  factory_display.init();
  factory_display.clear();
  factory_display.display();

 // Obtain and format chip ID for sensor node identification
  chipid = ESP.getEfuseMac();  // The chip ID is essentially the MAC address
  chipIDString = String((uint16_t)(chipid >> 32), HEX) + String((uint32_t)chipid, HEX);
  Serial.print("ESP32 ChipID: ");
  Serial.println(chipIDString);
  
  attachInterrupt(0, interrupt_GPIO0, FALLING); // Startup the interupts 
  lora_init(); // Startup the LoRa module 

   // Initial OLED message
  factory_display.drawString(0, 10,"Chip ID: " + chipIDString);
  factory_display.drawString(0, 20,"Sensor Module Ready"); 
  factory_display.display();
  delay(5000);
  factory_display.clear();
  
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}

unsigned long lastSensorRead = 0; // Keep track of last sensor read 
const unsigned long sensorInterval = 5000; // Read sensors every 5 seconds

void loop()
{
 
  interrupt_handle(); // Ensure each loop properly handles the interupts 
   if(deepsleepflag) // all the stuff to do for deep sleep 
  {
    VextOFF();
    Radio.Sleep();
    SPI.end();
    pinMode(RADIO_DIO_1,ANALOG);
    pinMode(RADIO_NSS,ANALOG);
    pinMode(RADIO_RESET,ANALOG);
    pinMode(RADIO_BUSY,ANALOG);
    pinMode(LORA_CLK,ANALOG);
    pinMode(LORA_MISO,ANALOG);
    pinMode(LORA_MOSI,ANALOG);
    esp_sleep_enable_timer_wakeup(600*1000*(uint64_t)1000);
    esp_deep_sleep_start();
  }

  if(resendflag)
  {
    state = STATE_TX; // Change state to Transmit 
    resendflag = false; // Switch resend flag back to false (avoid infinite resend loop)
  }

    // Periodically read sensor values and transmit data
  if (millis() - lastSensorRead >= sensorInterval && state == LOWPOWER)
  {
    lastSensorRead = millis();
    
    // Read sensor data
    float temperature = readTemperature();
    int mq9 = readMQ9();
    int mq135 = readMQ135();

    factory_display.clear();
    factory_display.drawString(0, 10, "Temp: " + String(temperature));
    factory_display.drawString(0, 20, "MQ-9: " + String(mq9)); 
    factory_display.drawString(0, 30, "MQ-135: " + String(mq135));
    factory_display.display();
    delay(100); 

    // Create JSON payload using ArduinoJson
    StaticJsonDocument<256> doc;
    doc["chipid"] = chipIDString;    // Include chip ID for node identification
    doc["temperature"] = temperature;
    doc["mq9"] = mq9;
    doc["mq135"] = mq135;
    doc["packet_number"] = txNumber;
    
    // Serialize JSON into txpacket buffer
    serializeJson(doc, txpacket, BUFFER_SIZE);
    
    Serial.printf("Sending JSON: %s\n", txpacket);
    
    // Set state to TX to send the packet
    state = STATE_TX;
  }
  /********************** STATE Machine for LoRa operations *******************/ 
  switch (state)
  {
    case STATE_TX:
      delay(100);  // Optional delay before TX
      txNumber++;
      Serial.printf("Sending packet \"%s\", length %d\n", txpacket, strlen(txpacket));
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      // OnTxDone() will switch state to STATE_WAIT_ACK.
      state = LOWPOWER;
      break;
      
    case STATE_WAIT_ACK:
     // Check if ACK timeout has occurred
      if (millis() - ackWaitStart >= ackTimeout) {
        ackRetries++;
        if (ackRetries < maxRetries) {
            factory_display.clear();
            factory_display.drawString(20, 20, "No ACK, retrying TX...");
            factory_display.display(); 
          Serial.println("No ACK, Retrying TX...");
          state = STATE_TX;  // Retransmit packet
        } else {
          factory_display.clear();
          factory_display.drawString(20, 20, "Max ACK reached, dropping packet...");
          factory_display.display(); 
          Serial.println("Max ACK retries reached, giving up on this packet.");
          ackRetries = 0;    // Reset for next transmission
          state = LOWPOWER;
        }
      }
      break;
      
    case STATE_RX:
      Serial.println("Entering RX mode...");
      Radio.Rx(0);
      state = LOWPOWER;
      break;
      
    case LOWPOWER:
      Radio.IrqProcess();
      break;
      
    default:
      break;
  }
}
