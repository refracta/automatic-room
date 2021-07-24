#define DEBUG_MODE false

#if DEBUG_MODE
  #define debug(log) Serial.print(log);
  #define debugMemory()
#else
  #define debug(log)
  #define debugMemory()
#endif

#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>

const String SSID = "";
const String SSID_PASSWORD = "";
const int WEB_SERVER_PORT = 80;
const int WIFI_INIT_DELAY = 500;
ESP8266WebServer server(WEB_SERVER_PORT);

const int JSON_MEMORY = 256;
const int SERIAL_BAUD_RATE = 115200;

IRsend irsend(D8);
#define IR_LENGTH 51
const int IR_KHZ = 38;
const uint16_t IR_COMMAND_STOP[IR_LENGTH] = {4450,4600, 600,550, 550,550, 600,550, 600,550, 600,1650, 600,1650, 600,1650, 600,1700, 550,550, 600,550, 600,500, 600,550, 600,550, 600,500, 600,550, 600,550, 600,1650, 600,1650, 600,1650, 600,1700, 550,1700, 600,1650, 600,1700, 550,1700, 550};
const uint16_t IR_COMMAND_SET_FAN_SPEED[IR_LENGTH] = {4450,4550, 650,500, 600,500, 600,550, 600,550, 600,1650, 600,1700, 600,1650, 600,1700, 550,1700, 550,550, 600,550, 550,600, 550,550, 600,550, 550,600, 550,550, 600,550, 550,1700, 550,1700, 600,1700, 550,1700, 550,1700, 550,1700, 600,1700, 550};
const uint16_t IR_COMMAND_SELECT_TIME[IR_LENGTH] = {4450,4550, 600,550, 600,550, 600,550, 600,500, 600,1700, 600,1650, 600,1650, 600,1650, 600,1700, 550,1700, 550,550, 600,550, 600,550, 550,550, 600,550, 600,550, 550,600, 550,550, 600,1700, 550,1700, 550,1700, 550,1700, 600,1700, 550,1700, 550};
const uint16_t IR_COMMAND_CONTROL_DIRECTION[IR_LENGTH] = {4450,4550, 650,500, 600,550, 600,550, 600,500, 600,1700, 550,1700, 600,1650, 600,1650, 600,550, 600,550, 550,1700, 550,550, 600,550, 600,550, 550,550, 600,550, 600,1700, 550,1650, 600,600, 550,1700, 550,1700, 550,1700, 600,1700, 550,1700, 550}; 
const uint16_t IR_COMMAND_SET_FAN_MODE[IR_LENGTH] = {4450,4600, 600,550, 600,500, 650,500, 600,550, 600,1650, 600,1650, 600,1700, 550,1700, 600,550, 550,1700, 550,600, 550,550, 600,550, 550,600, 550,550, 600,550, 550,1700, 550,600, 550,1700, 550,1700, 600,1700, 550,1700, 550,1700, 550,1700, 550};

String handleRead(String json){
  DynamicJsonDocument doc(JSON_MEMORY);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    debug(F("deserializeJson() failed: "));
    debug(error.c_str());
    debug(F("\n"));
    return F("{\"status\":\"error\",\"reason\":\"deserializeJson() failed\"}");
  } else {
    String type = doc["type"];
    if(String(F("UNKNOWN")).equals(type)) {
      // return ; 
    } else {
      return F("{\"status\":\"error\",\"reason\":\"failed to find type\"}");
    }
  }
}


String handleRequest(String json){
  DynamicJsonDocument doc(JSON_MEMORY);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    debug(F("deserializeJson() failed: "));
    debug(error.c_str());
    debug(F("\n"));
    return F("{\"status\":\"error\",\"reason\":\"deserializeJson() failed\"}");
  } else {
    String type = doc["type"];
    if(String(F("IR_COMMAND_STOP")).equals(type)) {
    int numberOfTry = doc["numberOfTry"];
    for(int i = 0; i < numberOfTry; i++){
      irsend.sendRaw(IR_COMMAND_STOP, IR_LENGTH, IR_KHZ);
      delay(1000);
    }
    return F("{\"status\":\"success\"}");
    } else if(String(F("IR_COMMAND_SET_FAN_SPEED")).equals(type)) {
    int numberOfTry = doc["numberOfTry"];
    for(int i = 0; i < numberOfTry; i++){
      irsend.sendRaw(IR_COMMAND_SET_FAN_SPEED, IR_LENGTH, IR_KHZ);
      delay(1000);
    }
    return F("{\"status\":\"success\"}");
    } else if(String(F("IR_COMMAND_SELECT_TIME")).equals(type)) {
    int numberOfTry = doc["numberOfTry"];
    for(int i = 0; i < numberOfTry; i++){
      irsend.sendRaw(IR_COMMAND_SELECT_TIME, IR_LENGTH, IR_KHZ);
      delay(1000);
    }
    return F("{\"status\":\"success\"}");
    } else if(String(F("IR_COMMAND_CONTROL_DIRECTION")).equals(type)) {
    int numberOfTry = doc["numberOfTry"];
    for(int i = 0; i < numberOfTry; i++){
      irsend.sendRaw(IR_COMMAND_CONTROL_DIRECTION, IR_LENGTH, IR_KHZ);
      delay(1000);
    }
    return F("{\"status\":\"success\"}");
    } else if(String(F("IR_COMMAND_SET_FAN_MODE")).equals(type)) {
    int numberOfTry = doc["numberOfTry"];
    for(int i = 0; i < numberOfTry; i++){
      irsend.sendRaw(IR_COMMAND_SET_FAN_MODE, IR_LENGTH, IR_KHZ);
      delay(1000);
    }
    return F("{\"status\":\"success\"}");
    } else {
      return F("{\"status\":\"error\",\"reason\":\"failed to find type\"}");
    }
  }
}




void setupSerial() {
  Serial.begin(SERIAL_BAUD_RATE);
}

void setupWIFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(SSID, SSID_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_INIT_DELAY);
    debug(F("."));
  }
  
  debug(F("\n"));
  debug(F("Connecting to "));
  debug(SSID + F("\n"));
  debug(F("IP address: "));
  debug(WiFi.localIP());
  debug(F("\n"));

  server.on(F("/read"), [](){
    String data = server.arg(F("data"));
    debug(String(F("/read: ")) + data + F("\n"));
    server.send(200, F("text/json"), handleRead(data));
  });
  
  server.on(F("/request"), [](){
    String data = server.arg("data");
    debug(String(F("/request: ")) + data + "\n");
    server.send(200, F("text/json"), handleRequest(data));
  });
  
  server.onNotFound([](){
    server.send(200, F("text/json"), F("{\"status\":\"error\",\"reason\":\"invalid path was requested\"}"));
  });
  
  server.begin();
  debug(F("Server Started!\n"));
}

void setupIR(){
  irsend.begin();
}

void setup() {
  setupSerial();
  setupWIFi();
  setupIR();
}

void runWiFi() {
   server.handleClient();
}

void loop(){
  runWiFi();
}
