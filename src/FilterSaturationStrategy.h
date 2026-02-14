#ifndef FILTER_SATURATION_STRATEGY_H
#define FILTER_SATURATION_STRATEGY_H

#include <Arduino.h>
#include <TCS3200.h>

// Structure to hold all sensor readings in one place
struct SensorReadings {
  RGBColor rgb;
  HSVColor hsv;
  float chroma;
  
  SensorReadings(RGBColor _rgb) : rgb(_rgb) {
    hsv = TCS3200::calculate_hsv(rgb);
    chroma = TCS3200::calculate_chroma(rgb);
  }
};

// Calibration data structure
struct FilterCalibration {
  float cleanChroma;
  float dirtyChroma;
  float cleanValue;
  float dirtyValue;
  float cleanBlueRatio;
  float dirtyBlueRatio;
  float cleanHSVSat;
  float dirtyHSVSat;
};

// Base class for filter saturation calculation strategies
class FilterSaturationStrategy {
protected:
  FilterCalibration* calibration;
  
public:
  FilterSaturationStrategy(FilterCalibration* cal) : calibration(cal) {}
  virtual ~FilterSaturationStrategy() {}
  
  // Pure virtual function - must be implemented by derived classes
  virtual float calculate(const SensorReadings& readings) = 0;
  virtual String getName() = 0;
};

// Strategy 1: Brown/Yellow Score Based
class BrownScoreStrategy : public FilterSaturationStrategy {
public:
  BrownScoreStrategy(FilterCalibration* cal) : FilterSaturationStrategy(cal) {}
  
  float calculate(const SensorReadings& readings) override {
    float brownScore = 0.0;
    if (readings.rgb.red > readings.rgb.blue && readings.rgb.green > readings.rgb.blue) {
      brownScore = ((readings.rgb.red + readings.rgb.green) / 2.0 - readings.rgb.blue) / 255.0 * 100.0;
      brownScore = constrain(brownScore, 0.0, 100.0);
    }
    return brownScore;
  }
  
  String getName() override {
    return "Brown Score";
  }
};

// Strategy 2: HSV Saturation Based
class HSVSaturationStrategy : public FilterSaturationStrategy {
public:
  HSVSaturationStrategy(FilterCalibration* cal) : FilterSaturationStrategy(cal) {}
  
  float calculate(const SensorReadings& readings) override {
    float hsvSaturationScore = constrain(
      (readings.hsv.saturation - calibration->cleanHSVSat) / 
      (calibration->dirtyHSVSat - calibration->cleanHSVSat) * 100.0,
      0.0, 100.0
    );
    return hsvSaturationScore;
  }
  
  String getName() override {
    return "HSV Saturation";
  }
};

// Strategy 3: Chroma Based
class ChromaStrategy : public FilterSaturationStrategy {
public:
  ChromaStrategy(FilterCalibration* cal) : FilterSaturationStrategy(cal) {}
  
  float calculate(const SensorReadings& readings) override {
    float chromaSaturation = constrain(
      (readings.chroma - calibration->cleanChroma) / 
      (calibration->dirtyChroma - calibration->cleanChroma) * 100.0,
      0.0, 100.0
    );
    return chromaSaturation;
  }
  
  String getName() override {
    return "Chroma";
  }
};

// Strategy 4: Brightness Based
class BrightnessStrategy : public FilterSaturationStrategy {
public:
  BrightnessStrategy(FilterCalibration* cal) : FilterSaturationStrategy(cal) {}
  
  float calculate(const SensorReadings& readings) override {
    float brightnessSaturation = constrain(
      (calibration->cleanValue - readings.hsv.value) / 
      (calibration->cleanValue - calibration->dirtyValue) * 100.0,
      0.0, 100.0
    );
    return brightnessSaturation;
  }
  
  String getName() override {
    return "Brightness";
  }
};

// Strategy 5: Blue Ratio Based
class BlueRatioStrategy : public FilterSaturationStrategy {
public:
  BlueRatioStrategy(FilterCalibration* cal) : FilterSaturationStrategy(cal) {}
  
  float calculate(const SensorReadings& readings) override {
    float totalRGB = readings.rgb.red + readings.rgb.green + readings.rgb.blue;
    float blueRatio = (totalRGB > 0) ? (float)readings.rgb.blue / totalRGB : 0;
    float blueRatioSaturation = constrain(
      (calibration->cleanBlueRatio - blueRatio) / 
      (calibration->cleanBlueRatio - calibration->dirtyBlueRatio) * 100.0,
      0.0, 100.0
    );
    return blueRatioSaturation;
  }
  
  String getName() override {
    return "Blue Ratio";
  }
};

// Strategy 6: Weighted Mix of All Methods
class WeightedMixStrategy : public FilterSaturationStrategy {
private:
  ChromaStrategy chromaStrategy;
  BrightnessStrategy brightnessStrategy;
  BlueRatioStrategy blueRatioStrategy;
  HSVSaturationStrategy hsvSaturationStrategy;
  BrownScoreStrategy brownScoreStrategy;
  
  // Configurable weights
  float chromaWeight;
  float brightnessWeight;
  float blueRatioWeight;
  float hsvSatWeight;
  float brownWeight;
  
public:
  WeightedMixStrategy(FilterCalibration* cal,
                      float chroma_w = 0.25,
                      float brightness_w = 0.20,
                      float blueRatio_w = 0.20,
                      float hsvSat_w = 0.20,
                      float brown_w = 0.15)
    : FilterSaturationStrategy(cal),
      chromaStrategy(cal),
      brightnessStrategy(cal),
      blueRatioStrategy(cal),
      hsvSaturationStrategy(cal),
      brownScoreStrategy(cal),
      chromaWeight(chroma_w),
      brightnessWeight(brightness_w),
      blueRatioWeight(blueRatio_w),
      hsvSatWeight(hsvSat_w),
      brownWeight(brown_w) {}
  
  float calculate(const SensorReadings& readings) override {
    float saturation = (
      chromaStrategy.calculate(readings) * chromaWeight +
      brightnessStrategy.calculate(readings) * brightnessWeight +
      blueRatioStrategy.calculate(readings) * blueRatioWeight +
      hsvSaturationStrategy.calculate(readings) * hsvSatWeight +
      brownScoreStrategy.calculate(readings) * brownWeight
    );
    
    return constrain(saturation, 0.0, 100.0);
  }
  
  String getName() override {
    return "Weighted Mix";
  }
  
  // Allow adjusting weights at runtime
  void setWeights(float chroma_w, float brightness_w, float blueRatio_w, 
                  float hsvSat_w, float brown_w) {
    chromaWeight = chroma_w;
    brightnessWeight = brightness_w;
    blueRatioWeight = blueRatio_w;
    hsvSatWeight = hsvSat_w;
    brownWeight = brown_w;
  }
};

#endif // FILTER_SATURATION_STRATEGY_H
