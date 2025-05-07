// Library 
#include "PMS.h"
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>

#define RXD2 16
#define TXD2 17
HardwareSerial pmsSerial(2);
PMS pms(pmsSerial);
PMS::DATA data;

// WiFi credentials
const char* ssid = "Berserk";
const char* password = "123456789";
const char* serverURL = "http://192.168.203.12:5000/data"; 


// MQTT broker details (replace with Raspberry Pi's IP address)
const char* mqtt_server = "192.168.229.242";
const int mqtt_port = 1883;
const char* topic = "sensor/data";


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <DHT.h>
Adafruit_SSD1306 display(128, 64, &Wire, -1);
#define DHTPIN 4          // DHT11 Sensor Pin
#define DHTTYPE    DHT11
DHT dht(DHTPIN, DHTTYPE);

//Initialize pin
int mq02_pin = 34;
int mq02_value = 0;
int mp_pin = 35;
int mp_value = 0;
int led = 2;



// WiFi and MQTT clients
WiFiClient espClient;
PubSubClient mqttClient(espClient);



// Function to connect to Wi-Fi
void connectWiFi() {
  Serial.println("==== Connecting to Wi-Fi ====");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("=============================");
}


// Function to connect to the MQTT broker
void connectMQTT() {
  Serial.println("\n==== Connecting to MQTT Broker ====");
  while (!mqttClient.connected()) {
    Serial.print("Attempting connection...");
    if (mqttClient.connect("ESP32Publisher")) {  // MQTT Client ID
      Serial.println("Connected!");
    } else {
      Serial.print("Failed (State: ");
      Serial.print(mqttClient.state());
      Serial.println("). Retrying...");
      delay(2000);
    }
  }
  Serial.println("===============================");
}




void setup()
{
  Serial.begin(9600);
  pmsSerial.begin(9600, SERIAL_8N1, RXD2, TXD2);
  dht.begin();
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println(F("Display failed"));
    for (;;);
  }
  delay(2000);
  display.clearDisplay();
  display.setTextColor(WHITE);
  pinMode(mq02_pin, INPUT);
  pinMode(mp_pin, INPUT);
  pinMode(led, OUTPUT);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");


  
  // Set up MQTT
  mqttClient.setServer(mqtt_server, mqtt_port);
  connectMQTT();


}







void sendDataToServer(float temperature, float humidity, int mq02, int mp, PMS::DATA pmsData) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    // Build JSON payload
    String jsonData = "{";
    jsonData += "\"temperature\":" + String(temperature) + ",";
    jsonData += "\"humidity\":" + String(humidity) + ",";
    jsonData += "\"mq02\":" + String(mq02) + ",";
    jsonData += "\"mp\":" + String(mp) + ",";
    jsonData += "\"pm1.0\":" + String(pmsData.PM_AE_UG_1_0) + ",";
    jsonData += "\"pm2.5\":" + String(pmsData.PM_AE_UG_2_5) + ",";
    jsonData += "\"pm10\":" + String(pmsData.PM_AE_UG_10_0);
    jsonData += "}";

    // Send HTTP POST request
    http.begin(serverURL);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(jsonData);

    if (httpResponseCode > 0) {
      Serial.println("Data sent successfully!");
    } else {
      Serial.println("Error sending data: " + String(httpResponseCode));
    }

    http.end();
  } else {
    Serial.println("WiFi not connected");
  }
}


void loop()
{
  //delay(1000);
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  Serial.println("HERE");
  Serial.print(t);
  Serial.print(h);
  Serial.println("   ");
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT Sensor");
  }


    // Reconnect if MQTT client is disconnected
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();




  mq02_value = analogRead(mq02_pin);
  mp_value = analogRead(mp_pin);


  if (pms.read(data))
  {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.print("Temp: ");
    display.setCursor(30, 10);
    display.print(t);
    display.print(" ");
    display.setCursor(70, 10);
    display.print("Hum: ");
    display.setCursor(95, 10);
    display.print(h);
    display.setCursor(0, 20);
    display.print("mq2: ");
    display.setCursor(30, 20);
    display.print(mq02_value);
    display.setCursor(70, 20);
    display.print("AQ: ");
    display.setCursor(95, 20);
    display.print(mp_value);
    display.setCursor(0, 30);
    display.print("Dust Concent. (ug/m3)");
    display.setCursor(0, 40);
    display.print("PM1.0: ");
    display.setCursor(35, 40);
    display.print(String(data.PM_AE_UG_1_0));
    display.setCursor(65, 40);
    display.print("PM2.5: ");
    display.setCursor(105, 40);
    display.print(String(data.PM_AE_UG_2_5));
    display.setCursor(0, 50);
    display.print("PM10: ");
    display.setCursor(35, 50);
    display.print(String(data.PM_AE_UG_10_0));
    display.display();

    Serial.print("Dust Concentration  ");
    Serial.print("PM1.0 :" + String(data.PM_AE_UG_1_0) + "(ug/m3)");
    Serial.print("PM2.5 :" + String(data.PM_AE_UG_2_5) + "(ug/m3)");
    Serial.print("PM10  :" + String(data.PM_AE_UG_10_0) + "(ug/m3)");
    Serial.println();
    Serial.print("Temperature: ");
    Serial.print(t);
    Serial.print("  C");
    Serial.print("    Humidity:  ");
    Serial.print(h);
    Serial.print("  %");
    Serial.print("    mq2 value:  ");
    Serial.print(mq02_value);
    Serial.print("    mp air qiality value:  ");
    Serial.print(mp_value);
    Serial.println();
  }


  if (mq02_value > 3000)
  {
    digitalWrite(led, HIGH);
  }
  else
  {
    digitalWrite(led, LOW);
  }

  // Create CSV message: "t,h,mq02,mp,pm1.0,pm2.5,pm10"
String message = String((int)t) + "," + 
                 String((int)h) + "," + 
                 String(mq02_value) + "," + 
                 String(mp_value) + "," + 
                 String((int)data.PM_AE_UG_1_0) + "," + 
                 String((int)data.PM_AE_UG_2_5) + "," + 
                 String((int)data.PM_AE_UG_10_0);

// Convert to char array
char mqttPayload[message.length() + 1];
message.toCharArray(mqttPayload, message.length() + 1);

// Publish CSV message
if (mqttClient.publish(topic, mqttPayload)) {
    Serial.println("Published:");
    Serial.println(message);
} else {
    Serial.println("Failed to publish message");
}




}