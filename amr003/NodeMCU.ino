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
#include <Servo.h>

const String SSID = "";
const String SSID_PASSWORD = "";
const int WEB_SERVER_PORT = 80;
const int WIFI_INIT_DELAY = 500;
ESP8266WebServer server(WEB_SERVER_PORT);

const int JSON_MEMORY = 256;
const int SERIAL_BAUD_RATE = 115200;

int SG90_1_DEFAULT_ANGLE = 90;
int SG90_2_DEFAULT_ANGLE = 90;
const int SG90_1_PIN = D7;
const int SG90_2_PIN = D8;
Servo SG90_1; 
Servo SG90_2;

String getAngleInfo1(int beforeAngle, int afterAngle){
    String json;
    StaticJsonDocument<JSON_MEMORY> doc;
    doc[F("status")] = F("success");
    doc[F("beforeAngle")] = beforeAngle;
    doc[F("afterAngle")] = afterAngle;
    serializeJson(doc, json);
    return json;
}

String getAngleInfo2(){
    String json;
    StaticJsonDocument<JSON_MEMORY> doc;
    doc[F("status")] = F("success");
    doc[F("SG90_1")] = SG90_1.read();
    doc[F("SG90_2")] = SG90_2.read();
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
    if(String(F("Angle")).equals(type)) {
      return getAngleInfo2(); 
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
    if(String(F("SG90_1_WRITE")).equals(type)) {
      int beforeAngle = SG90_1.read();
      int afterAngle = doc["angle"];
      SG90_1.write(afterAngle);
      return getAngleInfo1(beforeAngle, afterAngle);
    } if(String(F("SG90_2_WRITE")).equals(type)) {
      int beforeAngle = SG90_2.read();
      int afterAngle = doc["angle"];
      SG90_2.write(afterAngle);
      return getAngleInfo1(beforeAngle, afterAngle);
    } if(String(F("SG90_1_WRITE_WITH_RELOCATE")).equals(type)) {
      int beforeAngle = SG90_1.read();
      int afterAngle = doc["angle"];
      int delayTime = doc["delay"];
      SG90_1.write(afterAngle);
      delay(delayTime);
      SG90_1.write(SG90_1_DEFAULT_ANGLE);
      return getAngleInfo1(beforeAngle, SG90_1_DEFAULT_ANGLE);
    } if(String(F("SG90_2_WRITE_WITH_RELOCATE")).equals(type)) {
      int beforeAngle = SG90_2.read();
      int afterAngle = doc["angle"];
      int delayTime = doc["delay"];
      SG90_2.write(afterAngle);
      delay(delayTime);
      SG90_2.write(SG90_2_DEFAULT_ANGLE);
      return getAngleInfo1(beforeAngle, SG90_2_DEFAULT_ANGLE);
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

void setupServo(){
  SG90_1.attach(SG90_1_PIN);
  SG90_2.attach(SG90_2_PIN);
  SG90_1.write(SG90_1_DEFAULT_ANGLE);
  SG90_2.write(SG90_2_DEFAULT_ANGLE);
}

void setup() {
  setupSerial();
  setupWIFi();
  setupServo();
}


void runWiFi() {
   server.handleClient();
}

void loop(){
  runWiFi();
}
