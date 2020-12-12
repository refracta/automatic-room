#include <MemoryFree.h>
#define DEBUG_MODE true

#if DEBUG_MODE
  #define debug(log) Serial.print(log);
  #define debugMemory() Serial.print(F("Left Memory: ")); Serial.println(freeMemory());
#else
  #define debug(log)
  #define debugMemory()
#endif

#include <ESP8266.h>
#include <SoftwareSerial.h>
#include <Array.h>
#include <PlayMelody.h>
#define SPEAKER1_PIN 6

#define SSID ""
#define PASS ""


SoftwareSerial esp8266Serial = SoftwareSerial(2, 3);
ESP8266 wifi = ESP8266(esp8266Serial);

String getStatus(bool status)
{
    if (status)
        return F("OK");
 
    return F("KO");
}
 
String getStatus(ESP8266CommandStatus status)
{
    switch (status) {
    case ESP8266_COMMAND_INVALID:
        return F("INVALID");
        break;
 
    case ESP8266_COMMAND_TIMEOUT:
        return F("TIMEOUT");
        break;
 
    case ESP8266_COMMAND_OK:
        return F("OK");
        break;
 
    case ESP8266_COMMAND_NO_CHANGE:
        return F("NO CHANGE");
        break;
 
    case ESP8266_COMMAND_ERROR:
        return F("ERROR");
        break;
 
    case ESP8266_COMMAND_NO_LINK:
        return F("NO LINK");
        break;
 
    case ESP8266_COMMAND_TOO_LONG:
        return F("TOO LONG");
        break;
 
    case ESP8266_COMMAND_FAIL:
        return F("FAIL");
        break;
 
    default:
        return F("UNKNOWN COMMAND STATUS");
        break;
    }
}
 
String getRole(ESP8266Role role)
{
    switch (role) {
    case ESP8266_ROLE_CLIENT:
        return F("CLIENT");
        break;
 
    case ESP8266_ROLE_SERVER:
        return F("SERVER");
        break;
 
    default:
        return F("UNKNOWN ROLE");
        break;
    }
}
 
String getProtocol(ESP8266Protocol protocol)
{
    switch (protocol) {
    case ESP8266_PROTOCOL_TCP:
        return F("TCP");
        break;
 
    case ESP8266_PROTOCOL_UDP:
        return F("UDP");
        break;
 
    default:
        return F("UNKNOWN PROTOCOL");
        break;
    }
}




#define SERIAL_BAUD_RATE 9600
#define ESP8266_SERIAL_BAUD_RATE 9600
void setupSerial(){
  Serial.begin(SERIAL_BAUD_RATE);
  esp8266Serial.begin(ESP8266_SERIAL_BAUD_RATE);
}

const int DEFAULT_WIFI_TIMEOUT = 5 * 1000;
void setupWiFi(){
    debug(F("setupWiFi\n"));
    wifi.begin();
    wifi.setTimeout(DEFAULT_WIFI_TIMEOUT);
    debug(F("Restart WiFI: "));
    String wifiRestart = getStatus(wifi.restart());
    debug(wifiRestart);
    debug(F("\n"))
    debug(F("Join AP: "));
    String joinAP = getStatus(wifi.joinAP(SSID, PASS));
    debug(joinAP);
    debug("\n");
    debug(F("Get AP: "));
    String getAP = getStatus(wifi.getAP(SSID));
    debug(getAP);
    debug("\n");
}

#define BUTTON1_PIN 8
#define BUTTON2_PIN 9
#define BUTTON3_PIN 10
#define BUTTON4_PIN 11
#define BUTTON5_PIN 12

void setupButton(){
  pinMode(BUTTON1_PIN, INPUT_PULLUP);
  pinMode(BUTTON2_PIN, INPUT_PULLUP);
  pinMode(BUTTON3_PIN, INPUT_PULLUP);
  pinMode(BUTTON4_PIN, INPUT_PULLUP);  
  pinMode(BUTTON5_PIN, INPUT_PULLUP);
}

#define LED_PIN1 13
void setupLED(){
  pinMode(LED_PIN1, OUTPUT);
  digitalWrite(LED_PIN1, HIGH);
}

