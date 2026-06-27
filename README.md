# TheMakersSpace Flight Radar

A portable ESP32-based flight tracking display that connects to your ADS-B receiver and shows nearby aircraft on a touchscreen display. Perfect for aviation enthusiasts, makerspaces, or anyone who wants a dedicated aircraft monitoring station.

![Flight Radar Display](docs/images/flight-radar.jpg)

## Features

- **Real-time Aircraft Tracking**: Displays nearby aircraft with callsign, distance, altitude, and speed
- **Multiple Unit Systems**: Choose between Native (knots/nautical miles), Imperial (mph/miles), or Metric (km/h/kilometers)
- **Interactive Touchscreen**: Tap aircraft to view detailed flight information
- **Auto-Rotation**: Built-in IMU detects orientation changes and rotates the display automatically
- **Shake-to-Wake**: Motion detection wakes the display from sleep mode
- **Audio Alerts**: Optional audio notifications for nearby aircraft
- **Demo Mode**: Simulated aircraft for testing and demonstration
- **Web Configuration**: Easy setup via built-in web portal
- **Daylight Saving Time**: Automatic DST adjustment option
- **Adjustable Range**: Monitor aircraft from 10 to 100+ miles away

## Hardware Requirements

- **ESP32-S3 DevKit** (or compatible ESP32 with sufficient RAM)
- **2.8" TFT LCD Display** with touchscreen (ILI9341 or compatible)
- **QMI8658 IMU** (6-axis accelerometer/gyro) - optional but recommended
- **Audio Amplifier + Speaker** - optional for audio alerts
- **WiFi Connection** to your ADS-B receiver

### Pin Connections

| Function | GPIO Pin |
| ---------- | ---------- |
| TFT CS | 10 |
| TFT DC | 9 |
| TFT RST | 14 |
| TFT MOSI | 11 |
| TFT SCK | 12 |
| TFT MISO | 13 |
| Touch CS | 6 |
| Touch IRQ | 5 |
| IMU SDA | 17 |
| IMU SCL | 18 |
| Audio Out | 8 |
| Backlight | 15 |

## Software Setup

### Prerequisites

- Arduino IDE or PlatformIO
- ESP32 board support package
- Required libraries:
  - Arduino_GFX
  - ArduinoJson
  - QMI8658 (for IMU support)
  - Preferences (built-in)

### Installation

1. Clone this repository:

   ```bash
   git clone https://github.com/commputethis/TheMakersSpace-Flight-Radar.git
   ```

2. Open the project in Arduino IDE or PlatformIO

3. Install required libraries via Library Manager

4. Configure your settings in `config.h` (optional):
   - Default WiFi credentials
   - ADS-B receiver URL
   - Display pins

5. Build and upload to your ESP32

## Configuration

### First Boot

On first boot, the device creates a WiFi access point named **"FlightRadar-Setup"**. Connect to it and navigate to `192.168.4.1` to configure:

- **WiFi Credentials**: Connect to your network
- **ADS-B Receiver URL**: IP address of your dump1090/readsb instance
- **Timezone**: UTC offset for local time display
- **DST**: Enable daylight saving time observation
- **Units**: Native (nm/kt), Miles (mi/mph), or Metric (km/km/h)
- **Range**: Detection radius (10-100+ miles)
- **Audio**: Enable/disable audio alerts

### ADS-B Receiver Setup

This project requires a running ADS-B receiver with a JSON API endpoint. Compatible receivers include:

- **dump1090-fa** (FlightAware)
- **readsb** (Mode S decoder)
- **tar1090**

Ensure your receiver exposes the `/data/aircraft.json` endpoint and is accessible on your network.

## Usage

### Main Display

The main screen shows a scrollable list of nearby aircraft with:

- **Callsign**: Flight identifier (or ICAO hex if unknown)
- **Distance**: From your location
- **Altitude**: In feet
- **Speed**:    In knots, mph, or km/h depending on settings

### Aircraft Details

Tap any aircraft to view detailed information:

- Full flight details
- Aircraft type (if known)
- Heading and vertical speed
- Last seen timestamp

### Settings Menu

Access the settings menu by long pressing on the screen:

- **Theme**: Toggle between Green Phosphor and Amber CRT modes
- **Range**: Change detection radius
- **Units**: Switch between Native/Miles/Metric
- **Time**: Switch clock between 12h and 24h format
- **Audio**: Toggle audio alerts
- **Demo**: Enable demo mode with simulated aircraft
- **Brightness**: Adjust screen backlight
- **Log**: Enable SD card logging (if equipped)
- **DST**: Toggle daylight saving time

### Motion Controls

- **Shake**: Wake display from sleep
- **Rotate**: Auto-rotate display orientation (if IMU enabled)

## Troubleshooting

### Display stays blank

- Check backlight pin connection
- Verify TFT driver matches your display (ILI9341, ST7789, etc.)

### No aircraft showing

- Verify ADS-B receiver URL in settings
- Try increasing range in settings

### Crash/Restart loop

- May indicate insufficient RAM - try reducing `ADSB_MAX_AIRCRAFT` in config
- Check serial monitor for errors
- Ensure power supply can deliver sufficient current (500mA+)

### Touch not working

- Verify touch controller matches your display (XPT2046 common for ILI9341)
- Check touch CS and IRQ pin connections

## Advanced Configuration

### Custom Aircraft Database

Edit `aircraft_types.h` to add more types of aircraft

### API Endpoints (future add)

The web interface exposes these endpoints:

- `/` - Main configuration portal
- `/api/aircraft` - Current aircraft list (JSON)
- `/api/stats` - Statistics (JSON)
- `/api/toggle` - Toggle settings remotely

## Contributing

Contributions welcome! Please submit pull requests or open issues for:

- Bug fixes
- New features
- Documentation improvements
- Hardware compatibility reports

## License

MIT License - See LICENSE file for details

## Acknowledgments

- dump1090/readsb developers for the excellent ADS-B decoder
- Arduino community for the extensive libraries
- FlightAware and ADS-B Exchange for aircraft database references

---

**Happy plane spotting!** ✈️
