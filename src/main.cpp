#include <Arduino.h>
#include <TCS3200.h>
#include "FilterSaturationStrategy.h"

#include <SuplaDevice.h>
#include <supla/network/esp_wifi.h>
#include <supla/control/button.h>
#include <supla/control/action_trigger.h>
#include <supla/device/status_led.h>
#include <supla/storage/littlefs_config.h>
#include <supla/network/esp_web_server.h>
#include <supla/network/html/device_info.h>
#include <supla/network/html/protocol_parameters.h>
#include <supla/network/html/status_led_parameters.h>
#include <supla/network/html/wifi_parameters.h>
#include <supla/events.h>
#include <supla/storage/eeprom.h>
#include <supla/sensor/general_purpose_measurement_base.h>
Supla::Eeprom eeprom;

#define STATUS_LED_GPIO 2
Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

Supla::Html::DeviceInfo htmlDeviceInfo(&SuplaDevice);
Supla::Html::WifiParameters htmlWifi;
Supla::Html::ProtocolParameters htmlProto;
Supla::Html::StatusLedParameters htmlStatusLed;

// Simple SUPLA sensor wrapper
class FilterSaturationSensor : public Supla::Sensor::GeneralPurposeMeasurementBase
{
public:
  FilterSaturationSensor() : value(0.0)
  {
    initialCaption("Filter Saturation");
    initialUnit("%");
  }

  void updateValue(float newValue)
  {
    value = newValue;
    setDouble(value);
  }

  double getValue() override
  {
    return value;
  }

private:
  float value;
};

FilterSaturationSensor *suplaSensor = nullptr;

// TCS3200 Color Sensor Pins
#define S0 2
#define S1 4
#define S2 16
#define S3 17
#define sensorOut 5
#define LED_PIN 18

// Create TCS3200 sensor object
TCS3200 colorSensor(S0, S1, S2, S3, sensorOut);

// Color names for dominant color detection
String colorNames[] = {"Red", "Green", "Blue"};

// Calibration values for filter saturation monitoring
FilterCalibration calibration = {
    .cleanChroma = 0.5,     // Calibrated for white surface (low chroma)
    .dirtyChroma = 0.92,    // Dirty filter has high chroma
    .cleanValue = 0.80,     // Clean filter brightness
    .dirtyValue = 0.40,     // Dirty filter is darker
    .cleanBlueRatio = 0.34, // Clean has higher blue ratio
    .dirtyBlueRatio = 0.10, // Dirty has lower blue (more brown/yellow)
    .cleanHSVSat = 0.10,    // Clean is nearly white (low saturation)
    .dirtyHSVSat = 0.80     // Dirty is highly saturated (colored)
};

// Create strategy instances
BrownScoreStrategy brownStrategy(&calibration);
HSVSaturationStrategy hsvStrategy(&calibration);
ChromaStrategy chromaStrategy(&calibration);
BrightnessStrategy brightnessStrategy(&calibration);
BlueRatioStrategy blueRatioStrategy(&calibration);
WeightedMixStrategy weightedMixStrategy(&calibration);

// Change if you want to try different one
FilterSaturationStrategy *currentStrategy = &brownStrategy;
int currentStrategyIndex = 0;
bool showAllStrategies = false;

// Sensor readings storage
RGBColor lastRGB = {0, 0, 0};
HSVColor lastHSV = {0, 0, 0};
float lastChroma = 0.0;
float lastSaturation = 0.0;
unsigned long lastSensorRead = 0;
const unsigned long SENSOR_READ_INTERVAL = 5000; // 5 seconds

// Array of all available strategies
FilterSaturationStrategy *strategies[] = {
    &weightedMixStrategy,
    &brownStrategy,
    &hsvStrategy,
    &chromaStrategy,
    &brightnessStrategy,
    &blueRatioStrategy};
const int strategyCount = 6;

void switchStrategy()
{
  currentStrategyIndex = (currentStrategyIndex + 1) % strategyCount;
  currentStrategy = strategies[currentStrategyIndex];

  Serial.println("\n*** STRATEGY SWITCHED ***");
  Serial.print("Active Strategy: ");
  Serial.println(currentStrategy->getName());
  Serial.println("************************\n");
}

void readSensorAndCalculate()
{
  // Read sensor ONCE
  lastRGB = colorSensor.read_rgb_color();
  SensorReadings readings(lastRGB);

  lastHSV = readings.hsv;
  lastChroma = readings.chroma;
  lastSaturation = currentStrategy->calculate(readings);
  lastSensorRead = millis();

#if ENABLE_SUPLA
  if (suplaSensor)
  {
    suplaSensor->updateValue(lastSaturation);
  }
#endif
}

void printAllStrategies(const SensorReadings &readings)
{
  Serial.println("\n=== ALL STRATEGIES COMPARISON ===");
  for (int i = 0; i < strategyCount; i++)
  {
    float result = strategies[i]->calculate(readings);
    Serial.print(strategies[i]->getName());
    Serial.print(": ");
    Serial.print(result, 1);
    Serial.println("%");
  }
  Serial.println("================================\n");
}

