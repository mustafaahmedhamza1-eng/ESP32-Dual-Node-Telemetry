# Troubleshooting

This document covers the most common setup, sensor, display, and ESP-NOW communication problems that may occur while running the project.

## ESP32 Is Not Detected by the Computer

If the Arduino IDE does not show a serial port:

1. Use a USB cable that supports data transfer, not charging only.
2. Connect the ESP32 directly to the computer.
3. Install the correct USB-to-UART driver.

The ESP32 boards used in this project required the Silicon Labs CP210x driver.

After installing the driver, restart the Arduino IDE and check:

```text
Tools → Port
```

Then select the port assigned to the ESP32.

Also verify that the selected board is:

```text
ESP32 Dev Module
```

## Serial Monitor Shows Unreadable Characters

Set Serial Monitor to:

```text
115200 baud
```

If the output is still unreadable:

- close and reopen Serial Monitor;
- press the ESP32 reset button;
- confirm that the sketch uses `Serial.begin(115200)`.

## SHT31 Sensor Is Not Detected

Check the transmitter wiring:

| SHT31 Pin | ESP32 Pin |
|---|---|
| VIN | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

Run the included diagnostic sketch:

```text
firmware/diagnostics/sht31_transmitter_diagnostic/sht31_transmitter_diagnostic.ino
```

The SHT31 normally appears at I2C address:

```text
0x44
```

Some modules may use:

```text
0x45
```

The transmitter sketch is configured for address `0x44`. If the diagnostic detects the sensor at `0x45`, change:

```cpp
constexpr uint8_t SHT31_ADDRESS = 0x44;
```

to:

```cpp
constexpr uint8_t SHT31_ADDRESS = 0x45;
```

If the sensor is not detected:

- check the power and ground connections;
- verify the SDA and SCL wires;
- disconnect and reconnect the module;
- confirm that the correct ESP32 board and port are selected;
- inspect the sensor module for loose wires or reversed connections.

The intentional disconnected-sensor test in the repository demonstrates the expected error behavior when the SHT31 is unavailable.

## Temperature or Humidity Readings Are Invalid

If the transmitter reports invalid readings:

- confirm that the SHT31 was initialized successfully;
- check that the sensor is firmly connected;
- avoid touching the sensing element during testing;
- allow the sensor a few seconds to stabilize after powering the board;
- run the standalone SHT31 diagnostic before testing ESP-NOW communication.

Wireless packets should not be sent when the sensor readings are invalid.

## OLED Display Is Blank

Check the receiver wiring:

| OLED Pin | ESP32 Pin |
|---|---|
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

Also verify that:

- the display is an SH1106 128 × 64 OLED;
- the U8g2 library is installed;
- the receiver sketch was uploaded to the correct ESP32;
- the OLED wires are connected to the receiver board, not the transmitter;
- the board was reset after uploading the sketch.

The transmitter and receiver are separate ESP32 boards, so both nodes can use GPIO 21 and GPIO 22 for their own I2C devices without conflict.

## Receiver Remains on `Status: Waiting`

This means the receiver has initialized but has not received a valid telemetry packet.

Check that:

- the transmitter ESP32 is powered on;
- the transmitter detected the SHT31 successfully;
- the transmitter and receiver use the same ESP-NOW channel;
- the receiver MAC address in the transmitter sketch is correct;
- the transmitter Serial Monitor reports that packets are being sent.

Both nodes in the documented test use:

```text
ESP-NOW channel: 9
```

## `Delivery Status: FAILED`

This message means that ESP-NOW did not confirm delivery to the receiver.

The most common causes are:

- the receiver is powered off;
- the receiver MAC address is incorrect;
- the transmitter and receiver are using different channels;
- the receiver sketch is not running;
- the boards are too far apart or affected by wireless interference.

During hardware testing, the transmitter reported failed delivery while the receiver was powered off. Delivery changed to successful automatically after the receiver was powered on.

A reset was not required.

## Receiver MAC Address Is Incorrect

The MAC address inside:

```text
firmware/transmitter_node/transmitter_node.ino
```

belongs to the receiver board used during the documented hardware test.

It is not a universal ESP32 address. A different receiver board requires its own Wi-Fi station MAC address.

Use the included utility:

```text
firmware/tools/read_esp32_mac_address/read_esp32_mac_address.ino
```

Upload it to the receiver ESP32 and open Serial Monitor at:

```text
115200 baud
```

An address displayed as:

```text
AA:BB:CC:DD:EE:FF
```

must be entered in the transmitter sketch as:

```cpp
uint8_t receiverMac[] = {
  0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF
};
```

Do not copy the colon-separated address directly into the C++ array.

## Receiver Shows `Status: Link lost`

This status appears when the receiver does not receive a packet for five seconds.

It can occur when:

- the transmitter is powered off;
- the transmitter is reset;
- the SHT31 becomes unavailable;
- wireless communication is interrupted;
- the transmitter stops sending packets.

This behavior is intentional and does not necessarily indicate a receiver failure.

When valid packets are received again, the display should return automatically to the connected state.

## Communication Does Not Recover Automatically

If the transmitter is powered on again but the receiver remains disconnected:

1. Check the transmitter Serial Monitor.
2. Confirm that the SHT31 is detected.
3. Confirm that packets are being sent.
4. Verify the receiver MAC address.
5. Verify that both boards still use the same ESP-NOW channel.
6. Press the transmitter reset button.

Restart both boards only after checking the configuration, because restarting cannot correct an incorrect MAC address or channel mismatch.

## Packet Number Restarts from `1`

The packet number is stored in the transmitter memory while the sketch is running.

When the transmitter is reset or powered off, its packet counter restarts from:

```text
1
```

This is expected behavior.

The receiver maintains its own received-packet count and may continue from its previous value as long as the receiver itself was not restarted.

## Compilation Errors

Confirm that the following libraries are installed:

- Adafruit SHT31 Library
- U8g2 Library

The following headers are supplied by the ESP32 board package:

```cpp
#include <WiFi.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <Wire.h>
```

The project was tested with:

```text
Arduino IDE: 2.3.7
ESP32 package: esp32 by Espressif Systems 3.3.4
Board: ESP32 Dev Module
```

Different ESP32 package versions may use different ESP-NOW callback definitions. Using the documented version is recommended when reproducing the project.

## Upload Fails or ESP32 Does Not Enter Programming Mode

Try the following:

- close Serial Monitor before uploading;
- confirm that the correct port is selected;
- press and hold the `BOOT` button when uploading begins;
- release `BOOT` when the upload process starts;
- reconnect the USB cable;
- press the `EN` or reset button;
- verify that the CP210x driver is installed correctly.

## Further Checks

When a problem occurs, test each part separately in this order:

1. confirm that the ESP32 is detected by the computer;
2. run the SHT31 diagnostic;
3. verify the receiver OLED;
4. read the receiver MAC address;
5. upload the receiver sketch;
6. upload the configured transmitter sketch;
7. inspect both Serial Monitor outputs;
8. test connection loss and automatic recovery.

Testing the sensor, display, and wireless communication separately makes it easier to identify the source of the problem.