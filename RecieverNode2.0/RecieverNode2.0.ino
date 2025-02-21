#include "Arduino.h"
#include "WiFi.h"
#include "LoRaWan_APP.h"
#include <Wire.h>
#include "HT_SSD1306Wire.h"
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
/********************************* OLED display initialization *********************************************/
SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

/********************************* Method Declarations  *****************************/
static RadioEvents_t RadioEvents;
void OnTxDone( void );
void OnTxTimeout( void );
void OnRxDone( uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr );

/********************************* State Definitions  *****************************/
typedef enum
{
  LOWPOWER,
  STATE_RX,
  STATE_TX,
} States_t;

/********************************* Global Variables  *****************************/
int16_t txNumber; // 16 Byte sized int 
int16_t rxNumber; // 16 Byte sized int 
States_t state;   // Variable to reference state 
int16_t Rssi, rxSize; // 16 Byte sized int(s)

uint64_t chipid;  // Unique chip ID for each node 
String chipIDString; // Global chipID string storage 
unsigned int counter = 0; 
bool receiveflag = false; // Flag: new LoRa message received
long lastSendTime = 0; 
int interval = 1000; 
int16_t RssiDetection = 0; 
/********************************* ACK Global Variables  *****************************/
String ACK_MSG = "ACK"; // ACK message definition 
const char* RecievedChipID; 
// Create a JSON document with the same capacity used during serialization
StaticJsonDocument<256> doc;

/********************************* TX Methods  ****************************************/
void OnTxDone(void)
{
  factory_display.clear();
  factory_display.drawString(20, 20, "ACK SENT");
  factory_display.display(); 
  Serial.println("ACK sent successfully."); // After TX, we wait for an ACK.
  state = STATE_RX; // Set state to waiting for ACK 
  Radio.Rx(0); 
}

void OnTxTimeout(void)
{
  factory_display.clear();
  factory_display.drawString(10, 20, "ACK Failed, Retrying...");
  factory_display.display(); 
  Radio.Sleep(); // Sleep the radio, TX failed 
  Serial.println("ACK TX Timeout, retrying transmission...");
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

  // Deserialize the JSON payload from the rxpacket buffer
  DeserializationError error = deserializeJson(doc, rxpacket);
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;  // Handle error appropriately
  }
  RecievedChipID = doc["chipid"]; 
  doc.clear();  // Clears all key-value pairs, resetting the document for reuse

  // Update the OLED with the received message
  factory_display.clear();
  factory_display.drawString(0, 10, "RX:");
  factory_display.drawString(0, 20, rxpacket);
  factory_display.display();
  delay(1000); 

  state = STATE_TX; 
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
	state=STATE_RX; // Start in recieve mode 
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
  
 // attachInterrupt(0, interrupt_GPIO0, FALLING); // Startup the interupts 
  lora_init(); // Startup the LoRa module 
  Radio.Rx(0); 

   // Initial OLED message
  factory_display.drawString(0, 10, "Receiver Ready");
  factory_display.drawString(0, 20, chipIDString);
  factory_display.display();
  delay(5000);
  factory_display.clear();
  factory_display.display(); 

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);
}


void loop()
{
  delay(2000); 
  factory_display.clear();
  factory_display.drawString(0, 10, "Waiting for data...");
  factory_display.display();
  delay(100);
  

  Radio.IrqProcess(); 

  /********************** STATE Machine for LoRa operations *******************/ 
  switch (state)
  {
    case STATE_TX:
      Serial.println("Sending ACK...");
        doc["chipid"] = chipIDString;  
        doc["sender"] = RecievedChipID; 
        doc["msg"] = ACK_MSG;
        serializeJson(doc, txpacket, BUFFER_SIZE);
        Serial.printf("Sending JSON: %s\n", txpacket);
      Radio.Send((uint8_t *)txpacket, strlen(txpacket));
      txNumber++;
      // Immediately change state to a temporary state (or simply wait for the callback)
      state = STATE_RX;  
      break;
      
    case STATE_RX:
      // In RX mode, simply process interrupts
      Radio.IrqProcess();
      break;
      
    default:
      break;
  }

}
