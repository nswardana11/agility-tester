#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <virtuabotixRTC.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <stdio.h>
#include <math.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

const int buzzerPin = 16;
const int CE        = 0;
const int CSN       = 2;
const int CLK       = 3;
const int DAT       = 1;
const int RST       = 15;

int randNum;
int nodeNum;
std::vector<int> patternArray;

const char* ssid                = "iPhone";
const char* password            = "omswastiastu";
const char* serverConfigUrl     = "http://weather-database.site/agility-tester/handler/getConfig.php";
const char* serverUploadDataurl = "http://weather-database.site/agility-tester/handler/uploadDataTraining.php";

const byte address1[6]      = "00001";
const byte address2[6]      = "00002";


struct Timestamp {
  time_t currentTime;
  int currentSeconds;
  int currentMillis;
  int currentMinutes;
  int currentHours;
  int currentDay;
  int currentMonth;
  int currentYear;
  int prevHours;
  int prevMinutes;
  int prevSeconds;
  int prevMillis;
  String timestamp;
  String prevTimestamp;
  int timeDiffSec;
  int timeDiffMillis;
};
Timestamp times;

struct Config  {
  String session_ID;
  String responseJSON;
  String username;
  String mode;
  std::vector<int> patternArray;
  String patternJSON;
  String patternType;
  int repetition;
  String status;
  int controlBoxDelay;
};
Config config;

struct Data {
  String session_ID;
  String username;
  int runNum;
  int repsNum;
  String nodeID;
  String prevTimestamp;
  String timestamp;
  int timeDiffSec;
  int timeDiffMillis;
};
Data data[500];

WiFiUDP ntpUDP;
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200); // Mengatur offset ke GMT+7
virtuabotixRTC rtcds1302(CLK, DAT, RST);
RF24 radio(CE, CSN);

void displayOLED(float textSize, int textColor, int cursorX, int cursorY, const char* textOutput) {
  display.setTextSize(textSize);
  display.setTextColor(textColor);
  display.setCursor(cursorX, cursorY);
  display.println(textOutput);
  display.display();
}

void connectWiFi() {

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  pinMode(buzzerPin, OUTPUT);
  display.clearDisplay();
  displayOLED(1, SSD1306_WHITE, 10, 10, "Connecting to WiFi");
  WiFi.begin(ssid, password);

  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
    displayOLED(1, SSD1306_WHITE, 10 + i, 20, ".");
    i++;
    delay(500);
  }

  display.clearDisplay();
  displayOLED(1, SSD1306_WHITE, 10, 10, "Connecting to WiFi");
  displayOLED(2, SSD1306_WHITE, 20, 30, "Connected");
  display.clearDisplay();

  for (int j = 0; j < 2; j++) {
    digitalWrite(buzzerPin, HIGH);   
    delay(500);                      
    digitalWrite(buzzerPin, LOW);    
    delay(500); 
  }
}

void updateRTC() {

  timeClient.begin();
  timeClient.update();

  times.currentTime     = timeClient.getEpochTime();
  times.currentSeconds  = second(times.currentTime);
  times.currentMinutes  = minute(times.currentTime);
  times.currentHours    = hour(times.currentTime);
  times.currentDay      = day(times.currentTime);
  times.currentMonth    = month(times.currentTime);
  times.currentYear     = year(times.currentTime);

  rtcds1302.setDS1302Time(times.currentSeconds, times.currentMinutes, times.currentHours, 2, times.currentDay, times.currentMonth, times.currentYear);
}

