#include <WiFi.h>
#include <ArduinoJson.h>
#include "LoRaWan_APP.h"
#include "HT_SSD1306Wire.h"

SSD1306Wire factory_display(0x3c, 500000, SDA_OLED, SCL_OLED, GEOMETRY_128_64, RST_OLED);

// Replace with your WiFi credentials
const char* ssid = "XXXXXX";      // Your WiFi network name
const char* password = "XXXXXXX"; // Your WiFi password

WiFiServer server(80);

struct SensorData {
  String chipid;
  float temperature;
  int mq9;
  int mq135;
  uint16_t packet_id;
  float lat;
  float lon;
  String fireRisk;
};
SensorData fireData[10];
int dataCount = 0;

void addTestData() {
  fireData[dataCount].chipid = "2425abcd1234"; 
  fireData[dataCount].temperature = 25.5;
  fireData[dataCount].mq9 = 123;
  fireData[dataCount].mq135 = 456;
  fireData[dataCount].packet_id = 1;
  fireData[dataCount].lat = 37.7749;
  fireData[dataCount].lon = -122.4194;
  fireData[dataCount].fireRisk = (fireData[dataCount].temperature > 50 || fireData[dataCount].mq9 > 200) ? "High" : "Low";
  dataCount++;

  fireData[dataCount].chipid = "f4e53e43ca48";
  fireData[dataCount].temperature = 60.0;
  fireData[dataCount].mq9 = 250;
  fireData[dataCount].mq135 = 300;
  fireData[dataCount].packet_id = 2;
  fireData[dataCount].lat = 37.7849;
  fireData[dataCount].lon = -122.4094;
  fireData[dataCount].fireRisk = (fireData[dataCount].temperature > 50 || fireData[dataCount].mq9 > 200) ? "High" : "Low";
  dataCount++;

  fireData[dataCount].chipid = "1234def56789";
  fireData[dataCount].temperature = 22.0;
  fireData[dataCount].mq9 = 100;
  fireData[dataCount].mq135 = 150;
  fireData[dataCount].packet_id = 3;
  fireData[dataCount].lat = 37.7649;
  fireData[dataCount].lon = -122.4294;
  fireData[dataCount].fireRisk = (fireData[dataCount].temperature > 50 || fireData[dataCount].mq9 > 200) ? "High" : "Low";
  dataCount++;

  Serial.println("Added " + String(dataCount) + " test data points.");
}

void setup() {
  Serial.begin(115200);

  // Initialize the display
  factory_display.init();
  factory_display.clear();
  factory_display.setFont(ArialMT_Plain_10);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Display IP on OLED
  String ipAddress = WiFi.localIP().toString();
  factory_display.clear();
  factory_display.drawString(5, 5, ipAddress.c_str());  // Use .c_str() here
  factory_display.display();

  addTestData();
  server.begin();
}
void loop() {
  WiFiClient client = server.available();
  if (client) {
    String request = client.readStringUntil('\r');
    Serial.println("Request: " + request);
    client.flush();

    if (request.indexOf("GET / ") != -1 || request.indexOf("GET /index.html") != -1) { // Root or index.html
      Serial.println("Serving map page...");
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println("<!DOCTYPE html>");
      client.println("<html>");
      client.println("<head>");
      client.println("<title>Fire Map</title>");
      client.println("<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css' />");
      client.println("<style>#map { height: 600px; width: 100%; border: 1px solid black; }</style>");
      client.println("</head>");
      client.println("<body>");
      client.println("<h1>Fire Risk Map</h1>");
      client.println("<div id='map'></div>");
      client.println("<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>");
      client.println("<script>");
      client.println("console.log('Leaflet script loaded');");
      client.println("var map = L.map('map', { zoomControl: true }).setView([37.7749, -122.4194], 12);");
      client.println("L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', {");
      client.println("  attribution: '© <a href=\"https://www.openstreetmap.org/copyright\">OpenStreetMap</a>',");
      client.println("  maxZoom: 18");
      client.println("}).addTo(map);");
      Serial.println("Generating " + String(dataCount) + " markers...");
      for (int i = 0; i < dataCount; i++) {
        client.print("L.circleMarker([");
        client.print(fireData[i].lat, 6);
        client.print(",");
        client.print(fireData[i].lon, 6);
        client.print("], {radius: 10, color: '");
        client.print(fireData[i].fireRisk == "High" ? "red" : "green");
        client.print("', fillOpacity: 0.5}).addTo(map).bindPopup('");
        client.print("ChipID: " + fireData[i].chipid + "<br>Temp: " + String(fireData[i].temperature));
        client.print("°C<br>MQ-9: " + String(fireData[i].mq9) + "<br>MQ-135: " + String(fireData[i].mq135));
        client.print("<br>Risk: " + fireData[i].fireRisk + "');");
      }
      client.println("console.log('Markers added');");
      client.println("</script>");
      client.println("</body>");
      client.println("</html>");
      Serial.println("HTML sent.");
    } else {
      client.println("HTTP/1.1 404 Not Found");
      client.println("Connection: close");
      client.println();
    }
    delay(10);
    client.stop();
    Serial.println("Client closed.");
  }
}
