# Build & Flash Guide

This guide covers downloading the Flight Radar code and uploading it to your Waveshare ESP32-S3-Touch-LCD-1.46C board.

---

## Quick Start (For the Impatient)

1. **Download**: [https://github.com/commputethis/TheMakersSpace-Flight-Radar](https://github.com/commputethis/TheMakersSpace-Flight-Radar) → Code → Download ZIP
2. **Extract** to Desktop → Rename folder to `FlightRadar`
3. **Open**: `FlightRadar/src/FlightRadar/FlightRadar.ino` in Arduino IDE
4. **Install Libraries**: Arduino_GFX_Library (1.5.7+), ArduinoJson (7.x), WiFiManager (2.0.x), SensorLib
5. **Board Settings**: Select "Wavesheare ESP32-S3-Touch-LCD-1.46", PSRAM: **Enabled**
6. **Upload** → Verify display shows "SETUP"

**Stuck?** See the detailed steps below.

---

## Part 1: Download the Code

### Option A: Download ZIP (Recommended)

1. Go to [https://github.com/commputethis/TheMakersSpace-Flight-Radar](https://github.com/commputethis/TheMakersSpace-Flight-Radar)
2. Click the green **"<> Code"** button
3. Select **"Download ZIP"**
4. Extract the ZIP file to your Desktop
5. **Rename** the extracted folder to `FlightRadar` (removes the `-main` suffix)

You should now have:

```Text
Desktop/
└── FlightRadar/
    ├── src/
    │   └── FlightRadar/
    │       ├── FlightRadar.ino    ← Open this
    │       ├── board_config.h
    │       ├── aircraft_types.h
    │       ├── themes.h
    |       ├── ui_layout.h
    │       └── font_config.h
    ├── docs/
    └── README.md
```

### Option B: Use Git (If Installed)

```bash
cd Desktop # or wherever you want to store the git repository
git clone https://github.com/commputethis/TheMakersSpace-Flight-Radar.git
```

---

## Part 2: Install Arduino IDE & ESP32 Support

### First-Time Setup

**If you already have Arduino IDE with ESP32 support, skip to Part 3.**

1. Download Arduino IDE from [https://www.arduino.cc/en/software](https://www.arduino.cc/en/software)
   - Version 1.8.x or 2.x both work
2. Install and launch Arduino IDE
3. Open **File → Preferences** (Arduino → Preferences on Mac)
4. Find "Additional Board Manager URLs" and add:
   1. `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
   - If there's already a URL there, click the icon and add this on a new line
5. Click **OK**
6. Go to **Tools → Board → Board Manager**
7. Search for **"ESP32"**
8. Install **"ESP32 by Espressif Systems"** (latest version)
9. Wait for installation to complete (may take a few minutes)

---

## Part 3: Install Required Libraries

Go to **Sketch → Include Library → Manage Libraries...**

In the Library Manager search box, install each of these:

| Library | Version | Author |
| --------- | --------- | -------- |
| `Arduino_GFX_Library` | 1.5.7 or higher | moononournation |
| `ArduinoJson` | 7.x | Benoit Blanchon |
| `WiFiManager` | 2.0.x | tzapu |
| `SensorLib` | Latest | Lewis He |

**Installation tips:**

- Search for the exact name shown above
- Click **Install** for each library
- If you see multiple results, pick the one with the matching author
- Some libraries may already be installed— that's fine!

**Close and restart Arduino IDE** after installing libraries.

---

## Part 4: Open the Project

1. In Arduino IDE: **File → Open**
2. Navigate to `Desktop/FlightRadar/src/FlightRadar/`
3. Select `FlightRadar.ino`
4. Click **Open**

You should see:

- Main code in the editor
- Tabs at the top: FlightRadar.ino, board_config.h, aircraft_types.h, themes.h, font_config.h

---

## Part 5: Configure Board Settings

Connect your board to the computer with a USB-C **data cable** (not a charge-only cable).

### Select the Board

Go to **Tools** menu and set **exactly** these options:

``` text
Board:                Wavesheare ESP32-S3-Touch-LCD-1.46
USB CDC On Boot:      Enabled
Events Run On:        Core 0
Flash Mode:           QIO 120MHz
Arduino Code Runs On: Core 1
Partition Scheme:     8M with spiffs (3MB APP/1.5MB SPIFFS)
PSRAM:                Enabled       ← CRITICAL!
Upload Speed:         921600
USB Mode:             Hardware CDC and JTAG
```

**⚠️ Critical:** PSRAM must be set to **"Enabled"** (not "Disabled"). The display framebuffer requires PSRAM. If PSRAM is disabled, the screen will stay black.

### Select the Port

Go to **Tools → Port** and select:

- **Windows**: `COM3` or similar (may show "USB-SERIAL")
- **Mac**: `/dev/cu.usbserial-XXXX` or `/dev/cu.wchusbserial-XXXX`
- **Linux**: `/dev/ttyUSB0` or similar

**If no port appears:**

- Try a different USB cable (must be data cable, not charge-only)
- Install CP210x USB driver (Windows): [https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- On Mac, wait 30 seconds after plugging in—ports take time to appear

---

## Part 6: Upload the Code

Click the **→ (Upload)** button or select **Sketch → Upload**

### If Upload Fails

#### Error: "Failed to connect to ESP32"

Use the BOOT button method:

1. Hold down the **BOOT** button on the board (small button labeled "BOOT")
2. While holding, click **Upload** in Arduino IDE
3. Keep holding until you see "Connecting..." in the output window
4. Release the BOOT button
5. If it still fails, try again

#### Error: "A fatal error occurred: Failed to write to target RAM"

- Press the **RST** button on the board
  - Might be labeled **PWR**
- Try upload again with BOOT button held

**Success:** You should see:

```Text
Leaving...
Hard resetting via RTS pin...
```

---

## Part 7: Verify It Works

### Check the Serial Monitor

1. Open **Tools → Serial Monitor** (or press Ctrl+Shift+M / Cmd+Shift+M)
2. Set baud rate to **115200** (bottom right dropdown)
3. Press the **RST** button on your board

You should see:

```Text
[Boot] FlightRadar starting…
[Boot] Waveshare ESP32-S3-Touch-LCD-1.46C
[Touch] SPD2010 reset complete
[IMU] QMI8658 ready
[FS] LittleFS ready — XXXX KB free
[LCD] panel + canvas ready
[WiFi] connected — IP=192.168.X.XXX
[mDNS] flightradar.local
[OTA] ready
[HTTP] web server up on port 80
[Boot] ready
```

### Check the Display

The round LCD should show:

**"SETUP"** in the center with a WiFi name like:

```Text
FlightRadar-A1B2
```

If you see this, **success!** Proceed to WiFi configuration in the [Student Guide](Student_Guide.md#first-time-setup).

---

## Troubleshooting

| Problem | Solution |
| --------- | ---------- |
| **No port appears** | Try different USB cable; install CP210x driver (Windows); wait 30s (Mac) |
| **"Failed to connect"** | Hold BOOT button during upload |
| **"Canvas->begin() FAILED"** | Check PSRAM is set to **Enabled** in Tools menu |
| **Upload hangs at "Connecting..."** | Press RST/PWR, then retry with BOOT button held |
| **Display stays black** | Press RST/PWR button; verify PSRAM setting |
| **"Library not found"** | Check spelling; install via Library Manager |
| **Serial Monitor shows garbage** | Set baud rate to 115200, not 9600 |
| **Touch doesn't work** | Restart board; SPD2010 needs a few seconds after boot |

---

## Next Steps

- **Students**: Return to [Student Guide](Student_Guide.md#first-time-setup) for WiFi configuration
- **Instructors**: See [Leader Guide](Leader_Guide.md) for teaching notes and troubleshooting tips

---

## Advanced: Flashing Without USB (OTA)

Once the board is running and connected to WiFi, you can flash updates wirelessly:

1. In Arduino IDE: **Tools → Port**
2. Look for `flightradar at 192.168.X.XXX` (network port)
3. Select it
4. Click Upload as normal

No USB cable needed! Requires Arduino IDE and board on same WiFi network.
