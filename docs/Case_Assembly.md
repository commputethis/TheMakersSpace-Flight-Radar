# Case Assembly Guide

## Required Parts

- [ ] Waveshare ESP32-S3-Touch-LCD-1.46C board
- [ ] 3D printed case (2 parts): [Flight Radar Enclosure.3mf](../3mf/Flight%20Radar%20Enclosure.3mf) in the [/3mf](../3mf/) folder
- [ ] USB-C cable (for power/programming)
- [ ] (Optional) 3.7V LiPo battery for portable operation

## Board Documentation

- **Official Waveshare Wiki**: [https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46)
- **Schematic PDF**: [Available on Waveshare wiki under "Resources"](https://files.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.46/ESP32-S3-Touch-LCD-1.46.pdf)
- **Dimensions**: [~49mm × ~49mm](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46#dimensions)

## Assembly Steps

### Step 1: Test Board Before Assembly

Always flash and test the board BEFORE mounting in the case:

1. Upload FlightRadar.ino
2. Verify display shows "FlightRadar" splash
3. Test touch responsiveness
4. Connect to WiFi

### Step 2: Case Orientation

- **Display faces up/out** (round LCD visible)
- **USB-C port** should align with case opening

### Step 3: Mounting

- Use M2 x 4mm screws (3 locations)
- Do not overtighten

### Step 4: Cable Management

- Route USB-C cable through enclosure back
- Battery (if used) fits in compartment behind board

## Troubleshooting Assembly Issues

| Issue | Solution |
| ------- | ---------- |
| Screw holes don't line up | Check board orientation - board only fits one way |
| Back of case isn't staying on | Make sure to push it until it clicks into place

## 3D Print Settings

- Layer height: 0.2mm
- Infill: 15%
- Supports: Built into the design, so keep them turned off
- Material: PLA or PETG