#define QUEUE_SIZE 5
Array<String, QUEUE_SIZE> urlQueue;

#define MAX_REDIRECTION 3
#define MAX_HOST_LENGTH 100
void httpGet(String url){
  digitalWrite(LED_PIN1, LOW);
  debug(F("URL: "));
  debug(url);
  debug(F("\n"));
  debug(F("WiFi Connect: "));
  String host = parseHost(url);
  debug(F("Host: "));
  debug(host);
  debug(F("\n"));
  
  String path = parsePath(url);
  debug(F("Path: "));
  debug(path);
  debug(F("\n"));
  if(String("").equals(url) || String("").equals(host)){
    debug(F("URL Error"));
    debug(F("\n"));
    return;
  }
  
  int port = parsePort(url);
  debug(F("Port: "));
  debug(String(port));
  debug(F("\n"));
  char hostCharArray[MAX_HOST_LENGTH] = {0};
  host.toCharArray(hostCharArray, host.length() + 1);
  String wifiConnect = getStatus(wifi.connect(ESP8266_PROTOCOL_TCP, hostCharArray, port));

  debug(F("WifiConnect: "));
  debug(wifiConnect);
  debug(F("\n"));
  if(String(F("UNKNOWN COMMAND STATUS")).equals(wifiConnect) || String(F("TIMEOUT")).equals(wifiConnect) || String(F("ERROR")).equals(wifiConnect) || String(F("NO LINK")).equals(wifiConnect)){
    if(!urlQueue.full()){
      debug(F("<Re-Request>\n"));
      urlQueue.push_back(url);
      return;
    }else {
      debug(F("Queue Full Error\n"));
    }    
  }

  
  
  String request  = String(F("GET ")) + path + String(F(" HTTP/1.0\r\nHost: ")) + host + String(F("\r\n\r\n"));
  debug(F("Full Request: "));
  debug(request);
  debug(F("\n"));
  String wifiSend = getStatus(wifi.send(request));
  debug(F("WiFi Send: "));
  debug(wifiSend);
  debug(F("\n"));
  if(String(F("UNKNOWN COMMAND STATUS")).equals(wifiSend) || String(F("TIMEOUT")).equals(wifiSend) || String(F("ERROR")).equals(wifiSend) || String(F("NO LINK")).equals(wifiSend)){
    if(!urlQueue.full()){
      debug(F("<Re-Request>\n"));
      urlQueue.push_back(url);
      return;
    }else {
      debug(F("Queue Full Error\n"));
    }    
  }

  

}
String parseHost(String url){
  url = parseFullHost(url);
  int hostEnd = url.indexOf(":");
  if(hostEnd < 0){
    hostEnd = url.length();
  }
  return url.substring(0, hostEnd);
}

int parsePort(String url){
  String fullHost = parseFullHost(url);
  int portStart = fullHost.indexOf(":") + 1;
  if(portStart == 0){
    if(url.startsWith(F("http://"))){
      return 80;
    }else if(url.startsWith(F("https://"))){
      return 443;
    }
  } else {
    return fullHost.substring(portStart, url.length()).toInt();
  }
}

String parseFullHost(String url){
  int fullHostStart = url.indexOf(F("//")) + 2;
  url = url.substring(fullHostStart);
  int fullHostEnd = url.indexOf(F("/"));
  if(fullHostEnd < 0){
    fullHostEnd = url.length();
  }
  url = url.substring(0, fullHostEnd);
  url.trim();
  return url;
}

String parsePath(String url){
  int fullHostStart = url.indexOf(F("//")) + 2;
  url = url.substring(fullHostStart);

  int fullHostEnd = url.indexOf(F("/"));
  if(fullHostEnd < 0){
    return F("/");
  }
  return url.substring(fullHostEnd);
}

void setup()
{
  setupSerial();
  setupWiFi();
  setupButton();
  setupLED();
}


/*
String simpleURLDecode(String str)
{
   str.replace("%5B", "[");
   str.replace("%5D", "]");
   str.replace("%7B", "{");
   str.replace("%7D", "}");
   str.replace("%22", "\"");
   str.replace("%25", "%");
   return str;
}
*/


