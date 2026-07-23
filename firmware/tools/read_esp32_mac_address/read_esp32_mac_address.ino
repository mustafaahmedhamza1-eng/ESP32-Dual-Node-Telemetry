#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  delay(500);

  // ESP-NOW in this project uses the Wi-Fi station interface.
  WiFi.mode(WIFI_STA);
  delay(100);

  Serial.println();
  Serial.println("ESP32 Wi-Fi Station MAC Address:");
  Serial.println(WiFi.macAddress());
}

void loop() {
  // No repeated operation is required.
}