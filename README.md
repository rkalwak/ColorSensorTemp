# Color Sensor Project

PlatformIO project for ESP32 Dev Kit C with HW-531 (TCS3200) color sensor.

## Description
This project reads color density values from the TCS3200 color sensor and displays RGB frequency values, normalized RGB values (0-255), and the dominant color on the serial monitor.

## Hardware Required
- ESP32 Dev Kit C
- HW-531 Color Sensor Module (TCS3200)
- Jumper wires
- USB cable for programming and serial monitoring

## Wiring
See [WIRING.md](WIRING.md) for detailed wiring instructions.

## Building and Uploading
1. Open this project in PlatformIO (VS Code with PlatformIO extension)
2. Build the project: `platformio run`
3. Upload to ESP32: `platformio run --target upload`
4. Open serial monitor: `platformio device monitor`

## Usage
1. Upload the program to your ESP32
2. Open the serial monitor at 115200 baud
3. Place different colored objects near the sensor
4. Observe the color density readings and RGB values in the serial monitor

## Output Format
The program displays:
- Raw frequency values for Red, Green, Blue, and Clear channels (in Hz)
- Normalized RGB values (0-255 scale)
- Dominant color detection (RED, GREEN, BLUE, or MIXED/NEUTRAL)

## Notes
- The sensor reads frequency values - lower frequency indicates more color absorption (higher density)
- You may need to calibrate the mapping values in the code based on your lighting conditions
- The sensor works best with good ambient lighting or using the onboard LED
- Readings are taken every 2 seconds

## Troubleshooting
- If you get all zeros, check your wiring connections
- If values seem inverted, adjust the map() function parameters in main.cpp
- Make sure the sensor has adequate lighting (LED pin connected to 3.3V)
