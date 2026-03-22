#include <WiFi.h>
#include <WiFiUdp.h>

const char* ssid = "ESP32_RC_Controller";
const char* password = "password1234";

WiFiUDP udp;
unsigned int localPort = 4210;

// Event handler for connection status
void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_AP_STACONNECTED) {
        Serial.println("[EVENT] Device Connected");
    } else if (event == ARDUINO_EVENT_WIFI_AP_STADISCONNECTED) {
        Serial.println("[EVENT] Device Disconnected");
    }
}

void setup() {
    Serial.begin(115200);

    // 1. Register the event handler before starting AP
    WiFi.onEvent(onWiFiEvent);

    // 2. Start Soft-AP 
    // Parameters: ssid, password, channel, hidden (0=visible), max_connection
    WiFi.softAP(ssid, password, 1, 0, 4);

    // 3. Performance optimization: Keep radio powered at all times
    WiFi.setSleep(false); 

    udp.begin(localPort);

    Serial.println("SYSTEM_READY");
    Serial.print("SSID: "); Serial.println(ssid);
    Serial.print("IP: ");   Serial.println(WiFi.softAPIP());
}

void loop() {
    int packetSize = udp.parsePacket();
    if (packetSize > 0) {
        uint8_t receivedByte;
        udp.read(&receivedByte, 1);
        
        switch (receivedByte) {
            case 1: Serial.println("Button1"); break;
            case 2: Serial.println("Button2"); break;
            case 3: Serial.println("Button3"); break;
            case 4: Serial.println("Button4"); break;
            default:
                Serial.print("Unknown_Input: ");
                Serial.println(receivedByte);
                break;
        }
    }
}
