#include <MemoryFree.h>
#define DEBUG_MODE false

#if DEBUG_MODE
  #define debug(log) Serial.print(log);
  #define debugMemory() Serial.print(F("Left Memory: ")); Serial.println(freeMemory());
#else
  #define debug(log)
  #define debugMemory()
#endif

#include <LedControl.h>
#include <Array.h>
#include <IRremote.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>
#include <PlayMelody.h>

#define SPEAKER1_PIN 3

const int SERIAL_BAUD_RATE = 9600;
const int NODEMCU1_BAUD_RATE = 9600;
SoftwareSerial NodeMCU1(5, 6);

const int JSON_MEMORY = 256;

const int DEFAULT_INTENSITY = 0;
const int LC1_NUM_OF_LED = 4;
const int LC_PIN[] = {12, 11, 10};
LedControl ledControl1 = LedControl(LC_PIN[0], LC_PIN[1], LC_PIN[2], LC1_NUM_OF_LED);

void setupLedControl1(){
  for(int i = 0 ; i < LC1_NUM_OF_LED; i++){
    ledControl1.shutdown(i, false);
    ledControl1.setIntensity(i, DEFAULT_INTENSITY);
    ledControl1.clearDisplay(i);
  }
}

void setupSerial() {
  Serial.begin(SERIAL_BAUD_RATE);
  NodeMCU1.begin(NODEMCU1_BAUD_RATE);
}

void setup() {
  setupSerial();
  setupLedControl1();
}

Array<boolean, 32> parseBAInteger4x8(unsigned long bAI){
  Array<boolean, 32> bA;
  int m;
  for(int i = 0; i < 32; i++){
    m = bAI % 2;
    bAI -= m;
    bAI /= 2;  
    bA.push_back(m);
  }
  Array<boolean, 32> bAR;
  for(int i = 0; i < 32; i++){
    bAR[i] = bA[32 - (i + 1)];
  }
  return bAR;
}

// Convert 4x8 Boolean Array to Row Byte Array
Array<byte, 4> convert4x8BA2RBA(Array<boolean, 32> bA){
  Array<byte, 4> rBA;
  for(int i = 0; i < 4; i++){
    byte rB = 0;
    int ix = i * 8;
    for(int j = 0; j < 8; j++){
      rB += bA[ix + j] << 8 - (j + 1);
    }
    rBA.push_back(rB);
  }
  return rBA;
}

Array<byte, 4> convert4x8BAI2RBA(unsigned long bAI){
  return convert4x8BA2RBA(parseBAInteger4x8(bAI));
}


void processLedControl1(DynamicJsonDocument& doc){
  String controlString = doc[F("control")];
  if (String(F("LED")).equals(controlString)) {
    JsonArray ledDataArray = doc[F("data")];
    for(int i = 0; i < ledDataArray.size(); i++){
      JsonArray ledData = ledDataArray[i];
      for(int j = 0; j < ledData.size(); j++){
        Array<byte, 4> rBA = convert4x8BAI2RBA(ledData[j]);
        for(int k = 0; k < rBA.size(); k++){
          ledControl1.setRow(i, j * 4 + k, rBA[k]);
        }
      }
    }
  } else if (String(F("Intensity")).equals(controlString)) {
    JsonArray intensityArray = doc[F("data")];
    for(int i = 0; i < intensityArray.size(); i++){
      int ledData = intensityArray[i];
      ledControl1.setIntensity(i, ledData);
    }
  }
}


#define MAXIMUM_SEND_RAW_LENGTH 30
IRsend irsend;
void processIRSend1(DynamicJsonDocument& doc){
  debugMemory();
  String func = doc[F("func")];
  
  if (String(F("sendLG")).equals(func)) {
    irsend.sendLG(doc[F("data")], doc[F("nbits")]);
  }else {
    debug(F("Unknown IRSend\n"));
  }
  debugMemory();
}

