# Case Assembly Guide

## Required Parts

- [ ] Waveshare ESP32-S3-Touch-LCD-1.46C board
- [ ] 3D printed case (files: `case/` folder or link to STL)
- [ ] USB-C cable (for power/programming)
- [ ] (Optional) 3.7V LiPo battery for portable operation

## Board Documentation

- **Official Waveshare Wiki**: [https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46](https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46)
- **Schematic PDF**: Available on Waveshare wiki under "Resources"
- **Dimensions**: 52mm × 52mm (verify from schematic)

## Assembly Steps

### Step 1: Test Board Before Assembly

Always flash and test the board BEFORE mounting in the case:

1. Upload FlightRadar.ino
2. Verify display shows "FlightRadar" splash
3. Test touch responsiveness
4. Connect to WiFi

### Step 2: Case Orientation

- **Display faces up** (round LCD visible)
- **USB-C port** should align with case opening
- **Boot button** should be accessible (for recovery)

### Step 3: Mounting

- Use M2.5 screws (4 locations)
- Do not overtighten - plastic threads strip easily
- Ensure no pressure on LCD flex cable

### Step 4: Cable Management

- Route USB-C cable through strain relief
- Battery (if used) fits in compartment behind board

## Troubleshooting Assembly Issues

| Issue | Solution |
| ------- | ---------- |
| Display doesn't fit | Check case orientation - board only fits one way |
| Can't access BOOT button | Use paperclip through case hole, or disassemble |
| Case blocks WiFi | Ensure antenna area (top of board) is not enclosed in metal |

## 3D Print Settings (if applicable)

- Layer height: 0.2mm
- Infill: 20%
- Supports: Yes (for USB-C opening)
- Material: PLA or PETG
