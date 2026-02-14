# HW-531 (TCS3200) Color Sensor Wiring

## Sensor Pinout
The HW-531 module uses the TCS3200 color sensor chip with the following pins:
- **VCC** - Power supply (3.3V or 5V)
- **GND** - Ground
- **S0** - Output frequency scaling selection
- **S1** - Output frequency scaling selection
- **S2** - Color filter selection
- **S3** - Color filter selection
- **OUT** - Output frequency (PWM signal)
- **LED** - LED control (optional, can be connected to VCC)

## Wiring to ESP32 Dev Kit C

| HW-531 Pin | ESP32 Pin | Description |
|------------|-----------|-------------|
| VCC        | 3.3V      | Power supply |
| GND        | GND       | Ground |
| S0         | GPIO 2    | Frequency scale (high bit) |
| S1         | GPIO 4    | Frequency scale (low bit) |
| S2         | GPIO 16   | Color filter (high bit) |
| S3         | GPIO 17   | Color filter (low bit) |
| OUT        | GPIO 5    | Frequency output |
| LED        | GPIO 18   | LED control for consistent lighting |

## Notes
- The sensor works with both 3.3V and 5V, but ESP32 uses 3.3V logic
- S0 and S1 control the output frequency scaling:
  - S0=LOW, S1=LOW: Power down
  - S0=LOW, S1=HIGH: 2% scaling
  - S0=HIGH, S1=LOW: 20% scaling
  - S0=HIGH, S1=HIGH: 100% scaling (used in this project)
- S2 and S3 select the color filter:
  - S2=LOW, S3=LOW: Red filter
  - S2=LOW, S3=HIGH: Blue filter
  - S2=HIGH, S3=LOW: Clear (no filter)
  - S2=HIGH, S3=HIGH: Green filter
- The OUT pin provides a square wave frequency proportional to light intensity
- Higher frequency = more light detected of the selected color