String getSaturationStatus(float saturation)
{
  if (saturation < 20)
    return "CLEAN - Good condition";
  if (saturation < 40)
    return "LIGHT - Minor contamination";
  if (saturation < 60)
    return "MODERATE - Needs attention";
  if (saturation < 80)
    return "HEAVY - Replace soon";
  return "SATURATED - Replace immediately";
}

void calibrateCleanFilter()
{
  Serial.println("\n=== CALIBRATING CLEAN FILTER ===");
  delay(2000);

  RGBColor rgb = colorSensor.read_rgb_color();
  SensorReadings readings(rgb);

  float totalRGB = rgb.red + rgb.green + rgb.blue;
  float blueRatio = (totalRGB > 0) ? (float)rgb.blue / totalRGB : 0;

  calibration.cleanChroma = readings.chroma;
  calibration.cleanValue = readings.hsv.value;
  calibration.cleanBlueRatio = blueRatio;
  calibration.cleanHSVSat = readings.hsv.saturation;

  Serial.println("Clean filter calibrated:");
  Serial.println("  Chroma: " + String(readings.chroma));
  Serial.println("  Value: " + String(readings.hsv.value));
  Serial.println("  HSV Saturation: " + String(readings.hsv.saturation));
  Serial.println("  Blue Ratio: " + String(blueRatio));
}

void calibrateDirtyFilter()
{
  Serial.println("\n=== CALIBRATING DIRTY FILTER ===");
  delay(2000);

  RGBColor rgb = colorSensor.read_rgb_color();
  SensorReadings readings(rgb);

  float totalRGB = rgb.red + rgb.green + rgb.blue;
  float blueRatio = (totalRGB > 0) ? (float)rgb.blue / totalRGB : 0;

  calibration.dirtyChroma = readings.chroma;
  calibration.dirtyValue = readings.hsv.value;
  calibration.dirtyBlueRatio = blueRatio;
  calibration.dirtyHSVSat = readings.hsv.saturation;

  Serial.println("Dirty filter calibrated:");
  Serial.println("  Chroma: " + String(readings.chroma));
  Serial.println("  Value: " + String(readings.hsv.value));
  Serial.println("  HSV Saturation: " + String(readings.hsv.saturation));
  Serial.println("  Blue Ratio: " + String(blueRatio));
}

void setup()
{
  // Initialize serial communication
  Serial.begin(115200);
  delay(100);

  // Initialize LED pin for consistent lighting
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);

  // Initialize the sensor
  colorSensor.begin();
  colorSensor.frequency_scaling(TCS3200_OFREQ_20P);

  Serial.println("Sensor initialized successfully!");
  Serial.println("LED enabled for consistent lighting");
  Serial.println();

  Serial.println("COMMANDS:");
  Serial.println("  'c' - Calibrate with CLEAN filter");
  Serial.println("  'd' - Calibrate with DIRTY filter");
  Serial.println("  's' - Switch calculation strategy");
  Serial.println("  'a' - Show all strategies comparison");
  Serial.println();
  Serial.print("Active Strategy: ");
  Serial.println(currentStrategy->getName());
  Serial.println();

  SuplaDevice.begin();
}

void loop()
{
  // Check for commands
  if (Serial.available() > 0)
  {
    char cmd = Serial.read();
    if (cmd == 'c' || cmd == 'C')
    {
      calibrateCleanFilter();
      return;
    }
    else if (cmd == 'd' || cmd == 'D')
    {
      calibrateDirtyFilter();
      return;
    }
    else if (cmd == 's' || cmd == 'S')
    {
      switchStrategy();
      return;
    }
    else if (cmd == 'a' || cmd == 'A')
    {
      showAllStrategies = !showAllStrategies;
      Serial.println(showAllStrategies ? "\nAll strategies comparison: ENABLED\n" : "\nAll strategies comparison: DISABLED\n");
      return;
    }
  }

  // Read sensor periodically
  if (millis() - lastSensorRead > SENSOR_READ_INTERVAL)
  {
    readSensorAndCalculate();

    // Display results
    Serial.println("--- Color Sensor Reading ---");
    Serial.println();
    Serial.print("RGB: (");
    Serial.print(lastRGB.red);
    Serial.print(", ");
    Serial.print(lastRGB.green);
    Serial.print(", ");
    Serial.print(lastRGB.blue);
    Serial.println(")");

    Serial.println("Hue: " + String(lastHSV.hue) +
                   ", Saturation: " + String(lastHSV.saturation) +
                   ", Value: " + String(lastHSV.value));

    Serial.println("Chroma: " + String(lastChroma));

    Serial.println("*** FILTER SATURATION ANALYSIS ***");
    Serial.print("Strategy: ");
    Serial.println(currentStrategy->getName());
    Serial.print("Saturation Level: ");
    Serial.print(lastSaturation, 1);
    Serial.println("%");
    Serial.print("Status: ");
    Serial.println(getSaturationStatus(lastSaturation));

    // Show all strategies comparison if enabled
    if (showAllStrategies)
    {
      SensorReadings readings(lastRGB);
      printAllStrategies(readings);
    }

    // Visual bar representation
    Serial.print("Progress: [");
    int bars = (int)(lastSaturation / 5);
    for (int i = 0; i < 20; i++)
    {
      Serial.print(i < bars ? "=" : " ");
    }
    Serial.println("]");
    Serial.println("===================================");
    Serial.println();
  }
  SuplaDevice.iterate();
}
