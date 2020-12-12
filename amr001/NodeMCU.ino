#define DEBUG_MODE false

#if DEBUG_MODE
  #define debug(log) Serial.print(log);
  #define debugMemory()
#else
  #define debug(log)
  #define debugMemory()
#endif

#include <Array.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>

#define DHT11_1_PIN D1

const String SSID = "";
const String SSID_PASSWORD = "";
const int WEB_SERVER_PORT = 80;
const int WIFI_INIT_DELAY = 500;
ESP8266WebServer server(WEB_SERVER_PORT);

const int SERIAL_BAUD_RATE = 115200;
const int ARDUINO1_BAUD_RATE = 9600;
SoftwareSerial Arduino1(D6, D5);
boolean isArduino1Busy = false;
boolean lockArduino1Dequeue = false;

const int JSON_MEMORY = 256;

const int QUEUE_SIZE = 30;
Array<String, QUEUE_SIZE> arduino1Queue;

DHT dht11_1(DHT11_1_PIN, DHT11);
void setupDHT11_1(){
  dht11_1.begin();
  getDHT11_1Info();
}

void setupSerial() {
  Serial.begin(SERIAL_BAUD_RATE);
  Arduino1.begin(ARDUINO1_BAUD_RATE);
}

String getDHT11_1Info(){
  String json;
  StaticJsonDocument<JSON_MEMORY> doc;
  float h = dht11_1.readHumidity();
  float t = dht11_1.readTemperature();
  float f = dht11_1.readTemperature(true);
  if (isnan(h) || isnan(t) || isnan(f)) {
    doc[F("status")] = F("error");
    doc[F("reason")] = F("initialization does not complete");
    serializeJson(doc, json);
    return json;
  }else{
    doc[F("status")] = F("success");
  }
  float hif = dht11_1.computeHeatIndex(f, h);
  float hic = dht11_1.computeHeatIndex(t, h, false);
  doc[F("h")] = h;
  doc[F("t")] = t;
  doc[F("F")] = f;
  doc[F("hif")] = hif;
  doc[F("hic")] = hic;

  serializeJson(doc, json);
  return json;
}


String getArduino1QueueStatusInfo(){
    String json;
    StaticJsonDocument<JSON_MEMORY> doc;
    doc[F("status")] = F("success");
    doc[F("enqueued")] = arduino1Queue.size();
    doc[F("queueSize")] = QUEUE_SIZE;
    serializeJson(doc, json);
    return json;
}

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
    if(String(F("DHT11-1")).equals(type)) {
      return getDHT11_1Info(); 
    } else if (String(F("Arduino1QueueStatus")).equals(type)) {
      return getArduino1QueueStatusInfo(); 
    } else {
      return F("{\"status\":\"error\",\"reason\":\"failed to find type\"}");
    }
  }
}

String handleRequest(String json){
  if(!arduino1Queue.full()){
    debug(String(F("[Arduino1 Request Enqueue] Current Size: ")) + String(arduino1Queue.size()) + String(F(" +1")) + String(F("\n")));
    arduino1Queue.push_back(json);
    return getArduino1QueueStatusInfo();
  }else{
    debug(F("Arduino1 Request Rejected!\n"));
    return F("{\"status\":\"error\",\"reason\":\"queue is full\"}");
  }
}

String handleSystem(String json){
  DynamicJsonDocument doc(JSON_MEMORY);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    debug(F("deserializeJson() failed: "));
    debug(error.c_str());
    debug(F("\n"));
    return F("{\"status\":\"error\",\"reason\":\"deserializeJson() failed\"}");
  } else {
    String type = doc["type"];
    if (String(F("Arduino1QueueClear")).equals(type)) {
       arduino1Queue.clear();
       isArduino1Busy = false;
       lockArduino1Dequeue = false;
       return F("{\"status\":\"success\",\"reason\":\"The queue has been cleared.\"}");
    } else {
      return F("{\"status\":\"error\",\"reason\":\"failed to find type\"}");
    }
  }
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

   server.on(F("/system"), [](){
    String data = server.arg("data");
    debug(String(F("/system: ")) + data + "\n");
    server.send(200, F("text/json"), handleSystem(data));
  });
  
  
  server.onNotFound([](){
    server.send(200, F("text/json"), F("{\"status\":\"error\",\"reason\":\"invalid path was requested\"}"));
  });
  
  server.begin();
  debug(F("Server Started!\n"));
}

void setup() {
  setupDHT11_1();
  setupSerial();
  setupWIFi();
}

void handleArduino1Serial(DynamicJsonDocument& doc){
  String type = doc[F("type")];
  if(String(F("UpdateStatus")).equals(String(type))){
    isArduino1Busy = doc[F("isBusy")];
    if(!isArduino1Busy){
      lockArduino1Dequeue = false;
    }
    debug(String(F("Arduino1 Busy Update Status: ")) + (isArduino1Busy ? String(F("busy")) : String(F("not busy"))) + F("\n"));
  }
}

/*
const unsigned long FORCE_STATUS_REQUEST_WAIT = 3 * 1000;
const int FSR_MIN_ENQUEUED = QUEUE_SIZE;
unsigned long LAST_DEQUEUE_TIME = 0;
*/
void runSerial() {
  if (Serial.available() > 0) {
    String receive = Serial.readStringUntil('\n');
    debug(receive + String(F("\n")));
    Arduino1.println(receive);
    debug(F("Send OK!\n"));
  }
  if (Arduino1.available() > 0) {
    String json = Arduino1.readStringUntil('\n');
    DynamicJsonDocument doc(JSON_MEMORY);
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      debug(F("deserializeJson() failed: "));
      debug(error.c_str());
      debug(F("\n"));
    } else {
      handleArduino1Serial(doc);
    }
  }
  if(!isArduino1Busy && !arduino1Queue.empty() && !lockArduino1Dequeue){
    debug(String(F("[Arduino1 Request Dequeue] Current Size: ")) + String(arduino1Queue.size()) + String(F(" -1")) + String(F("\n")));
    Arduino1.println(arduino1Queue.front());
    arduino1Queue.remove(0);
    lockArduino1Dequeue = true;
    // LAST_DEQUEUE_TIME = millis();
  }/* else {
    if(arduino1Queue.size() >= FSR_MIN_ENQUEUED && millis() - LAST_DEQUEUE_TIME > FORCE_STATUS_REQUEST_WAIT){
      Arduino1.println(F("{\"type\":\"ForceStatusRequest\"}"));
    }
  }*/
}

void runWiFi() {
   server.handleClient();
}

void loop(){
  runSerial();
  runWiFi();
}
