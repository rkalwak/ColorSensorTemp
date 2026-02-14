# ESP32 Filter Saturation Monitor with SUPLA

IoT-enabled filter saturation monitoring system using ESP32, TCS3200 color sensor, and SUPLA cloud platform. Detects filter contamination through color analysis with multiple calculation strategies.

## Features
- **SUPLA IoT Integration**: Cloud connectivity via GeneralPurposeMeasurement channel
- **Strategy Pattern**: 6 different saturation calculation algorithms
- **Web Configuration Portal**: WiFi setup, device configuration, and diagnostics
- **Real-time Calibration**: Serial commands for clean/dirty filter calibration
- **LED Control**: Consistent lighting via GPIO-controlled sensor LED
- **Flexible Calculation**: Runtime strategy switching for optimal detection
- **Visual Feedback**: Status LED with web-configurable behavior
- **Persistent Storage**: LittleFS configuration and EEPROM settings

## Hardware
- **ESP32 Dev Module** (240MHz dual-core, 320KB RAM, 4MB Flash)
- **HW-531 (TCS3200)** Color Sensor Module
- **Status LED** on GPIO 23 (optional)

### Pin Configuration
| Component | GPIO | Function |
|-----------|------|----------|
| S0 | 2 | TCS3200 frequency scaling (high bit) |
| S1 | 4 | TCS3200 frequency scaling (low bit) |
| S2 | 16 | TCS3200 color filter (high bit) |
| S3 | 17 | TCS3200 color filter (low bit) |
| OUT | 5 | TCS3200 frequency output (PWM) |
| LED | 18 | Sensor LED control (consistent lighting) |
| Status LED | 23 | SUPLA status indicator (inverted) |

See [WIRING.md](WIRING.md) for detailed wiring diagrams.

## Saturation Calculation Strategies

The system implements 6 different algorithms for calculating filter saturation:

1. **Brown Score Strategy** (default)
   - Detects brown/yellow contamination
   - Formula: `((R+G)/2 - B) / 255 * 100`
   - Returns 0 if blue dominates (B > R or B > G)
   - Best for: Brown/yellow filter contamination

2. **HSV Saturation Strategy**
   - Maps HSV saturation between clean/dirty calibration
   - Uses color saturation intensity
   - Best for: General color intensity changes

3. **Chroma Strategy**
   - Maps chroma (color purity) between calibration points
   - Measures color "vividness"
   - Best for: High chroma filters (0.9+) or blue-dominant readings

4. **Brightness Strategy**
   - Inverse mapping of HSV value (darker = dirtier)
   - Best for: Filters that darken with contamination

5. **Blue Ratio Strategy**
   - Maps blue ratio between calibration points
   - Best for: Filters where blue component changes significantly

6. **Weighted Mix Strategy**
   - Combines all methods with configurable weights
   - Best for: Comprehensive multi-factor analysis

## Software Dependencies
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
lib_deps = 
    nthnn/TCS3200@^1.0.3
    supla/SuplaDevice@^2.4.2
