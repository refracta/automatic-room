#define DEBUG_MODE true

#if DEBUG_MODE
#define debug(log) Serial.print(log)
#else
#define debug(log)
#endif

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

char AP_MODE_SSID[32 + 1] = "LIGHT-CONTROLLER";
IPAddress localIp(192, 168, 0, 1);
IPAddress gateway(192, 168, 0, 254);
IPAddress subnet(255, 255, 255, 0);

AsyncWebServer server(80);

struct LightDevice
{
    const char* name;
    const uint8_t pin;
    const char* pinName;
    bool status;
    const char* defaultAlias;
};

LightDevice lights[] = {
    {"light1", D2, "D2", true, nullptr},
    {"light2", D3, "D3", true, nullptr},
    {"light3", D4, "D4", true, nullptr}
};

enum ProcessStatus
{
    LOGIC,
    CHANGE_AP_MODE,
};

ProcessStatus status = CHANGE_AP_MODE;

enum ServerMode
{
    AP,
    STA
};

enum EEPROMSectorSize
{
    S_NAME = 32 + 1,
    S_SERVER_MODE = 1,
    S_SSID = 32 + 1,
    S_PASSWORD = 32 + 1,
    S_AUTH_USERNAME = 32 + 1,
    S_AUTH_PASSWORD = 32 + 1,
    S_ALIAS = 32 + 1,
    S_SCRIPT_ENTRYPOINT = 64 + 1,
};

enum EEPROMSectorIndex
{
    I_NAME = 0,
    I_SERVER_MODE = I_NAME + S_NAME,
    I_SSID = I_SERVER_MODE + S_SERVER_MODE,
    I_PASSWORD = I_SSID + S_SSID,
    I_AUTH_USERNAME = I_PASSWORD + S_PASSWORD,
    I_AUTH_PASSWORD = I_AUTH_USERNAME + S_AUTH_USERNAME,
    I_ALIAS_LIGHT1 = I_AUTH_PASSWORD + S_AUTH_PASSWORD,
    I_ALIAS_LIGHT2 = I_ALIAS_LIGHT1 + S_ALIAS,
    I_ALIAS_LIGHT3 = I_ALIAS_LIGHT2 + S_ALIAS,
    I_SCRIPT_ENTRYPOINT = I_ALIAS_LIGHT3 + S_ALIAS,
};

void writeStringToEEPROM(int offset, const String& target)
{
    byte length = target.length();
    EEPROM.write(offset, length);
    for (int i = 0; i < length; i++)
    {
        EEPROM.write(offset + 1 + i, target[i]);
    }
}

String readStringFromEEPROM(int offset)
{
    int length = EEPROM.read(offset);
    char data[length + 1];
    for (int i = 0; i < length; i++)
    {
        data[i] = EEPROM.read(offset + 1 + i);
    }
    data[length] = '\0';
    return String(data);
}

bool isAuthenticated(AsyncWebServerRequest* request)
{
    String username = readStringFromEEPROM(I_AUTH_USERNAME);
    String password = readStringFromEEPROM(I_AUTH_PASSWORD);
    if (username.length() > 0 && password.length() > 0)
    {
        return request->authenticate(username.c_str(), password.c_str());
    }
    return true;
}

void setupSerial()
{
    Serial.begin(115200);
}

void setupEEPROM()
{
    EEPROM.begin(4096);
    String name = readStringFromEEPROM(I_NAME);
    if (name.length() > 0)
    {
        strncpy(AP_MODE_SSID, name.c_str(), sizeof(AP_MODE_SSID) - 1);
    }
}

