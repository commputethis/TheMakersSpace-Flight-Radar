# Build & Flash Guide

## Quick Start (Experienced Users)

1. Clone/download: [https://github.com/commputethis/TheMakersSpace-Flight-Radar](https://github.com/commputethis/TheMakersSpace-Flight-Radar)
2. Open `src/FlightRadar/FlightRadar.ino` in Arduino IDE
3. Install libraries: Arduino_GFX_Library (1.5.7+), ArduinoJson (7.x), WiFiManager (2.0.x), SensorLib
4. Board: Wavesheare ESP32-S3-Touch-LCD-1.46, PSRAM: Enabled
5. Upload, then proceed to WiFi setup

## Detailed Instructions

### Part 1: Download Code

[Step-by-step for GitHub ZIP download]

### Part 2: Arduino IDE Setup

[Library installation with screenshots]

### Part 3: Board Configuration

[Tools menu settings - emphasize PSRAM!]

### Part 4: First Upload

[Upload process, BOOT button technique]

### Part 5: Verification

#### Serial Monitor (115200 baud)

You should see on boot:

``` serial
[Mem] PSRAM size: 8388608 bytes
[Mem] Free PSRAM: 8388608 bytes
[Boot] FlightRadar starting…
[Boot] Waveshare ESP32-S3-Touch-LCD-1.46C
[Touch] SPD2010 reset complete
[IMU] QMI8658 ready
[FS] LittleFS ready — 1420 KB free
[LCD] panel + canvas ready
[WiFi] connected — IP=192.168.1.xxx
[mDNS] flightradar.local
[OTA] ready
[HTTP] web server up on port 80
[Boot] ready
```

If you see `[LCD] canvas->begin() FAILED`, check that PSRAM is enabled in Tools menu.

## Troubleshooting

[Common issues table]

## Adding Different Hardware

[Brief explanation of board_config.h for advanced users]