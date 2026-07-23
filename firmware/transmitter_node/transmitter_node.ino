#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <math.h>

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

constexpr uint8_t ESPNOW_CHANNEL = 9;
constexpr uint8_t SHT31_ADDRESS = 0x44;

constexpr unsigned long SEND_INTERVAL_MS = 1500;

// MAC address of the receiver ESP32 used during the documented test.
// Replace it with the Wi-Fi station MAC address of your own receiver
// before uploading this sketch.
uint8_t receiverMac[] = {
  0xFC, 0xE8, 0xC0, 0x7C, 0x30, 0xA8
};

// -----------------------------------------------------------------------------
// SHT31 sensor
// The default ESP32 I2C bus is retained, matching the original working setup.
// -----------------------------------------------------------------------------

Adafruit_SHT31 sht31 = Adafruit_SHT31();

// -----------------------------------------------------------------------------
// ESP-NOW packet
// This structure must be identical to the receiver structure.
// -----------------------------------------------------------------------------

struct __attribute__((packed)) TelemetryPacket {
  uint32_t packetNumber;
  float temperature;
  float humidity;
};

// -----------------------------------------------------------------------------
// Transmission status
// -----------------------------------------------------------------------------

volatile bool sendStatusAvailable = false;
volatile esp_now_send_status_t latestSendStatus = ESP_NOW_SEND_FAIL;

uint32_t nextPacketNumber = 1;
unsigned long previousSendTime = 0;

// -----------------------------------------------------------------------------
// Utility functions
// -----------------------------------------------------------------------------

void printMacAddress(const uint8_t *macAddress) {
  for (uint8_t i = 0; i < 6; i++) {
    if (macAddress[i] < 0x10) {
      Serial.print('0');
    }

    Serial.print(macAddress[i], HEX);

    if (i < 5) {
      Serial.print(':');
    }
  }
}

void stopWithError(const char *message) {
  Serial.print("[FAIL] ");
  Serial.println(message);

  while (true) {
    delay(1000);
  }
}

// -----------------------------------------------------------------------------
// ESP-NOW send callback
// -----------------------------------------------------------------------------

void onDataSent(
  const esp_now_send_info_t *sendInfo,
  esp_now_send_status_t status
) {
  latestSendStatus = status;
  sendStatusAvailable = true;
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 ESP-NOW Transmitter Node");
  Serial.println("================================");

  // Keep the same default I2C connection used by the original transmitter.
  Wire.begin();

  if (!sht31.begin(SHT31_ADDRESS)) {
    stopWithError(
      "SHT31 was not detected at address 0x44."
    );
  }

  Serial.println("[PASS] SHT31 initialized.");

  // ESP-NOW uses the Wi-Fi radio without connecting to a router.
  WiFi.mode(WIFI_STA);
  delay(100);

  Serial.print("Transmitter MAC: ");
  Serial.println(WiFi.macAddress());

  esp_err_t channelResult =
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (channelResult != ESP_OK) {
    stopWithError(
      "Could not set the ESP-NOW channel."
    );
  }

  Serial.print("ESP-NOW channel: ");
  Serial.println(ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    stopWithError(
      "ESP-NOW initialization failed."
    );
  }

  if (esp_now_register_send_cb(onDataSent) != ESP_OK) {
    stopWithError(
      "Could not register the send callback."
    );
  }

  esp_now_peer_info_t peerInfo = {};

  memcpy(
    peerInfo.peer_addr,
    receiverMac,
    sizeof(receiverMac)
  );

  peerInfo.channel = ESPNOW_CHANNEL;
  peerInfo.ifidx = WIFI_IF_STA;
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    stopWithError(
      "Could not add the receiver as an ESP-NOW peer."
    );
  }

  Serial.print("Receiver MAC: ");
  printMacAddress(receiverMac);
  Serial.println();

  Serial.println("[PASS] ESP-NOW initialized.");
  Serial.println("[PASS] Receiver peer added.");
  Serial.println("Starting telemetry transmission...");
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop() {
  // Print the result reported by the ESP-NOW callback.
  if (sendStatusAvailable) {
    esp_now_send_status_t status = latestSendStatus;
    sendStatusAvailable = false;

    Serial.print("Delivery status: ");

    if (status == ESP_NOW_SEND_SUCCESS) {
      Serial.println("SUCCESS");
    } else {
      Serial.println("FAILED");
    }
  }

  const unsigned long currentTime = millis();

  if (currentTime - previousSendTime < SEND_INTERVAL_MS) {
    delay(5);
    return;
  }

  previousSendTime = currentTime;

  const float temperature = sht31.readTemperature();
  const float humidity = sht31.readHumidity();

  if (!isfinite(temperature) || !isfinite(humidity)) {
    Serial.println(
      "[FAIL] Invalid SHT31 reading. Packet was not sent."
    );
    return;
  }

  TelemetryPacket packet;

  packet.packetNumber = nextPacketNumber;
  packet.temperature = temperature;
  packet.humidity = humidity;

  esp_err_t sendResult = esp_now_send(
    receiverMac,
    reinterpret_cast<const uint8_t *>(&packet),
    sizeof(packet)
  );

  if (sendResult == ESP_OK) {
    Serial.print("Packet ");
    Serial.print(packet.packetNumber);

    Serial.print(" queued | Temperature: ");
    Serial.print(packet.temperature, 2);
    Serial.print(" C");

    Serial.print(" | Humidity: ");
    Serial.print(packet.humidity, 2);
    Serial.println(" %");

    nextPacketNumber++;
  } else {
    Serial.print("[FAIL] Could not queue packet. Error code: ");
    Serial.println(static_cast<int>(sendResult));
  }
}