monitor_speed = 115200
```

## Serial Commands

Connect via serial monitor at **115200 baud** and use these commands:

| Command | Description |
|---------|-------------|
| `c` | Calibrate with **CLEAN** filter (captures clean reference values) |
| `d` | Calibrate with **DIRTY** filter (captures dirty reference values) |
| `s` | Switch calculation strategy (cycles through all 6) |
| `a` | Toggle all-strategies comparison mode |
| `p` | Print current sensor values and saturation percentage |

### Calibration Workflow

1. **Prepare Clean Filter**:
   - Place clean white filter under sensor
   - Wait 2 seconds for stable reading
   - Send `c` command
   - System captures: chroma, HSV values, blue ratio

2. **Prepare Dirty Filter**:
   - Place known dirty/contaminated filter under sensor
   - Wait 2 seconds
   - Send `d` command
   - System captures dirty reference values

3. **Test Strategies**:
   - Send `a` to enable comparison mode
   - Send `p` to see all 6 strategies' outputs
   - Send `s` to switch to best-performing strategy

## Web Configuration Portal

On first boot, device starts in **Configuration Mode**:

1. Connect to WiFi AP: `SUPLA-ESP-XXXXXX`
2. Navigate to `192.168.4.1` in browser
3. Configure:
   - WiFi credentials
   - SUPLA server and email
   - Status LED behavior
   - Device name

Access portal anytime by holding config button during startup.

## SUPLA Cloud Integration

The `FilterSaturationSensor` inherits from `Supla::Sensor::GeneralPurposeMeasurement`:

- **Auto-reporting**: Sensor reads every 1 second
- **Value range**: 0-100% saturation
- **Channel type**: General Purpose Measurement
- **Cloud visibility**: View in SUPLA mobile/web app

Sensor lifecycle:
```cpp
onInit()      // Initializes TCS3200, sets frequency scaling
getValue()    // Called by SUPLA every 1s, returns saturation %
printValues() // User-triggered detailed output
```

## Troubleshooting

### Issue: Saturation always shows 0%
**Cause**: Brown Score Strategy expects brown/yellow filters (R+G > B), but sensor reads blue-dominant colors (B highest).

**Solution**:
1. Send `a` command to enable all-strategies comparison
2. Send `p` to see which strategies give reasonable values
3. If sensor reads blue (B > R and B > G):
   - Switch to **Chroma Strategy** (`s` until "Chroma Strategy")
   - Or use **HSV Saturation Strategy**
4. Recalibrate with new strategy: place clean filter, send `c`, place dirty filter, send `d`

### Issue: Readings unstable
**Causes**:
- Inconsistent lighting (LED not connected)
- Filter not flat against sensor
- Ambient light interference

**Solutions**:
- Verify LED pin 18 connection
- Ensure filter is 5-10mm from sensor surface
- Use enclosure to block ambient light

### Issue: All strategies return similar values
**Cause**: Clean and dirty calibration values are too similar.

**Solution**:
- Use more contrasting samples (pristine white vs heavily contaminated)
- Verify 2-second stabilization before calibration commands

## Building and Uploading

```bash
# Build project
pio run

# Upload to ESP32
pio run --target upload

# Monitor serial output
pio device monitor --baud 115200
```

## Project Structure
```
Color sensor/
├── src/
│   ├── main.cpp                      # Main application with SUPLA setup
│   ├── FilterSaturationSensor.h      # SUPLA sensor implementation
│   └── FilterSaturationStrategy.h    # Strategy pattern (6 algorithms)
├── platformio.ini                    # Build configuration
├── README.md                         # This file
└── WIRING.md                         # Detailed wiring guide
```

## Debug Output

Enable verbose logging by monitoring serial output during operation:

```
*** SENSOR READ COMPLETE ***
RGB(138, 15, 236)  Chroma: 0.96  Strategy: Brown Score  Result: 0.00%
Status: CLEAN - Good condition
[==========                              ] 0.0%
```

Key metrics shown:
- **RGB values**: Raw color readings (0-255)
- **Chroma**: Color purity (0.0-1.0)
- **Strategy**: Active calculation method
- **Result**: Saturation percentage
- **Status**: Human-readable filter condition
- **Progress bar**: Visual saturation indicator

## Advanced Configuration

### Adjusting Strategy Weights (WeightedMix)
Edit in code:
```cpp
WeightedMixStrategy weightedMixStrategy(&calibration);
// Default weights: 30% brown, 25% HSV, 20% chroma, 15% brightness, 10% blue ratio
```

### Changing Default Strategy
In `main.cpp`:
```cpp
FilterSaturationStrategy *currentStrategy = &chromaStrategy; // Change here
int currentStrategyIndex = 3; // Update index (0=Weighted, 1=Brown, 2=HSV, 3=Chroma, etc.)
```

## References
- **TCS3200 Library**: [nthnn/TCS3200](https://github.com/nthnn/TCS3200)
- **SUPLA Platform**: [supla.org](https://www.supla.org/)
- **Hardware**: See [WIRING.md](WIRING.md) for connections