void getConfig(){

  display.clearDisplay();
  displayOLED(1, SSD1306_WHITE, 10, 10, "Request config from");
  displayOLED(2, SSD1306_WHITE, 10, 30, "Webserver");

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
        displayOLED(1, SSD1306_WHITE, 10 + failsReqCount, 50, ".");
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

          config.session_ID      = doc[0]["session_ID"].as<const char*>();
          config.username        = doc[0]["username"].as<const char*>();
          config.mode            = doc[0]["mode"].as<const char*>();
          config.patternType     = doc[0]["pattern_type"].as<const char*>();
          config.repetition      = doc[0]["set_repetition"];
          config.status          = doc[0]["status"].as<const char*>();
          config.patternJSON     = doc[0]["pattern"].as<const char*>();
          config.controlBoxDelay = doc[0]["controlbox_delay_ms"];

          const char* patternJSON = doc[0]["pattern"].as<const char*>();
          char* patternParse = strtok((char*)patternJSON, ",");
          
          while (patternParse!= NULL) {
            config.patternArray.push_back(atoi(patternParse));
            patternParse = strtok(NULL, ",");
          }
        }
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        }
        http.end();

        display.clearDisplay();
        displayOLED(2, SSD1306_WHITE, 10, 30, "Config");

        for (int j = 0; j < 2; j++) {
          digitalWrite(buzzerPin, HIGH);   
          delay(500);                      
          digitalWrite(buzzerPin, LOW);    
          delay(500); 
        }

        char repetitionChar[10];

        display.clearDisplay();

        char combinedText[50];
        sprintf(combinedText, "session  : %s", config.session_ID.c_str());
        displayOLED(1, SSD1306_WHITE, 10, 0, combinedText);

        sprintf(combinedText, "user     : %s", config.username.c_str());
        displayOLED(1, SSD1306_WHITE, 10, 10, combinedText);

        sprintf(combinedText, "mode     : %s", config.mode.c_str());
        displayOLED(1, SSD1306_WHITE, 10, 20, combinedText);

        sprintf(combinedText, "pattern: %s", config.patternJSON.c_str());
        displayOLED(1, SSD1306_WHITE, 10, 30, combinedText);

        sprintf(combinedText, "type   : %s", config.patternType.c_str());
        displayOLED(1, SSD1306_WHITE, 10, 40, combinedText);

        sprintf(combinedText, "reps   : %s", itoa(config.repetition, repetitionChar, 10));
        displayOLED(1, SSD1306_WHITE, 10, 50, combinedText);
        delay(5000);

        if(config.patternType == "random"){
          randNum = random(0, config.patternArray.size());
          nodeNum = config.patternArray[randNum];
          Serial.println("end getConfig");
        } else {
          nodeNum = config.patternArray[0];
        }

  }
}

void nRF24Config() {
  
  radio.begin();
  radio.setDataRate(RF24_2MBPS);
  radio.setChannel(124);
  radio.openWritingPipe(address2);
  radio.openReadingPipe(1, address1);
  radio.setPALevel(RF24_PA_MIN);
  randomSeed(analogRead(A0));

  display.clearDisplay();
  displayOLED(2, SSD1306_WHITE, 10, 30, "Prepare");

  for (int j = 0; j < 5; j++) {
    digitalWrite(buzzerPin, HIGH);   
    delay(500);                      
    digitalWrite(buzzerPin, LOW);    
    delay(500);                      
  }
  display.clearDisplay();
  Serial.println("end nRF24Config");
}

void setup() {

  Serial.begin(9600);

  connectWiFi();

  updateRTC();

  getConfig();

  nRF24Config();
}

void loop() {
  int repsCount;
  for(repsCount = 0; repsCount < config.repetition*config.patternArray.size(); repsCount++) {
      char nodeNumChar[10]; 
      sprintf(nodeNumChar, "%d", nodeNum);

      radio.stopListening();

      if(radio.write(nodeNumChar, strlen(nodeNumChar) + 1)){
        displayOLED(1, SSD1306_WHITE, 10, 25,  "Active Node");
        displayOLED(2, SSD1306_WHITE, 10, 40,  "Node");
        displayOLED(2, SSD1306_WHITE, 70, 40,  nodeNumChar);
        display.println(nodeNum);
      }
      else{
        Serial.print("failed send response to node");
        Serial.println(nodeNum);
      }

      while(true){
        radio.startListening();
        if(radio.available()){
          char message[32] = "";
          radio.read(&message, sizeof(message));
          Serial.print("receive message from node");
          Serial.println(message);

          saveData(message, repsCount);

          if(strcmp(nodeNumChar, message) == 0){
            Serial.println("correct response");
            while (nodeNum == atoi(message)){
              if(config.patternType == "random"){
              randNum = random(0, config.patternArray.size()); 
              nodeNum = config.patternArray[randNum];
              } else {
                nodeNum = config.patternArray[(repsCount + 1) % config.patternArray.size()];
                char nodeNumChar[10]; 

              }

              Serial.print("next node: ");
              Serial.println(nodeNum);
            }
            break;
          }
        }
        Serial.println("waiting for node");
        delay(config.controlBoxDelay);
      }
  }
    for (int j = 0; j < 3; j++) {
    digitalWrite(buzzerPin, HIGH);   
    delay(500);                      
    digitalWrite(buzzerPin, LOW);    
    delay(500);                 
  }


  for (int arrayNum = 0; arrayNum < repsCount; arrayNum++) {
    sendDataToServer(data[arrayNum]);

    display.clearDisplay();
    displayOLED(2, SSD1306_WHITE, 10, 10, "Upload");
    displayOLED(2, SSD1306_WHITE, 10, 30, "Data");
  

    char currentReps[10];
    char currentRepsString[10];

    sprintf(currentReps, "%d", repsCount);
    sprintf(currentRepsString, "%d", (arrayNum + 1));
    displayOLED(2, SSD1306_WHITE, 65, 30, currentRepsString);
    displayOLED(2, SSD1306_WHITE, 90, 30, "/");
    displayOLED(2, SSD1306_WHITE, 100, 30, currentReps);
    delay(10);

    display.clearDisplay();
    }

  for (int j = 0; j < 4; j++) {
    digitalWrite(buzzerPin, HIGH);   
    delay(500);                      
    digitalWrite(buzzerPin, LOW);    
    delay(500);
  }
  display.clearDisplay();

  delay(10000000000000);

  
}