#define MAX_BUFFER_SIZE 400
#define HEADER_LOCATION String(F("Location: "))
void runWiFi(){
    //debugMemory();
    unsigned int id;
    int length;
    int totalRead;
    char buffer[MAX_BUFFER_SIZE] = {};
 
    if ((length = wifi.available()) > 0) {
      digitalWrite(LED_PIN1, LOW);
      id = wifi.getId();
      totalRead = wifi.read(buffer, MAX_BUFFER_SIZE);
      if (length > 0) {
        debug(F("Received "));
        debug(totalRead);
        debug(F("/"));
        debug(length);
        debug(F(" bytes from client\n"));
        
        String bufferString = buffer;
        debug(F("<BUFFER START>\n"))
        debug(bufferString);
        debug(F("\n<BUFFER END>\n"))
        debug(F("\n"));
        debugMemory();
        
        int bsLIx = bufferString.indexOf(HEADER_LOCATION);
        if(bsLIx > -1) {
          String url = bufferString.substring(bsLIx + HEADER_LOCATION.length(), bufferString.length());
          int locationEnd = url.indexOf(F("\n"));
          if(locationEnd < 0){
            locationEnd = url.length();
          }
          url = url.substring(0, locationEnd);
          url.trim();
          
          if(!urlQueue.full()){
            urlQueue.push_back(url);
          }else {
            debug(F("Queue Full Error\n"));
          }
          debug(F("Location Redirect: "));
          debug(url);
          debug("\n");
        }


        }
      }
}
void runEnqueue(){
  if(urlQueue.size() > 0){
    httpGet(urlQueue.front());
    urlQueue.remove(0);
  }
}

boolean lastStatusButton1;
boolean lastStatusButton2;
boolean lastStatusButton3;
boolean lastStatusButton4;
boolean lastStatusButton5;
void(* resetFunc) (void) = 0;
void runButton(){
  digitalWrite(LED_PIN1, HIGH);
  boolean triggerButton1 = digitalRead(BUTTON1_PIN) == LOW;
  boolean triggerButton2 = digitalRead(BUTTON2_PIN) == LOW;
  boolean triggerButton3 = digitalRead(BUTTON3_PIN) == LOW;
  boolean triggerButton4 = digitalRead(BUTTON4_PIN) == LOW;
  boolean triggerButton5 = digitalRead(BUTTON5_PIN) == LOW;
  
  if(triggerButton1 && !lastStatusButton1){
    debug(F("Button1\n"));
    PlayMelody(SPEAKER1_PIN, "O4T500C");
    httpGet(F("http://l.abstr.net/arduinobutton11"));
    return;
  }
  lastStatusButton1 = triggerButton1;
  
  if(triggerButton2 && !lastStatusButton2){
    debug(F("Button2\n"));
    PlayMelody(SPEAKER1_PIN, "O4T500D");
    httpGet(F("http://l.abstr.net/arduinobutton12"));
    return;
  }
  lastStatusButton2 = triggerButton2;
  
  if(triggerButton3 && !lastStatusButton3){
    debug(F("Button3\n"));
    PlayMelody(SPEAKER1_PIN, "O4T500E");
    httpGet(F("http://l.abstr.net/arduinobutton13"));
    return;
  }
  lastStatusButton3 = triggerButton3;
  
  if(triggerButton4 && !lastStatusButton4){
    debug(F("Button4\n"));
    PlayMelody(SPEAKER1_PIN, "O4T500F");
    httpGet(F("http://l.abstr.net/arduinobutton14"));
    return;
  }
  lastStatusButton4 = triggerButton4;
  
  if(triggerButton5 && !lastStatusButton5){
    debug(F("Button5\n"));
    PlayMelody(SPEAKER1_PIN, "O4T400G#D#O3A#G#");
    digitalWrite(LED_PIN1, LOW);
    resetFunc();
    return;
  }
  lastStatusButton5 = triggerButton5;
}
void loop()
{
  runWiFi();
  runButton();
  runEnqueue();
}
 
