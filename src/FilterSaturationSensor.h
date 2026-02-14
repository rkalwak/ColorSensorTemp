#ifndef FILTER_SATURATION_SENSOR_H
#define FILTER_SATURATION_SENSOR_H

#include <TCS3200.h>
#include "FilterSaturationStrategy.h"
#include <supla/sensor/general_purpose_measurement.h>

class FilterSaturationSensor : public Supla::Sensor::GeneralPurposeMeasurement
{
public:
  FilterSaturationSensor(
      uint8_t s0_pin, uint8_t s1_pin, uint8_t s2_pin, uint8_t s3_pin, uint8_t out_pin,
      FilterCalibration *calib,
      FilterSaturationStrategy *strategy,
      int ledGpio) : GeneralPurposeMeasurement(),
                     colorSensor(s0_pin, s1_pin, s2_pin, s3_pin, out_pin),
                     calibration(calib),
                     currentStrategy(strategy),
                     ledPin(ledGpio),
                     value(0.0),
                     lastRGB({0, 0, 0}),
                     lastHSV({0, 0, 0}),
                     lastChroma(0.0)
  {
    setInitialCaption("Filter Saturation");
    setUnitAfterValue("%");
    setValuePrecision(2);
    // Initialize LED for consistent lighting
    if (ledPin >= 0)
    {
      pinMode(ledPin, OUTPUT);
      digitalWrite(ledPin, HIGH);
    }
  }

  void onInit() override
  {
    // Must call begin() before frequency_scaling()!
    colorSensor.begin();
    colorSensor.frequency_scaling(TCS3200_OFREQ_20P);
    GeneralPurposeMeasurement::onInit();
  }

  // Override getValue() - called automatically by base class
  double getValue() override
  {
    if (!currentStrategy) {
        Serial.println("No strategy set! Returning 0.");
        return 0.0;
    }
    // Read sensor
    lastRGB = colorSensor.read_rgb_color();
    SensorReadings readings(lastRGB);

    // Store calculated values for external access
    lastHSV = readings.hsv;
    lastChroma = readings.chroma;

    // Calculate saturation using current strategy
    value = currentStrategy->calculate(readings);
    printValues();
    return value;
  }

  void printValues() const
  {
    Serial.println("--- Color Sensor Reading ---");
    Serial.print("RGB: (");
    Serial.print(lastRGB.red);
    Serial.print(", ");
    Serial.print(lastRGB.green);
    Serial.print(", ");
    Serial.print(lastRGB.blue);
    Serial.println(")");
    
    Serial.print("HSV: (H:");
    Serial.print(lastHSV.hue);
    Serial.print(", S:");
    Serial.print(lastHSV.saturation);
    Serial.print(", V:");
    Serial.print(lastHSV.value);
    Serial.println(")");
    
    Serial.print("Chroma: ");
    Serial.println(lastChroma);
    
    Serial.println("*** FILTER SATURATION ANALYSIS ***");
    Serial.print("Strategy: ");
    Serial.println(currentStrategy ? currentStrategy->getName() : "None");
    Serial.print("Saturation Level: ");
    Serial.print(value, 1);
    Serial.println("%");

    Serial.print("Progress: [");
    int bars = (int)(value / 5);
    for (int i = 0; i < 20; i++)
    {
      Serial.print(i < bars ? "=" : " ");
    }
    Serial.println("]");
    Serial.println("===================================");
    Serial.println();
  }

  void setStrategy(FilterSaturationStrategy *strategy)
  {
    if (strategy)
    {
      currentStrategy = strategy;
    }
  }

  FilterSaturationStrategy *getStrategy() const
  {
    return currentStrategy;
  }

  RGBColor getLastRGB() const
  {
    return lastRGB;
  }
  
  HSVColor getLastHSV() const
  {
    return lastHSV;
  }
  
  float getLastChroma() const
  {
    return lastChroma;
  }
  
  float getLastValue() const
  {
    return value;
  }

private:
  TCS3200 colorSensor;
  FilterCalibration *calibration;
  FilterSaturationStrategy *currentStrategy;
  int ledPin;
  mutable float value;
  mutable RGBColor lastRGB;
  mutable HSVColor lastHSV;
  mutable float lastChroma;
};

#endif // FILTER_SATURATION_SENSOR_H