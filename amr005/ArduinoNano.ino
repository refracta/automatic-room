#include <MemoryFree.h>
#define DEBUG_MODE false

#if DEBUG_MODE
#define debug(log) Serial.print(log);
#define debugMemory() Serial.print(F("Left Memory: ")); Serial.println(freeMemory());
#else
#define debug(log)
#define debugMemory()
#endif

#include <SoftwareSerial.h>
#include <ArduinoJson.h>

#define RELAY_PIN 2
#define BUTTON_PIN 12

const int SERIAL_BAUD_RATE = 9600;
const int NODEMCU1_BAUD_RATE = 9600;
SoftwareSerial NodeMCU1(5, 6);

const int JSON_MEMORY = 256;

void setupSerial() {
  Serial.begin(SERIAL_BAUD_RATE);
  NodeMCU1.begin(NODEMCU1_BAUD_RATE);
}

// const unsigned int NUMBER_OF_MODES = 2;
__attribute__((section(".noinit"))) unsigned int mode;

void setupMode(){
    // if (++mode >= NUMBER_OF_MODES) mode = 0;
}

boolean LIGHT_STATUS = true;

void setupRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  if(mode == 0){
    digitalWrite(RELAY_PIN, HIGH);
    LIGHT_STATUS = true;
  } else {
    digitalWrite(RELAY_PIN, LOW);
    LIGHT_STATUS = false;
  }
  mode = LIGHT_STATUS ? 1 : 0;
}

void setup() {
  setupMode();
  setupSerial();
  setupRelay();
}

unsigned long LAST_UPDATE_STATUS_TIME = 0;
unsigned long UPDATE_STATUS_DELAY = 5 * 1000;

void handleNodeMCUSerial(DynamicJsonDocument& doc) {
  debugMemory();
  NodeMCU1.println(F("{\"type\":\"UpdateStatus\",\"isBusy\":true}"));
  String type = doc[F("type")];
  if (String(F("LightOn")).equals(String(type))) {
    digitalWrite(RELAY_PIN, HIGH);
    LIGHT_STATUS = true;
  } else if (String(F("LightOff")).equals(String(type))) {
    digitalWrite(RELAY_PIN, LOW);
    LIGHT_STATUS = false;
  } else if (String(F("LightToggle")).equals(String(type))) {
    if (LIGHT_STATUS) {
      digitalWrite(RELAY_PIN, LOW);
    } else {
      digitalWrite(RELAY_PIN, HIGH);
    }
    LIGHT_STATUS = !LIGHT_STATUS;
  }
  mode = LIGHT_STATUS ? 1 : 0;
  NodeMCU1.println(F("{\"type\":\"UpdateStatus\",\"isBusy\":false}"));
  LAST_UPDATE_STATUS_TIME = millis();
  debugMemory();
}

void runNodeMCUSerial() {
  if (NodeMCU1.available() > 0) {
    String json = NodeMCU1.readStringUntil('\n');
    debug(json);
    DynamicJsonDocument doc(JSON_MEMORY);
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
      debug(F("deserializeJson() failed: "));
      debug(error.c_str());
      debug(F("\n"));
    } else {
      handleNodeMCUSerial(doc);
    }
  } else {
    if (millis() - LAST_UPDATE_STATUS_TIME > UPDATE_STATUS_DELAY) {
      NodeMCU1.println(F("{\"type\":\"UpdateStatus\",\"isBusy\":false}"));
      debug(F("UpdateStatus to NodeMCU1\n"));
      LAST_UPDATE_STATUS_TIME = millis();
    }
  }
}

void loop() {
  runNodeMCUSerial();
}
