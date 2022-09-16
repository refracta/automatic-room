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
#include <IRrecv.h>
#include <IRremoteESP8266.h>
#include "IRsend.h"
#include "IRutils.h"

const char *AP_MODE_SSID = "IR-DUMPER";

AsyncWebServer server(80);

enum ProcessStatus {
    LOGIC,
    CHANGE_AP_MODE,
    PLAY_DUMP,
    PLAY
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
    S_DATA = 4096 - 67,
};

enum EEPROMSectorIndex {
    I_SERVER_MODE = 0,
    I_SSID = I_SERVER_MODE + S_SERVER_MODE,
    I_PASSWORD = I_SSID + S_SSID,
    I_DATA = I_PASSWORD + S_PASSWORD,
};

void writeBufferToEEPROM(int offset, uint8_t *buffer, int length) {
    EEPROM.write(offset + 0, length & 0xFF);
    EEPROM.write(offset + 1, (length & 0xFF00) >> 8);
    for (int i = 0; i < length; i++) {
        EEPROM.write(offset + 2 + i, buffer[i]);
    }

}

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

uint8_t *readBufferFromEEPROM(int offset) {
    int length = EEPROM.read(offset + 0) + (EEPROM.read(offset + 1) << 8);
    uint8_t *data = new uint8_t[length];
    for (int i = 0; i < length; i++) {
        data[i] = EEPROM.read(offset + 2 + i);
    }
    return data;
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

const char successJson[] = R"rawliteral({"status":"success"})rawliteral";

String toBufferInfo(uint16_t *buffer, int length) {
    String content = "uint16_t buffer[";
    content += length;
    content += "] = {";
    for (int i = 0; i < length - 1; i++) {
        content += buffer[i];
        content += ", ";
    }
    content += buffer[length - 1];
    content += "};";
    return content;
}

String toJsonBufferInfo(uint16_t *buffer, int length) {
    String content = "[";
    for (int i = 0; i < length - 1; i++) {
        content += buffer[i];
        content += ",";
    }
    content += buffer[length - 1];
    content += "]";
    return content;
}

bool dumpLock = true;

int playBufferLength = -1;
uint16_t *playBuffer = NULL;

void setupServer() {
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
    server.on("/playDump", HTTP_GET, [](AsyncWebServerRequest *request) {
        status = PLAY_DUMP;
        request->send_P(200, "application/json", successJson);
    });
    server.on("/dumpLock", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("lock")) {
            String lock = request->getParam("lock")->value();
            if (lock == "true") {
                dumpLock = true;
            } else if (lock == "false") {
                dumpLock = false;
            }
            request->send_P(200, "application/json", successJson);
        } else {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/play", HTTP_GET, [](AsyncWebServerRequest *request) {
        if (request->hasParam("buffer")) {
            free(playBuffer);
            playBuffer = NULL;

            String bufferString = request->getParam("buffer")->value();
            DynamicJsonDocument doc(2048);
            deserializeJson(doc, bufferString);
            JsonArray array = doc.as<JsonArray>();
            playBufferLength = array.size();
            playBuffer = new uint16_t[array.size()];
            for (int i = 0; i < playBufferLength; i++) {
                playBuffer[i] = array[i];
            }
            status = PLAY;
            request->send_P(200, "application/json", successJson);
        } else {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });
    server.on("/dump", HTTP_GET, [](AsyncWebServerRequest *request) {
        int rawLength = (EEPROM.read(I_DATA + 0) + (EEPROM.read(I_DATA + 1) << 8)) / 2;
        u_int16_t *buffer = (uint16_t *) readBufferFromEEPROM(I_DATA);
        debug("/dump Request\n");
        String bufferInfo;
        if (request->hasParam("json")) {
            bufferInfo = toJsonBufferInfo(buffer, rawLength);
        } else {
            bufferInfo = toBufferInfo(buffer, rawLength);
        }
        debug(bufferInfo + "\n");
        request->send_P(200, "text/plain", bufferInfo.c_str());
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

IRsend irsend(D3);

IRrecv irrecv(D6, 1024, 50, true);

decode_results results;

void setupIR() {
    irsend.begin();
    irrecv.enableIRIn();
}

void setup() {
    setupSerial();
    setupEEPROM();
    setupServer();
    setupIR();
}

void loop() {
    if (status == CHANGE_AP_MODE) {
        changeAPMode();
    } else if (status == LOGIC) {
        if (irrecv.decode(&results) && !dumpLock) {
            int rawLength = getCorrectedRawLength(&results) * (sizeof(uint16_t) / sizeof(uint8_t));
            uint8_t *startBuf = (uint8_t *) resultToRawArray(&results);
            writeBufferToEEPROM(I_DATA, startBuf, rawLength);
            EEPROM.commit();

            debug("Dump success!\n");
            debug(toBufferInfo((uint16_t *) startBuf, rawLength / (sizeof(uint16_t) / sizeof(uint8_t))) + "\n");

            free(startBuf);
        }
    } else if (status == PLAY_DUMP) {
        int rawLength = (EEPROM.read(I_DATA + 0) + (EEPROM.read(I_DATA + 1) << 8)) / 2;
        u_int16_t *buffer = (uint16_t *) readBufferFromEEPROM(I_DATA);
        for (int i = 0; i < 5; i++) {
            irsend.sendRaw(buffer, rawLength, 38);
        }
        debug("Play success!\n");
        free(buffer);
        status = LOGIC;
    } else if (status == PLAY) {
        debug("Play\n");
        debug(toBufferInfo(playBuffer, playBufferLength) + "\n");

        for (int i = 0; i < 5; i++) {
            irsend.sendRaw(playBuffer, playBufferLength, 38);
        }
        free(playBuffer);
        playBuffer = NULL;
        status = LOGIC;
    }
}