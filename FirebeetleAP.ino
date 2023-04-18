#include <WiFi.h>

const char* ssid     = "Omega-Test";
const char* password = "123456789";

void setup()
{
    Serial.begin(115200);
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    Serial.println(WiFi.softAPIP());
}

void loop(){}