void saveData(char* message, int repsCount) {

  rtcds1302.updateTime();
  
  unsigned long currentMillis = millis();
  times.currentMillis = currentMillis % 1000;
  times.timestamp     = String(String(rtcds1302.year)
                        + "-" + String(rtcds1302.month) 
                        + "-" + rtcds1302.dayofmonth) 
                        + " " + String(rtcds1302.hours) 
                        + ":" + String(rtcds1302.minutes)
                        + ":" + String(rtcds1302.seconds)
                        + "." + String(times.currentMillis);

  if(repsCount == 0){
    times.prevTimestamp = times.timestamp;
    times.prevHours     = rtcds1302.hours;
    times.prevMinutes   = rtcds1302.minutes;
    times.prevSeconds   = rtcds1302.seconds;
    times.prevMillis    = times.currentMillis;
  }

  times.timeDiffSec     = floor((((rtcds1302.hours-times.prevHours)*3600 + (rtcds1302.minutes-times.prevMinutes)*60 + (rtcds1302.seconds-times.prevSeconds))*1000 + (times.currentMillis-times.prevMillis))/1000);
  
  if(times.prevMillis <= times.currentMillis){
    times.timeDiffMillis  = times.currentMillis - times.prevMillis;
  } else {
    times.timeDiffMillis  = 1000 - (times.prevMillis - times.currentMillis);
  };


  data[repsCount].session_ID     = config.session_ID;
  data[repsCount].username       = config.username;
  data[repsCount].runNum         = repsCount+1;
  data[repsCount].repsNum        = ceil(static_cast<double>(repsCount + 1) / config.patternArray.size());
  data[repsCount].nodeID         = message;
  data[repsCount].prevTimestamp  = times.prevTimestamp;
  data[repsCount].timestamp      = times.timestamp;
  data[repsCount].timeDiffSec    = times.timeDiffSec;
  data[repsCount].timeDiffMillis = times.timeDiffMillis;

  times.prevTimestamp = times.timestamp;
  times.prevHours     = rtcds1302.hours;
  times.prevMinutes   = rtcds1302.minutes;
  times.prevSeconds   = rtcds1302.seconds;
  times.prevMillis    = times.currentMillis;

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10);
  display.println(times.timeDiffSec);
  display.setCursor(15, 10);
  display.println(", ");
  display.setCursor(20, 10);
  display.println(times.timeDiffMillis);
  display.display();
}

void sendDataToServer(Data data) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    WiFiClient client;

    http.begin(client, serverUploadDataurl);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    
    String httpRequestData = "session_ID=" + data.session_ID +
                             "&username=" + data.username +
                             "&run_number=" + String(data.runNum) +
                             "&reps_number=" + String(data.repsNum) +
                             "&node_ID=" + data.nodeID +
                             "&node_prev_timestamp=" + data.prevTimestamp +
                             "&node_timestamp=" + data.timestamp +
                             "&time_diff_second=" + String(data.timeDiffSec) +
                             "&time_diff_millis=" + String(data.timeDiffMillis);
                             
    int httpResponseCode = http.POST(httpRequestData);
    
    if (httpResponseCode > 0) {
      Serial.println("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String response = http.getString();
      Serial.println(response);
    } else {
      Serial.println("Error code: " + String(httpResponseCode));
    }

    http.end();
  }
}