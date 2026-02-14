#include <Arduino.h>
#include <TCS3200.h>
#include "FilterSaturationStrategy.h"

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

// Current active strategy (default: WeightedMix)
FilterSaturationStrategy* currentStrategy = &weightedMixStrategy;
int currentStrategyIndex = 0;
bool showAllStrategies = false;

// Array of all available strategies
FilterSaturationStrategy* strategies[] = {
  &weightedMixStrategy,
  &brownStrategy,
  &hsvStrategy,
  &chromaStrategy,
  &brightnessStrategy,
  &blueRatioStrategy
};
const int strategyCount = 6;

void switchStrategy() {
  currentStrategyIndex = (currentStrategyIndex + 1) % strategyCount;
  currentStrategy = strategies[currentStrategyIndex];
  Serial.println("\n*** STRATEGY SWITCHED ***");
  Serial.print("Active Strategy: ");
  Serial.println(currentStrategy->getName());
  Serial.println("************************\n");
}

void printAllStrategies(const SensorReadings& readings) {
  Serial.println("\n=== ALL STRATEGIES COMPARISON ===");
  for (int i = 0; i < strategyCount; i++) {
    float result = strategies[i]->calculate(readings);
    Serial.print(strategies[i]->getName());
    Serial.print(": ");
    Serial.print(result, 1);
    Serial.println("%");
  }
  Serial.println("================================\n");
}

String getSaturationStatus(float saturation) {
  if (saturation < 20) return "CLEAN - Good condition";
  if (saturation < 40) return "LIGHT - Minor contamination";
  if (saturation < 60) return "MODERATE - Needs attention";
  if (saturation < 80) return "HEAVY - Replace soon";
  return "SATURATED - Replace immediately";
}

void calibrateCleanFilter() {
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

void calibrateDirtyFilter() {
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

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  Serial.println("TCS3200 Color Sensor - Color Density Reader");
  Serial.println("============================================");
  
  // Initialize LED pin for consistent lighting
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);  // Turn on LED for consistent lighting
  
  // Initialize the sensor
  colorSensor.begin();
  
  // Set frequency scaling to 20% for optimal performance
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
}

void loop() {
  // Check for commands
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'c' || cmd == 'C') {
      calibrateCleanFilter();
      return;
    } else if (cmd == 'd' || cmd == 'D') {
      calibrateDirtyFilter();
      return;
    } else if (cmd == 's' || cmd == 'S') {
      switchStrategy();
      return;
    } else if (cmd == 'a' || cmd == 'A') {
      showAllStrategies = !showAllStrategies;
      Serial.println(showAllStrategies ? "\nAll strategies comparison: ENABLED\n" : "\nAll strategies comparison: DISABLED\n");
      return;
    }
  }
  
  // Read sensor ONCE and create readings object
  RGBColor rgb = colorSensor.read_rgb_color();
  SensorReadings readings(rgb);
  
  // Display results on Serial Monitor
  Serial.println("--- Color Sensor Reading ---");
  Serial.println();
  Serial.print("RGB: (");
  Serial.print(rgb.red);
  Serial.print(", ");
  Serial.print(rgb.green);
  Serial.print(", ");
  Serial.print(rgb.blue);
  Serial.println(")");
  
  // Display calculated values from the single sensor reading
  Serial.println("Hue: " + String(readings.hsv.hue) +
    ", Saturation: " + String(readings.hsv.saturation) +
    ", Value: " + String(readings.hsv.value));
  
  Serial.println("Chroma: " + String(readings.chroma));
  
  // === FILTER SATURATION ANALYSIS ===
  // Use current strategy to calculate saturation
  float filterSaturation = currentStrategy->calculate(readings);
  
  Serial.println("*** FILTER SATURATION ANALYSIS ***");
  Serial.print("Strategy: ");
  Serial.println(currentStrategy->getName());
  Serial.print("Saturation Level: ");
  Serial.print(filterSaturation, 1);
  Serial.println("%");
  Serial.print("Status: ");
  Serial.println(getSaturationStatus(filterSaturation));
  
  // Show all strategies comparison if enabled
  if (showAllStrategies) {
    printAllStrategies(readings);
  }
  
  // Visual bar representation
  Serial.print("Progress: [");
  int bars = (int)(filterSaturation / 5);
  for (int i = 0; i < 20; i++) {
    Serial.print(i < bars ? "=" : " ");
  }
  Serial.println("]");
  Serial.println("===================================");
  Serial.println();
  
  // Wait before next reading
  delay(5000);
}
