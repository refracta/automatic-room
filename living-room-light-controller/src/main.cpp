#define DEBUG_MODE true

#if DEBUG_MODE
#define debug(log) Serial.print(log)
#else
#define debug(log)
#endif

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJSON.h>

const char *AP_MODE_SSID = "LIGHT-CONTROLLER";

AsyncWebServer server(80);

enum ProcessStatus {
    LOGIC,
    CHANGE_AP_MODE,
};
ProcessStatus status = CHANGE_AP_MODE;

enum ServerMode {
    AP,
    STA
};

enum EEPROMSectorSize {
    S_SERVER_MODE = 1,
    S_SSID = 32 + 1,
    S_PASSWORD = 32 + 1,
};

enum EEPROMSectorIndex {
    I_SERVER_MODE = 0,
    I_SSID = I_SERVER_MODE + S_SERVER_MODE,
    I_PASSWORD = I_SSID + S_SSID,
};

void writeStringToEEPROM(int offset, const String &target) {
    byte length = target.length();
    EEPROM.write(offset, length);
    for (int i = 0; i < length; i++) {
        EEPROM.write(offset + 1 + i, target[i]);
    }
}

String readStringFromEEPROM(int offset) {
    int length = EEPROM.read(offset);
    char data[length + 1];
    for (int i = 0; i < length; i++) {
        data[i] = EEPROM.read(offset + 1 + i);
    }
    data[length] = '\0';
    return String(data);
}

void setupSerial() {
    Serial.begin(115200);
}

void setupEEPROM() {
    EEPROM.begin(4096);
}

void runServer() {
    debug("runServer()\n");
    WiFi.softAPdisconnect(true);
    String ssid = readStringFromEEPROM(I_SSID);
    String password = readStringFromEEPROM(I_PASSWORD);

    ServerMode mode = (ServerMode) EEPROM.read(I_SERVER_MODE);
    switch (mode) {
        case STA:
            debug("STA: ");
            debug(ssid);
            WiFi.mode(WIFI_STA);
            if (password.length() == 0) {
                WiFi.begin(ssid);
            } else {
                debug(", ");
                debug(password);
                debug("\n");
                WiFi.begin(ssid, password);
            }
            break;
        case AP:
        default:
            debug("AP\n");
            WiFi.mode(WIFI_AP_STA);

            WiFi.softAPConfig(IPAddress(192, 168, 0, 1),
                              IPAddress(192, 168, 0, 254),
                              IPAddress(255, 255, 255, 0));
            WiFi.softAP(AP_MODE_SSID);
            break;
    }
    server.begin();
    debug("Server on!\n");
}

#define LIGHT1_PIN D2
#define LIGHT2_PIN D3
bool light1Status = true;
bool light2Status = true;
const char successJson[] = R"rawliteral({"status":"success"})rawliteral";

String getLightInfo() {
    String json;
    StaticJsonDocument<256> doc;

    doc[F("light1")] = light1Status;
    doc[F("light2")] = light2Status;

    serializeJson(doc, json);
    return json;
}

void setupServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json;
        StaticJsonDocument<256> doc;
        doc[F("name")] = AP_MODE_SSID;
        serializeJson(doc, json);
        request->send_P(200, "application/json", json.c_str());
    });

    server.on("/setting", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("ap")) {
            EEPROM.write(I_SERVER_MODE, AP);
            EEPROM.commit();
            status = CHANGE_AP_MODE;
            request->send_P(200, "application/json", successJson);
        } else if (request->hasParam("ssid") && request->hasParam("password")) {
            writeStringToEEPROM(I_SSID, request->getParam("ssid")->value());
            writeStringToEEPROM(I_PASSWORD, request->getParam("password")->value());
            EEPROM.write(I_SERVER_MODE, STA);
            EEPROM.commit();
            status = CHANGE_AP_MODE;
            request->send_P(200, "application/json", successJson);
        } else {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/set", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("status") && request->hasParam("type")) {
            String status = request->getParam("status")->value();
            String type = request->getParam("type")->value();
            int pin = type == "light2" ? LIGHT2_PIN : LIGHT1_PIN;
            if (status == "on") {
                digitalWrite(pin, HIGH);
            } else {
                digitalWrite(pin, LOW);
            }
            request->send_P(200, "application/json", successJson);
        } else {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send_P(200, "application/json", getLightInfo().c_str());
    });
}

const int TIMEOUT_SECOND = 10;

void changeAPMode() {
    debug("changeAPMode()\n");
    runServer();
    ServerMode mode = (ServerMode) EEPROM.read(I_SERVER_MODE);
    if (mode == STA) {
        unsigned long waitStart = millis();
        while (WiFi.status() != WL_CONNECTED) {
            if (millis() - waitStart > TIMEOUT_SECOND * 1000) {
                EEPROM.write(I_SERVER_MODE, AP);
                EEPROM.commit();
                debug("Connection timeout\n");
                return;
            } else {
                yield();
            }
        }
    }
    status = LOGIC;
}

void setupLight() {
    pinMode(LIGHT1_PIN, OUTPUT);
    digitalWrite(LIGHT1_PIN, light1Status ? HIGH : LOW);
    pinMode(LIGHT2_PIN, OUTPUT);
    digitalWrite(LIGHT2_PIN, light2Status ? HIGH : LOW);
}

void setup() {
    setupSerial();
    setupEEPROM();
    setupLight();
    setupServer();
}

void loop() {
    if (status == CHANGE_AP_MODE) {
        changeAPMode();
    } else if (status == LOGIC) {

    }
}