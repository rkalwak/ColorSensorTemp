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

#include "FilterSaturationSensor.h"
Supla::Eeprom eeprom;

#define STATUS_LED_GPIO 23
Supla::ESPWifi wifi;
Supla::LittleFsConfig configSupla;

Supla::Device::StatusLed statusLed(STATUS_LED_GPIO, true); // inverted state
Supla::EspWebServer suplaServer;

Supla::Html::DeviceInfo htmlDeviceInfo(&SuplaDevice);
Supla::Html::WifiParameters htmlWifi;
Supla::Html::ProtocolParameters htmlProto;
Supla::Html::StatusLedParameters htmlStatusLed;
// Calibration values for filter saturation monitoring
// NOTE: These values need to match YOUR actual sensor readings!
FilterCalibration calibration = {
    .cleanChroma = 0.5,     // Calibrated for white surface (low chroma)
    .dirtyChroma = 1.0,     // UPDATED: Dirty filter has high chroma (was 0.92, but sensor reads 0.96-0.97)
    .cleanValue = 0.80,     // Clean filter brightness
    .dirtyValue = 0.40,     // Dirty filter is darker
    .cleanBlueRatio = 0.34, // Clean has higher blue ratio
    .dirtyBlueRatio = 0.10, // Dirty has lower blue (more brown/yellow)
    .cleanHSVSat = 0.10,    // Clean is nearly white (low saturation)
    .dirtyHSVSat = 0.95     // UPDATED: Dirty is highly saturated (was 0.80, sensor reads ~0.90+)
};

// TCS3200 Color Sensor Pins
#define S0 2
#define S1 4
#define S2 16
#define S3 17
#define sensorOut 5
#define LED_PIN 18
// SUPLA sensor for filter saturation monitoring


FilterSaturationSensor *suplaSensor = nullptr;

// Create strategy instances
BrownScoreStrategy brownStrategy(&calibration);
HSVSaturationStrategy hsvStrategy(&calibration);
ChromaStrategy chromaStrategy(&calibration);
BrightnessStrategy brightnessStrategy(&calibration);
BlueRatioStrategy blueRatioStrategy(&calibration);
WeightedMixStrategy weightedMixStrategy(&calibration);

// Change if you want to try different one
FilterSaturationStrategy *currentStrategy = &brownStrategy;
int currentStrategyIndex = 1; // Start with Brown Score strategy
bool showAllStrategies = false;

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

  // Update SUPLA sensor strategy
  if (suplaSensor)
  {
    suplaSensor->setStrategy(currentStrategy);
  }

  Serial.println("\n*** STRATEGY SWITCHED ***");
  Serial.print("Active Strategy: ");
  Serial.println(currentStrategy->getName());
  Serial.println("************************\n");
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

  RGBColor rgb = suplaSensor->getLastRGB();
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

  RGBColor rgb = suplaSensor->getLastRGB();
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

  // Initialize the sensor

  Serial.println("COMMANDS:");
  Serial.println("  'c' - Calibrate with CLEAN filter");
  Serial.println("  'd' - Calibrate with DIRTY filter");
  Serial.println("  's' - Switch calculation strategy");
  Serial.println("  'a' - Show all strategies comparison");
  Serial.println("  'p' - Print current sensor values");
  Serial.println();
  Serial.print("Active Strategy: ");
  Serial.println(currentStrategy->getName());
  Serial.println();

  // Create SUPLA sensor with strategy and LED GPIO
  suplaSensor = new FilterSaturationSensor(
      S0, S1, S2, S3, sensorOut,
      &calibration,
      currentStrategy,
      LED_PIN);
  SuplaDevice.setInitialMode(Supla::InitialMode::StartInCfgMode);
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
    else if (cmd == 'p' || cmd == 'P')
    {
      // Print current sensor values
      if (suplaSensor) {
        suplaSensor->printValues();
        
        // Show all strategies if enabled
        if (showAllStrategies) {
          RGBColor rgb = suplaSensor->getLastRGB();
          SensorReadings readings(rgb);
          printAllStrategies(readings);
        }
      }
      return;
    }
  }

  SuplaDevice.iterate();
}
