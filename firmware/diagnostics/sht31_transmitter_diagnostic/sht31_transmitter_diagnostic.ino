#include <Wire.h>
#include <Adafruit_SHT31.h>
#include <math.h>

// Local hardware configuration.
// The working firmware used the default ESP32 I2C pins.
constexpr uint8_t SHT31_SDA_PIN = 21;
constexpr uint8_t SHT31_SCL_PIN = 22;

constexpr uint8_t SHT31_ADDRESS_1 = 0x44;
constexpr uint8_t SHT31_ADDRESS_2 = 0x45;

constexpr int SAMPLE_COUNT = 10;
constexpr unsigned long SAMPLE_INTERVAL_MS = 1500;

Adafruit_SHT31 sht31;

bool probeI2CAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

int scanI2CBus() {
  int detectedDevices = 0;

  Serial.println();
  Serial.println("Scanning SHT31 I2C bus");

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("  Device found at 0x");

      if (address < 16) {
        Serial.print("0");
      }

      Serial.println(address, HEX);
      detectedDevices++;
    }
  }

  if (detectedDevices == 0) {
    Serial.println("  No I2C devices detected");
  }

  return detectedDevices;
}

uint8_t detectSHT31Address() {
  if (probeI2CAddress(SHT31_ADDRESS_1)) {
    return SHT31_ADDRESS_1;
  }

  if (probeI2CAddress(SHT31_ADDRESS_2)) {
    return SHT31_ADDRESS_2;
  }

  return 0;
}

bool isValidReading(float temperatureC, float humidityPercent) {
  if (isnan(temperatureC) || isnan(humidityPercent)) {
    return false;
  }

  if (temperatureC < -40.0f || temperatureC > 125.0f) {
    return false;
  }

  if (humidityPercent < 0.0f || humidityPercent > 100.0f) {
    return false;
  }

  return true;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("================================");
  Serial.println("ESP32 Transmitter Node Diagnostics");
  Serial.println("SHT31 Temperature and Humidity Test");
  Serial.println("================================");

  Wire.begin(SHT31_SDA_PIN, SHT31_SCL_PIN, 100000);

  scanI2CBus();

  uint8_t detectedAddress = detectSHT31Address();

  if (detectedAddress == 0) {
    Serial.println();
    Serial.println("[FAIL] SHT31 was not detected");
    Serial.println("Check power, ground, SDA, SCL, and I2C address.");
    Serial.println();
    Serial.println("Press the ESP32 RST button to run the test again.");
    return;
  }

  Serial.print("[PASS] SHT31 detected at address 0x");
  Serial.println(detectedAddress, HEX);

  if (!sht31.begin(detectedAddress)) {
    Serial.println("[FAIL] SHT31 library initialization failed");
    return;
  }

  Serial.println("[PASS] SHT31 communication established");
  Serial.println();
  Serial.println("Reading ten samples");
  Serial.println("--------------------------------");
  Serial.println("Sample,Temperature_C,Humidity_Percent");

  int validSamples = 0;

  float temperatureSum = 0.0f;
  float humiditySum = 0.0f;

  float minimumTemperature = 1000.0f;
  float maximumTemperature = -1000.0f;

  float minimumHumidity = 1000.0f;
  float maximumHumidity = -1000.0f;

  for (int sampleNumber = 1; sampleNumber <= SAMPLE_COUNT; sampleNumber++) {
    float temperatureC = sht31.readTemperature();
    float humidityPercent = sht31.readHumidity();

    Serial.print(sampleNumber);
    Serial.print(",");

    if (isValidReading(temperatureC, humidityPercent)) {
      Serial.print(temperatureC, 2);
      Serial.print(",");
      Serial.println(humidityPercent, 2);

      temperatureSum += temperatureC;
      humiditySum += humidityPercent;

      if (temperatureC < minimumTemperature) {
        minimumTemperature = temperatureC;
      }

      if (temperatureC > maximumTemperature) {
        maximumTemperature = temperatureC;
      }

      if (humidityPercent < minimumHumidity) {
        minimumHumidity = humidityPercent;
      }

      if (humidityPercent > maximumHumidity) {
        maximumHumidity = humidityPercent;
      }

      validSamples++;
    } else {
      Serial.println("INVALID,INVALID");
    }

    delay(SAMPLE_INTERVAL_MS);
  }

  Serial.println();
  Serial.println("Diagnostic summary");
  Serial.println("--------------------------------");

  Serial.print("Valid samples: ");
  Serial.print(validSamples);
  Serial.print("/");
  Serial.println(SAMPLE_COUNT);

  if (validSamples > 0) {
    float averageTemperature = temperatureSum / validSamples;
    float averageHumidity = humiditySum / validSamples;

    Serial.print("Average temperature: ");
    Serial.print(averageTemperature, 2);
    Serial.println(" C");

    Serial.print("Temperature range: ");
    Serial.print(minimumTemperature, 2);
    Serial.print(" to ");
    Serial.print(maximumTemperature, 2);
    Serial.println(" C");

    Serial.print("Average humidity: ");
    Serial.print(averageHumidity, 2);
    Serial.println(" %");

    Serial.print("Humidity range: ");
    Serial.print(minimumHumidity, 2);
    Serial.print(" to ");
    Serial.print(maximumHumidity, 2);
    Serial.println(" %");
  }

  Serial.println("--------------------------------");

  if (validSamples == SAMPLE_COUNT) {
    Serial.println("Overall result: PASS");
  } else if (validSamples > 0) {
    Serial.println("Overall result: PARTIAL PASS");
  } else {
    Serial.println("Overall result: FAIL");
  }

  Serial.println();
  Serial.println("Press the ESP32 RST button to run the test again.");
}

void loop() {
  // Diagnostics run once during startup.
}