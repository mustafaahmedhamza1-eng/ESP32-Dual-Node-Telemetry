#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Wire.h>
#include <U8g2lib.h>

// -----------------------------------------------------------------------------
// Configuration
// -----------------------------------------------------------------------------

constexpr uint8_t ESPNOW_CHANNEL = 9;

constexpr uint8_t OLED_SDA_PIN = 21;
constexpr uint8_t OLED_SCL_PIN = 22;

constexpr unsigned long CONNECTION_TIMEOUT_MS = 5000;
constexpr unsigned long DISPLAY_REFRESH_MS = 250;

// -----------------------------------------------------------------------------
// OLED display
// -----------------------------------------------------------------------------

U8G2_SH1106_128X64_NONAME_F_HW_I2C display(
  U8G2_R0,
  U8X8_PIN_NONE
);

// -----------------------------------------------------------------------------
// ESP-NOW packet
// This structure must be identical in the transmitter code.
// -----------------------------------------------------------------------------

struct __attribute__((packed)) TelemetryPacket {
  uint32_t packetNumber;
  float temperature;
  float humidity;
};

// -----------------------------------------------------------------------------
// Received data
// -----------------------------------------------------------------------------

TelemetryPacket latestPacket = {};

uint8_t lastSenderMac[6] = {};

bool hasReceivedData = false;
bool newDataAvailable = false;

uint32_t receivedPacketCount = 0;
unsigned long lastPacketTime = 0;

portMUX_TYPE dataLock = portMUX_INITIALIZER_UNLOCKED;

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

void showStartupMessage(const char *line1, const char *line2) {
  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tr);

  display.drawStr(0, 20, line1);
  display.drawStr(0, 40, line2);

  display.sendBuffer();
}

void stopWithError(const char *serialMessage, const char *displayMessage) {
  Serial.print("[FAIL] ");
  Serial.println(serialMessage);

  showStartupMessage("Initialization failed", displayMessage);

  while (true) {
    delay(1000);
  }
}

// -----------------------------------------------------------------------------
// ESP-NOW receive callback
// -----------------------------------------------------------------------------

void onDataReceived(
  const esp_now_recv_info_t *receiveInfo,
  const uint8_t *incomingData,
  int dataLength
) {
  if (receiveInfo == nullptr || incomingData == nullptr) {
    return;
  }

  if (dataLength != sizeof(TelemetryPacket)) {
    return;
  }

  TelemetryPacket receivedPacket;
  memcpy(&receivedPacket, incomingData, sizeof(receivedPacket));

  if (!isfinite(receivedPacket.temperature) ||
      !isfinite(receivedPacket.humidity)) {
    return;
  }

  portENTER_CRITICAL(&dataLock);

  latestPacket = receivedPacket;
  memcpy(lastSenderMac, receiveInfo->src_addr, sizeof(lastSenderMac));

  receivedPacketCount++;
  lastPacketTime = millis();

  hasReceivedData = true;
  newDataAvailable = true;

  portEXIT_CRITICAL(&dataLock);
}

// -----------------------------------------------------------------------------
// OLED update
// -----------------------------------------------------------------------------

void updateDisplay(
  bool dataReceived,
  bool connectionActive,
  const TelemetryPacket &packet,
  uint32_t packetCount
) {
  char line[32];

  display.clearBuffer();
  display.setFont(u8g2_font_6x12_tr);

  display.drawStr(0, 10, "ESP-NOW RECEIVER");

  if (!dataReceived) {
    display.drawStr(0, 24, "Status: Waiting");
    display.drawStr(0, 42, "No telemetry data");
    display.drawStr(0, 56, "received yet");
  } else {
    if (connectionActive) {
      display.drawStr(0, 24, "Status: Connected");
    } else {
      display.drawStr(0, 24, "Status: Link lost");
    }

    snprintf(
      line,
      sizeof(line),
      "Temp: %.2f C",
      packet.temperature
    );
    display.drawStr(0, 36, line);

    snprintf(
      line,
      sizeof(line),
      "Humidity: %.2f %%",
      packet.humidity
    );
    display.drawStr(0, 48, line);

    snprintf(
      line,
      sizeof(line),
      "Packets: %lu",
      static_cast<unsigned long>(packetCount)
    );
    display.drawStr(0, 60, line);
  }

  display.sendBuffer();
}

// -----------------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 ESP-NOW Receiver Node");
  Serial.println("================================");

  // Start the OLED on its existing wiring.
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN, 100000);

  if (!display.begin()) {
    Serial.println("[FAIL] OLED initialization failed.");
  }

  showStartupMessage("Receiver starting", "Please wait...");

  // ESP-NOW uses the Wi-Fi radio, but it does not connect to a router.
  WiFi.mode(WIFI_STA);
  delay(100);

  Serial.print("Receiver MAC: ");
  Serial.println(WiFi.macAddress());

  // Both ESP32 boards must use the same channel.
  esp_err_t channelResult =
    esp_wifi_set_channel(ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (channelResult != ESP_OK) {
    stopWithError(
      "Could not set the ESP-NOW channel.",
      "Channel error"
    );
  }

  Serial.print("ESP-NOW channel: ");
  Serial.println(ESPNOW_CHANNEL);

  if (esp_now_init() != ESP_OK) {
    stopWithError(
      "ESP-NOW initialization failed.",
      "ESP-NOW error"
    );
  }

  if (esp_now_register_recv_cb(onDataReceived) != ESP_OK) {
    stopWithError(
      "Could not register the receive callback.",
      "Callback error"
    );
  }

  Serial.println("[PASS] ESP-NOW initialized.");
  Serial.println("Waiting for telemetry data...");

  showStartupMessage("ESP-NOW ready", "Waiting for data");
}

// -----------------------------------------------------------------------------
// Main loop
// -----------------------------------------------------------------------------

void loop() {
  TelemetryPacket packetSnapshot;
  uint8_t senderMacSnapshot[6];

  bool dataReceivedSnapshot;
  bool newDataSnapshot;

  uint32_t packetCountSnapshot;
  unsigned long lastPacketTimeSnapshot;

  // Copy the shared data safely.
  portENTER_CRITICAL(&dataLock);

  packetSnapshot = latestPacket;
  memcpy(
    senderMacSnapshot,
    lastSenderMac,
    sizeof(senderMacSnapshot)
  );

  dataReceivedSnapshot = hasReceivedData;
  newDataSnapshot = newDataAvailable;

  packetCountSnapshot = receivedPacketCount;
  lastPacketTimeSnapshot = lastPacketTime;

  newDataAvailable = false;

  portEXIT_CRITICAL(&dataLock);

  const unsigned long currentTime = millis();

  const bool connectionActive =
    dataReceivedSnapshot &&
    (currentTime - lastPacketTimeSnapshot <= CONNECTION_TIMEOUT_MS);

  if (newDataSnapshot) {
    Serial.print("Packet ");
    Serial.print(packetSnapshot.packetNumber);

    Serial.print(" received from ");
    printMacAddress(senderMacSnapshot);

    Serial.print(" | Temperature: ");
    Serial.print(packetSnapshot.temperature, 2);
    Serial.print(" C");

    Serial.print(" | Humidity: ");
    Serial.print(packetSnapshot.humidity, 2);
    Serial.println(" %");
  }

  static unsigned long previousDisplayUpdate = 0;

  if (currentTime - previousDisplayUpdate >= DISPLAY_REFRESH_MS) {
    previousDisplayUpdate = currentTime;

    updateDisplay(
      dataReceivedSnapshot,
      connectionActive,
      packetSnapshot,
      packetCountSnapshot
    );
  }

  delay(5);
}