void resetEEPROM()
{
    for (int i = 0; i < 4096; i++)
    {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
}

void runServer()
{
    debug("runServer()\n");
    WiFi.softAPdisconnect(true);
    String ssid = readStringFromEEPROM(I_SSID);
    String password = readStringFromEEPROM(I_PASSWORD);

    ServerMode mode = (ServerMode)EEPROM.read(I_SERVER_MODE);
    switch (mode)
    {
    case STA:
        debug("STA: ");
        debug(ssid);
        WiFi.mode(WIFI_STA);
        if (password.length() == 0)
        {
            WiFi.begin(ssid);
        }
        else
        {
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
        WiFi.softAPConfig(localIp, gateway, subnet);
        WiFi.softAP(AP_MODE_SSID);
        break;
    }
    server.begin();
    debug("Server on!\n");
}

const char successJson[] = R"rawliteral({"status":"success"})rawliteral";

String getStatus()
{
    String json;
    StaticJsonDocument<1024> doc;

    JsonArray devices = doc.createNestedArray("devices");
    for (size_t i = 0; i < sizeof(lights) / sizeof(lights[0]); i++)
    {
        JsonObject device = devices.createNestedObject();
        device["name"] = lights[i].name;
        device["status"] = lights[i].status;
        device["pin"] = lights[i].pinName;
        String alias = readStringFromEEPROM(I_ALIAS_LIGHT1 + i * S_ALIAS);
        if (alias.length() > 0)
        {
            device["alias"] = alias;
        }
    }

    doc[F("mac")] = WiFi.macAddress();
    doc[F("ssid")] = readStringFromEEPROM(I_SSID);
    doc[F("name")] = AP_MODE_SSID;
    String scriptURL = readStringFromEEPROM(I_SCRIPT_ENTRYPOINT);
    if (scriptURL.length() == 0)
    {
        scriptURL = "/script";
        writeStringToEEPROM(I_SCRIPT_ENTRYPOINT, scriptURL);
        EEPROM.commit();
    }
    doc[F("scriptURL")] = scriptURL;

    serializeJson(doc, json);
    return json;
}

Ticker restartTimer;

void restartCallback() {
    ESP.restart();
}

void setupServer()
{
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        if (!isAuthenticated(request))
        {
            return request->requestAuthentication();
        }
        request->send_P(200, "application/json", getStatus().c_str());
    });

    server.on("/reset", HTTP_GET, [](AsyncWebServerRequest* request) {
        resetEEPROM();
        request->send_P(200, "application/json", successJson);
        restartTimer.once(5, restartCallback);
    });

    server.on("/set/ap-mode", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        EEPROM.write(I_SERVER_MODE, AP);
        EEPROM.commit();
        status = CHANGE_AP_MODE;
        request->send_P(200, "application/json", successJson);
    });

    server.on("/set/wifi", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        if (request->hasParam("ssid") && request->hasParam("password"))
        {
            writeStringToEEPROM(I_SSID, request->getParam("ssid")->value());
            writeStringToEEPROM(I_PASSWORD, request->getParam("password")->value());
            EEPROM.write(I_SERVER_MODE, STA);
            EEPROM.commit();
            status = CHANGE_AP_MODE;
            request->send_P(200, "application/json", successJson);
        }
        else
        {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/set/auth", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        if (request->hasParam("username") && request->hasParam("password"))
        {
            writeStringToEEPROM(I_AUTH_USERNAME, request->getParam("username")->value());
            writeStringToEEPROM(I_AUTH_PASSWORD, request->getParam("password")->value());
            EEPROM.commit();
            request->send_P(200, "application/json", successJson);
        }
        else
        {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/set/device", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        if (request->hasParam("name") && request->hasParam("status"))
        {
            String name = request->getParam("name")->value();
            String status = request->getParam("status")->value();
            for (size_t i = 0; i < sizeof(lights) / sizeof(lights[0]); i++)
            {
                if (name == lights[i].name)
                {
                    digitalWrite(lights[i].pin, status == "on" ? HIGH : LOW);
                    lights[i].status = (status == "on");
                    request->send_P(200, "application/json", successJson);
                    return;
                }
            }
            request->send_P(200, "application/json", R"rawliteral({"error":"unknown device"})rawliteral");
        }
        else
        {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/set/alias", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        if (request->hasParam("name") && request->hasParam("value"))
        {
            String name = request->getParam("name")->value();
            String value = request->getParam("value")->value();
            for (size_t i = 0; i < sizeof(lights) / sizeof(lights[0]); i++)
            {
                if (name == lights[i].name)
                {
                    writeStringToEEPROM(I_ALIAS_LIGHT1 + i * S_ALIAS, value);
                    EEPROM.commit();
                    request->send_P(200, "application/json", successJson);
                    return;
                }
            }
            request->send_P(200, "application/json", R"rawliteral({"error":"unknown device"})rawliteral");
        }
        else
        {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/set/script", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        if (request->hasParam("value"))
        {
            writeStringToEEPROM(I_SCRIPT_ENTRYPOINT, request->getParam("value")->value());
            EEPROM.commit();
            request->send_P(200, "application/json", successJson);
        }
        else
        {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/set/name", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        if (request->hasParam("value"))
        {
            String value = request->getParam("value")->value();
            writeStringToEEPROM(I_NAME, value);
            EEPROM.commit();
            strncpy(AP_MODE_SSID, value.c_str(), sizeof(AP_MODE_SSID) - 1);
            request->send_P(200, "application/json", successJson);
        }
        else
        {
            request->send_P(200, "application/json", R"rawliteral({"error":"parameter error"})rawliteral");
        }
    });

    server.on("/script", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        request->send_P(200, "application/javascript", "console.log('script loaded!');");
    });

    server.on("/", HTTP_GET, [](AsyncWebServerRequest* request)
    {
        String scriptURL = readStringFromEEPROM(I_SCRIPT_ENTRYPOINT);
        if (scriptURL.length() == 0)
        {
            // scriptURL = "/script";
            scriptURL = "https://refracta.github.io/automatic-room/light-controller/script.js";
            writeStringToEEPROM(I_SCRIPT_ENTRYPOINT, scriptURL);
            EEPROM.commit();
        }
        String html = R"rawliteral(
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8">
            <script src=")rawliteral" + scriptURL + R"rawliteral("></script>
        </head>
        </html>
    )rawliteral";
        request->send_P(200, "text/html", html.c_str());
    });
}

const int TIMEOUT_SECOND = 10;

void changeAPMode()
{
    debug("changeAPMode()\n");
    runServer();
    ServerMode mode = (ServerMode)EEPROM.read(I_SERVER_MODE);
    if (mode == STA)
    {
        unsigned long waitStart = millis();
        while (WiFi.status() != WL_CONNECTED)
        {
            if (millis() - waitStart > TIMEOUT_SECOND * 1000)
            {
                EEPROM.write(I_SERVER_MODE, AP);
                EEPROM.commit();
                debug("Connection timeout\n");
                return;
            }
            else
            {
                yield();
            }
        }
    }
    status = LOGIC;
}

void setupLight()
{
    for (size_t i = 0; i < sizeof(lights) / sizeof(lights[0]); i++)
    {
        pinMode(lights[i].pin, OUTPUT);
        digitalWrite(lights[i].pin, lights[i].status ? HIGH : LOW);
    }
}

void setup()
{
    setupSerial();
    setupEEPROM();
    // resetEEPROM();
    setupLight();
    setupServer();
}

void loop()
{
    if (status == CHANGE_AP_MODE)
    {
        changeAPMode();
    }
    else if (status == LOGIC)
    {
        // Placeholder for logic
    }
}
