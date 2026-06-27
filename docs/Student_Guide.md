# 📄 STUDENT HANDOUT

```text
═══════════════════════════════════════════════════════════════════
  The Maker's Space — Local Flight Radar
  Waveshare ESP32-S3-Touch-LCD-1.46C  ·  Arduino IDE
═══════════════════════════════════════════════════════════════════
```

## What You're Building

A self-contained, battery-capable flight radar that pulls live ADS-B data from `adsb.fi` over WiFi and displays nearby aircraft as glowing blips on a 1.46" round display. Doubles as a clock, has multiple themes, a web settings portal, OTA firmware updates, optional flight logging, and a demo mode for showing it off without WiFi.

## The Board at a Glance

| Component | Chip | Purpose |
| --- | --- | --- |
| MCU | ESP32-S3 (240 MHz, dual-core, WiFi+BT) | Runs everything |
| Display | SPD2010, 412×412 round, QSPI | Radar + UI |
| Touch | SPD2010 (same chip), I²C | Gestures |
| IMU | QMI8658 6-axis | Auto-rotate + shake-to-wake |
| RTC | PCF85063 with coin-cell backup | Keeps time without WiFi |
| Audio | PCM5101 DAC + speaker | Alert chirps |
| Storage | 16 MB Flash + 8 MB PSRAM + microSD slot | Code, framebuffer, logs |
| Other | TCA9554 I/O expander, microphone, battery charger | — |

## First-Time Setup

1. **Power on** the device. The screen shows `SETUP` and a unique WiFi name like `FlightRadar-A1B2`.
2. **Join that WiFi** from your phone. The captive portal opens automatically — if not, browse to `192.168.4.1`.
3. **Pick your home WiFi**, enter your password, and fill in:
   - Home latitude / longitude (default: Fort Wayne, IN)
   - Timezone offset from UTC (e.g., `-5` for EST)
4. **Save.** The device reconnects to your WiFi and the radar appears.
5. After connection, browse to `http://flightradar.local/` on the same network for full settings.

## Touch Gestures

| Gesture | What Happens |
| --- | --- |
| **Tap a blip** | Open the flight detail panel |
| **Tap in flight detail** | Return to radar |
| **Tap a settings row** | Cycle that value (theme, range, units, etc.) |
| **Long-press** (>500 ms) | Open the settings overlay |
| **Swipe left** (radar) | Go to Stats |
| **Swipe down** (radar) | Go to Clock |
| **Swipe right** (most screens) | Back to radar |
| **Drag finger around the outer ring** | Adjust brightness (clockwise = brighter) |
| **Shake the device** | Wake the screen from dim mode |

## On-Screen Symbols

```text
  ▲  Heavy jet (airliner)         ◆  Medium jet (regional/business)
  ┼  Helicopter                   ⊙  Military
  ◯  Light aircraft (GA)          △  Glider (outline only)
```

A line trailing from each blip shows direction and speed.
A short tail of dim blips behind the sweep = phosphor afterglow.

## Colors Tell You Altitude

| Color brightness | Altitude |
| --- | --- |
| Dim | Below 5,000 ft |
| Normal | 5,000–18,000 ft |
| Bright | 18,000–35,000 ft |
| Near-white | Above 35,000 ft |

**Emergency squawk overrides:**

- 🔴 **7500** — Hijack
- 🟡 **7600** — Radio failure
- 🟠 **7700** — General emergency

If the radar plays three urgent beeps and a blip turns red, you're watching aviation history happen.

## Web Portal (`http://flightradar.local/`)

Everything from the on-screen settings plus:

- Precise lat/lon entry
- Min altitude filter (filters out ground vehicles)
- Idle-dim timeout
- OTA password
- Reboot / forget-WiFi buttons

## Settings Reference

| Setting | Options | What it does |
| --- | --- | --- |
| Theme | Green Phosphor / Amber CRT | Color palette |
| Range | 5 / 10 / 25 / 50 (units) | Outer ring radius |
| Units | Native (nm/kt) / Miles (mi/mph) / Metric (km/kmh) | Display units |
| Time | 12h / 24h | Clock format |
| Audio | ON / OFF | Alert chirps |
| Demo | ON / OFF | Fake aircraft (no WiFi needed) |
| Bright | 10–255 | Backlight level (also via edge drag) |
| Log | ON / OFF | Append spotted flights to `/flights.csv` on internal flash |

## Troubleshooting

| Symptom | Try |
| --- | --- |
| Black screen, backlight on | Power-cycle (unplug 5 s). Confirm Tools → PSRAM = OPI PSRAM if reflashing. |
| Can't find AP / portal | Hold the BOOT button 5 s after boot — this clears saved WiFi. |
| No flights showing | Confirm WiFi is connected (top-right "WiFi" indicator green). Try enabling Demo mode to verify rendering. |
| Touch unresponsive | Restart. The SPD2010 needs a few seconds after boot to leave BIOS mode. |
| Time is wrong | Set timezone offset in the web portal. NTP runs once on connect. |
| Want a clean start | In web portal → "Forget WiFi" button. |

## Where to Go Next

- **Modify the code.** Try changing a theme color, adding a new range preset, or tweaking the sweep speed.
- **Add aircraft types.** Edit `aircraft_types.h` and add entries — they're alphabetically sorted (binary searched).
- **Add a third theme.** Copy the Amber CRT block in `themes.h`, change the colors, and bump `THEME_COUNT`.
- **Flash via WiFi.** With OTA enabled, Arduino IDE → Tools → Port shows `flightradar at <ip>` as a network port. No USB needed.

## Quick Code Tour

```text
FlightRadar/
├── FlightRadar.ino   ← Main sketch (3 parts: init, render, input/network)
├── board_config.h    ← Every hardware pin lives here
├── font_config.h     ← Set font sizes
├── themes.h          ← Color palettes
└── aircraft_types.h  ← ICAO code → human name lookup
```

Class project page (with code, schematics, and updates):

> [https://github.com/commputethis/TheMakersSpace-Flight-Radar](https://github.com/commputethis/TheMakersSpace-Flight-Radar)

```text
═══════════════════════════════════════════════════════════════════
       Built with: Arduino_GFX · ArduinoJson · WiFiManager
       · SensorLib · adsb.fi API · tar1090-db (concept credit)
═══════════════════════════════════════════════════════════════════
```