/*
 * Full Version of processIRSend1
#define MAXIMUM_SEND_RAW_LENGTH 30
IRsend irsend;
void processIRSend1(DynamicJsonDocument& doc){
  debugMemory();
  String func = doc[F("func")];
  
  if (String(F("custom_delay_usec")).equals(func)) {
    irsend.custom_delay_usec(doc[F("uSecs")]);
  } else if (String(F("enableIROut")).equals(func)) {
    irsend.enableIROut(doc[F("khz")]);
  } else if (String(F("mark")).equals(func)) {
    irsend.mark(doc[F("usec")]);
  } else if (String(F("space")).equals(func)) {
    irsend.space(doc[F("usec")]);
  } else if (String(F("sendRaw")).equals(func)) {
    JsonArray rawBuf = doc[F("buf")].as<JsonArray>();
    unsigned int buf[MAXIMUM_SEND_RAW_LENGTH] = {0};
    int i = 0;
    for(JsonVariant v : rawBuf) {
        buf[i++] = v.as<unsigned int>();
    }
    String lens = doc[F("len")];
    String hzs = doc[F("hz")];
    irsend.sendRaw(buf, doc[F("len")], doc[F("hz")]);
  } else if (String(F("sendRC5")).equals(func)) {
    irsend.sendRC5(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendRC6")).equals(func)) {
    irsend.sendRC6(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendNEC")).equals(func)) {
    irsend.sendNEC(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendSony")).equals(func)) {
    irsend.sendSony(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendPanasonic")).equals(func)) {
    irsend.sendPanasonic(doc[F("address")], doc[F("data")]);
  } else if (String(F("sendJVC")).equals(func)) {
    irsend.sendJVC(doc[F("data")], doc[F("nbits")], doc[F("repeat")]);
  } else if (String(F("sendSAMSUNG")).equals(func)) {
    irsend.sendSAMSUNG(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendWhynter")).equals(func)) {
    irsend.sendWhynter(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendAiwaRCT501")).equals(func)) {
    irsend.sendAiwaRCT501(doc[F("code")]);
  } else if (String(F("sendLG")).equals(func)) {
    irsend.sendLG(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendDISH")).equals(func)) {
    irsend.sendDISH(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendSharpRaw")).equals(func)) {
    irsend.sendSharpRaw(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendSharp")).equals(func)) {
    irsend.sendSharp(doc[F("address")], doc[F("command")]);
  } else if (String(F("sendDenon")).equals(func)) {
    irsend.sendDenon(doc[F("data")], doc[F("nbits")]);
  } else if (String(F("sendLegoPowerFunctions")).equals(func)) {
    irsend.sendLegoPowerFunctions(doc[F("data")], doc[F("repeat")]);
  } else {
    debug(F("Unknown IRSend\n"));
  }
  debugMemory();
}
*/

unsigned long LAST_UPDATE_STATUS_TIME = 0;
unsigned long UPDATE_STATUS_DELAY = 5 * 1000;
void handleNodeMCUSerial(DynamicJsonDocument& doc) {
  debugMemory();
  NodeMCU1.println(F("{\"type\":\"UpdateStatus\",\"isBusy\":true}"));
  String type = doc[F("type")];
  if(String(F("PlayMelody1")).equals(String(type))){
    const char* playData = doc[F("playData")];
    PlayMelody(SPEAKER1_PIN, const_cast<char*>(playData));
  }else if(String(F("IRSend1")).equals(String(type))){
    processIRSend1(doc);
  }else if(String(F("LedControl1")).equals(String(type))){
    processLedControl1(doc);
  }
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
    if(millis() - LAST_UPDATE_STATUS_TIME > UPDATE_STATUS_DELAY){
      NodeMCU1.println(F("{\"type\":\"UpdateStatus\",\"isBusy\":false}"));
      debug(F("UpdateStatus to NodeMCU1\n"));
      LAST_UPDATE_STATUS_TIME = millis();
    }
  }
}

void loop() {
  runNodeMCUSerial();
}
