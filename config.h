#ifndef CONFIG_H
#define CONFIG_H

// SPI (VSPI) Pins for LoRa Communication
#define SCK   18   // SPI Clock
#define MISO  16   // SPI MISO (Master In, Slave Out)
#define MOSI  17   // SPI MOSI (Master Out, Slave In)
#define SS    19    // LoRa Chip Select (NSS)

// LoRa Module Pins
#define RST   15   // LoRa Reset
#define DIO1  13   // LoRa Interrupt Request (IRQ)

// Sensor & Additional Pins
#define ONE_WIRE_BUS  4   // Temperature Sensor (e.g., DS18B20)
#define MQ9_PIN       34  // MQ-9 Gas Sensor
#define MQ135_PIN     35  // MQ-135 Gas Sensor

// LoRa Frequency (United States Standard)
#define LORA_FREQUENCY 915E6

// Thresholds for Wildfire Detection
const float TEMP_THRESHOLD   = 50.0; // 50°C as a dangerous temperature
const float MQ9_THRESHOLD    = 200.0;
const float MQ135_THRESHOLD  = 300.0;

// Timings
unsigned long lastSensorRead = 0;
unsigned long lastSend       = 0;
const unsigned long readInterval = 2UL * 60UL * 1000UL;  // 2 minutes
const unsigned long sendInterval = 10UL * 60UL * 1000UL; // 10 minutes

#endif // CONFIG_H
