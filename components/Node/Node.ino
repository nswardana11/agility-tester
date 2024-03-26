#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "Adafruit_VL53L0X.h"
#include <Adafruit_NeoPixel.h>
#define buzzerandLEDPin 15
#define numLED          12

const int CE        = 0;
const int CSN       = 2;

const char* ssid                = "iPhone";
const char* password            = "omswastiastu";
const char* serverConfigUrl     = "http://weather-database.site/agility-tester/handler/getConfig.php";
const byte address1[6]          = "00001";
const byte address2[6]          = "00002";

struct Config  {
  int nodeRangeBot;
  int nodeRangeTop;
  int nodeDelay;
};
Config config;

Adafruit_VL53L0X lox = Adafruit_VL53L0X();
Adafruit_NeoPixel strip(numLED, buzzerandLEDPin, NEO_GRB + NEO_KHZ800);
RF24 radio(CE, CSN);

void connectWiFi() {
  pinMode(buzzerandLEDPin, OUTPUT);
  Wire.begin();
  WiFi.begin(ssid, password);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    i++;
    delay(500);
  }
  digitalWrite(buzzerandLEDPin, HIGH);   
  delay(500);                      
  digitalWrite(buzzerandLEDPin, LOW);    
  delay(500);
}

void getConfig(){

  WiFiClient client;
  HTTPClient http;

  int getConfig = 0;
  int failsReqCount = 0;

  while (getConfig == 0) {
    failsReqCount++;
    http.begin(client, serverConfigUrl);
    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      Serial.println(response);

      if (response == "0 results") {
        Serial.println(getConfig);
        delay(500); 
        } else {
          getConfig = 1;
          Serial.println("result available");

          const size_t capacity = JSON_ARRAY_SIZE(6) + 6*JSON_OBJECT_SIZE(1) + 270;
          DynamicJsonDocument doc(capacity);

          DeserializationError error = deserializeJson(doc, response);
          if (error) {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return;
          }

          config.nodeRangeBot   = doc[0]["range_node_bottom_mm"];
          config.nodeRangeTop   = doc[0]["range_node_top_mm"];
          config.nodeDelay      = doc[0]["node_delay_ms"];
        }
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        }
        Serial.println("end getConfig");
  }
   for (int j = 0; j < 2; j++) {
    digitalWrite(buzzerandLEDPin, HIGH);   
    delay(500);                      
    digitalWrite(buzzerandLEDPin, LOW);    
    delay(500);                 
  }
}

void nRF24Config(){
    lox.begin();
    strip.begin();           
    strip.show();            
    strip.setBrightness(100); 
    radio.begin();
    radio.setDataRate(RF24_2MBPS);
    radio.setChannel(124);
    radio.openWritingPipe(address1);
    radio.openReadingPipe(1, address2);
    radio.setPALevel(RF24_PA_MIN);
    digitalWrite(buzzerandLEDPin, LOW);
    VL53L0X_RangingMeasurementData_t measure;
}

void setup() {

  Serial.begin(9600);

  connectWiFi();

  getConfig();

  nRF24Config();
}

void loop() {
  radio.startListening();
  char message[32] = "";
  if(radio.available()){
    radio.read(&message, sizeof(message));
    Serial.println(message);
    String message_str = String(message);
    if(message_str == "1") {
    for (int i = 0; i < strip.numPixels(); i++) {
      strip.setPixelColor(i, 0, 0, 255);
    }
      strip.show();
        while(true){
          VL53L0X_RangingMeasurementData_t measure;
          lox.rangingTest(&measure, false);
          int distance = measure.RangeMilliMeter;
          while(distance > config.nodeRangeTop | distance < config.nodeRangeBot){
            VL53L0X_RangingMeasurementData_t measure;
            lox.rangingTest(&measure, false);
            distance = measure.RangeMilliMeter;
            Serial.println(measure.RangeMilliMeter);
            radio.stopListening();
          }
          for (int i = 0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, 0, 0, 0);
          }
          strip.show();
          radio.stopListening();

          const char data[] = "1";
          if(radio.write(&data, sizeof(data))){
          }
          else{}
          break;
        }
    }
  }
  Serial.println("waiting for server");
  delay(config.nodeDelay);
  }