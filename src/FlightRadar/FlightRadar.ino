// ================================================================
// FlightRadar.ino  —  Part 1 of 3
// ----------------------------------------------------------------
// Local Flight Radar for Waveshare ESP32-S3-Touch-LCD-1.46C
// The Maker's Space — educational class project
//
// Broken into 3 parts for covering in class
//
// PART 1: includes, types, globals, NVS settings,
//         hardware init (display, touch, IMU, RTC, audio, expander),
//         ADS-B fetch task, demo mode generator
// PART 2: coordinate math, all screen rendering, sweep animation
// PART 3: touch/gesture handling, IMU rotation + shake-to-wake,
//         WiFi/OTA/mDNS/web portal, LittleFS logging, setup()/loop()
//
// Paste all three parts in order into the SAME FlightRadar.ino file.
//
// Libraries (Arduino IDE → Library Manager):
//   - Arduino_GFX_Library  (moononournation)  v1.4.7+
//   - ArduinoJson          (Benoit Blanchon)  v7.x
//   - WiFiManager          (tzapu)            v2.0.x
//   - SensorLib            (Lewis He)         (for QMI8658)
//   - ArduinoOTA, ESPmDNS, WebServer, LittleFS, Preferences (built-in)
//
// Board (Tools menu):
//   Board:            Wavesheare ESP32-S3-Touch-LCD-1.46
//   USB CDC On Boot:  Enabled
//   Events Run On:    Core 0
//   Flash Mode:       QIO 120MHz
//   Arduino Code Runs On: Core 1
//   Partition Scheme: 8M with spiffs (3MB APP/1.5MB SPIFFS)
//   PSRAM:            Enabled
//   Upload Speed:     921600
//   USB Mode:         Hardware CDC and JTAG
// ================================================================

#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiManager.h>          // tzapu/WiFiManager
#include <HTTPClient.h>
#include <ArduinoJson.h>          // v7+ (JsonDocument)
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <time.h>
#include <esp_sntp.h>
#include <driver/i2s.h>
#include "driver/spi_master.h"

#include <Arduino_GFX_Library.h>  // moononournation/Arduino_GFX

#include "board_config.h"
#include "themes.h"
#include "aircraft_types.h"
#include "font_config.h"

#include "SensorQMI8658.hpp"      // From SensorLib (Lewis He)

// ================================================================
//                      Types & Constants
// ================================================================

enum ScreenMode : uint8_t {
  SCREEN_RADAR = 0,
  SCREEN_FLIGHT_DETAIL,
  SCREEN_CLOCK,
  SCREEN_STATS,
  SCREEN_SETTINGS,
  SCREEN_AP_MODE,
};

// One aircraft. Compact so a fleet of 64 fits comfortably in RAM.
struct Aircraft {
  char     hex[8];         // ICAO 24-bit hex
  char     callsign[10];   // Flight number
  char     type[5];        // ICAO type (e.g. "B738")
  float    lat, lon;
  int32_t  altitude_ft;
  float    ground_speed;   // knots from API
  float    track;          // degrees, 0 = N
  char     squawk[5];
  uint8_t  category;       // AircraftCategory
  // Derived each render
  float    distance_units; // mi or km, depending on settings.use_km
  float    bearing;
  // For the sweep animation: 255 = fully bright, decays to 0
  uint8_t  intensity;
  uint32_t last_seen_ms;
  bool     in_range;
};

struct RadarStats {
  uint32_t aircraft_seen_total;
  uint32_t aircraft_in_range_now;
  float    closest_distance;
  char     closest_callsign[10];
  int32_t  highest_altitude_ft;
  char     highest_callsign[10];
  float    fastest_speed;
  char     fastest_callsign[10];
  uint32_t boot_time_unix;
};

struct Settings {
  float    home_lat;
  float    home_lon;
  int32_t  tz_offset_seconds;
  bool     use_24h;
  bool     observe_dst = true;   // true = add 1 hour when in DST period
  uint8_t  range_idx;            // 0..3 → 5/10/25/50
  uint8_t  unit_mode = 0;        // 0=NATIVE(nm/kt), 1=MILES(mi/mph), 2=KILOMETERS(km/kmh);
  uint8_t  theme_idx;
  uint8_t  brightness;
  int32_t  min_altitude_ft;
  bool     audio_enabled;
  bool     demo_mode;
  bool     log_to_fs;            // log spotted aircraft to LittleFS
  uint32_t idle_timeout_sec;     // 0 = never dim
  char     ota_password[24];
};

// ================================================================
//                          Globals
// ================================================================

// ---- Display ----
Arduino_DataBus *gfxBus   = nullptr;
Arduino_GFX     *gfxPanel = nullptr;   // raw SPD2010 panel object
Arduino_GFX     *gfx      = nullptr;   // primary draw target (points at canvas after init)

// Off-screen 16-bit framebuffer for flicker-free animation.
// 412 * 412 * 2 = ~340 KB — comfortably fits in PSRAM.
Arduino_Canvas  *canvas   = nullptr;   // PSRAM framebuffer wrapping gfxPanel

SensorQMI8658 imu;

SemaphoreHandle_t aircraftMutex = nullptr;
Aircraft aircraftList[ADSB_MAX_AIRCRAFT];
volatile uint8_t  aircraftCount   = 0;
volatile uint32_t lastAdsbFetchMs = 0;
volatile bool     lastAdsbOk      = false;

Settings    settings;
Preferences prefs;

RadarStats  stats;
#define UNIQUE_ICAO_CACHE 256
uint32_t  uniqueIcaoCache[UNIQUE_ICAO_CACHE];
uint16_t  uniqueIcaoHead = 0;

volatile ScreenMode currentScreen = SCREEN_RADAR;
uint8_t  displayRotation = 0;
int8_t   selectedAircraft = -1;
uint8_t  settingsPage = 0;

String    apSsid;
bool      apMode = false;
bool      wifiConnected = false;
WebServer webServer(80);

// Sweep animation
float    sweepAngleDeg = 0.0f;
uint32_t lastSweepMs   = 0;

// Idle / brightness
uint32_t lastInteractionMs = 0;
bool     screenDimmed     = false;
uint8_t  preDimBrightness = BRIGHTNESS_DEFAULT;

// TCA9554 mirrored state
uint8_t tca9554OutputState = 0xFF;
uint8_t tca9554Config      = 0xFF;

// Forward declarations
void  initDisplay();
void  initTouch();
void  initIMU();
void  initRTC();
void  initAudio();
void  initStorage();
void  initWiFiAndPortal();
void  initOTA();
void  setBrightness(uint8_t b);
void  loadSettings();
void  saveSettings();
void  fetchAdsbTask(void* arg);
void  generateDemoAircraft();
void  handleAdsbResults(Aircraft* incoming, uint8_t count);
const RadarTheme& currentTheme();
void  renderRadar();
void  renderClock();
void  renderStats();
void  renderFlightDetail();
void  renderSettingsOverlay();
void  renderApMode();
void  pollTouchAndHandleGestures();
void  updateImuRotation();
void  playTone(uint16_t freq_hz, uint16_t duration_ms);

// Tiny helpers
inline float milesToKm(float mi)   { return mi * 1.609344f; }
inline float kmToMiles(float km)   { return km / 1.609344f; }
inline float nmToMiles(float nm)   { return nm * 1.15078f; }
inline float milesToNm(float mi)   { return mi / 1.15078f; }
inline int   currentRangeMi() {
  static const int presets[4] = {5, 10, 25, 50};
  return presets[settings.range_idx & 3];
}
inline int currentRangeUnits() {
  int mi = currentRangeMi();
  switch(settings.unit_mode) {
    case 0: // Native - nautical miles
      return (int)(milesToNm(mi) + 0.5f);
    case 1: // Miles
      return mi;
    case 2: // Kilometers
    default:
      return (int)(milesToKm(mi) + 0.5f);
  }
}
inline const char* unitsLabel() {
  switch(settings.unit_mode) {
    case 0: return "nm";
    case 1: return "mi";
    case 2: return "km";
    default: return "km";
  }
}

// ================================================================
//                    TCA9554 I/O Expander Helpers
// ----------------------------------------------------------------
// The expander gives us 8 extra GPIOs ("EXIO0..EXIO7") over I2C.
// We mirror the output state in software so each pin change is a
// single 2-byte write to register 0x01.  Config register is 0x03
// (0 = output, 1 = input).
// ================================================================
bool tca9554WriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(TCA9554_ADDR);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void tca9554PinMode(uint8_t pin, uint8_t mode) {
  if (mode) tca9554Config |=  (1 << pin);
  else      tca9554Config &= ~(1 << pin);
  tca9554WriteRegister(0x03, tca9554Config);
}

void tca9554DigitalWrite(uint8_t pin, uint8_t value) {
  if (value) tca9554OutputState |=  (1 << pin);
  else       tca9554OutputState &= ~(1 << pin);
  tca9554WriteRegister(0x01, tca9554OutputState);
}

void initExpander() {
  tca9554PinMode(EXIO_TP_RST,   0);   // output
  tca9554PinMode(EXIO_LCD_RST,  0);   // output
  tca9554PinMode(EXIO_SD_CS,    0);   // output
  tca9554PinMode(EXIO_IMU_INT1, 1);   // input
  tca9554PinMode(EXIO_IMU_INT2, 1);   // input
  // Defaults: resets released, SD deselected
  tca9554DigitalWrite(EXIO_TP_RST,  1);
  tca9554DigitalWrite(EXIO_LCD_RST, 1);
  tca9554DigitalWrite(EXIO_SD_CS,   1);
}

// ================================================================
//                         Settings (NVS)
// ----------------------------------------------------------------
// Persistent key/value store. One namespace "flightradar".
// ================================================================
void loadSettings() {
  prefs.begin("flightradar", true);   // read-only

  settings.home_lat          = prefs.getFloat("lat",    DEFAULT_LAT);
  settings.home_lon          = prefs.getFloat("lon",    DEFAULT_LON);
  settings.tz_offset_seconds = prefs.getInt  ("tz",     -18000);  // EST
  settings.use_24h           = prefs.getBool ("h24",    false);
  settings.observe_dst       = prefs.getBool("ovserve_dst", true);
  settings.range_idx         = prefs.getUChar("range",  2);       // 25 mi
  settings.unit_mode         = prefs.getUChar("unit_mode", 0);  // 0=native default
  settings.theme_idx         = prefs.getUChar("theme",  THEME_GREEN_PHOSPHOR);
  settings.brightness        = prefs.getUChar("bright", BRIGHTNESS_DEFAULT);
  settings.min_altitude_ft   = prefs.getInt  ("minalt", 500);
  settings.audio_enabled     = prefs.getBool ("audio",  true);
  settings.demo_mode         = prefs.getBool ("demo",   false);
  settings.log_to_fs         = prefs.getBool ("log",    false);
  settings.idle_timeout_sec  = prefs.getUInt ("idle",   300);

  String otap = prefs.getString("otapw", OTA_DEFAULT_PASS);
  strlcpy(settings.ota_password, otap.c_str(), sizeof(settings.ota_password));

  prefs.end();
}

void saveSettings() {
  prefs.begin     ("flightradar", false);
  prefs.putFloat  ("lat",         settings.home_lat);
  prefs.putFloat  ("lon",         settings.home_lon);
  prefs.putInt    ("tz",          settings.tz_offset_seconds);
  prefs.putBool   ("h24",         settings.use_24h);
  prefs.putBool   ("observe_dst", settings.observe_dst);
  prefs.putUChar  ("range",       settings.range_idx);
  prefs.putUChar  ("unit_mode",   settings.unit_mode);
  prefs.putUChar  ("theme",       settings.theme_idx);
  prefs.putUChar  ("bright",      settings.brightness);
  prefs.putInt    ("minalt",      settings.min_altitude_ft);
  prefs.putBool   ("audio",       settings.audio_enabled);
  prefs.putBool   ("demo",        settings.demo_mode);
  prefs.putBool   ("log",         settings.log_to_fs);
  prefs.putUInt   ("idle",        settings.idle_timeout_sec);
  prefs.putString ("otapw",       settings.ota_password);
  prefs.end();
}

const RadarTheme& currentTheme() {
  uint8_t idx = settings.theme_idx;
  if (idx >= THEME_COUNT) idx = 0;
  return *THEMES[idx];
}

// ================================================================
//                    Hardware Initialization
// ================================================================

// ---- Display (SPD2010 over QSPI) -------------------------------
void initDisplay() {

  // 1) QSPI bus — matches Waveshare's verified config exactly
  gfxBus = new Arduino_ESP32QSPI(
      LCD_CS_PIN /* 21 */, LCD_SCK_PIN /* 40 */,
      LCD_D0_PIN /* 46 */, LCD_D1_PIN /* 45 */,
      LCD_D2_PIN /* 42 */, LCD_D3_PIN /* 41 */);

  // 2) SPD2010 panel object — do NOT call begin() here.
  //    The canvas wrapper will own the begin() chain.
  gfxPanel = new Arduino_SPD2010(gfxBus, GFX_NOT_DEFINED /* RST handled via expander */);

  // 3) Canvas wraps the panel. This is the actual drawing target —
  //    everything renders into a 412×412 framebuffer in PSRAM, then
  //    `canvas->flush()` DMAs the whole frame to the panel.
  canvas = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, gfxPanel, 0, 0, 0);

  // 4) Single begin() call — cascades through canvas → panel → bus.
  //    Allocates the framebuffer, sends the SPD2010 init sequence,
  //    sets up the QSPI device.
  if (!canvas->begin()) {
    Serial.println("[LCD] canvas->begin() FAILED");
    return;
  }

  // 5) Point the global `gfx` at the canvas so all our existing
  //    render code (gfx->fillScreen, gfx->drawCircle, etc.) writes
  //    into the framebuffer instead of trying to hit QSPI directly.
  gfx = canvas;
  Serial.println("[LCD] panel + canvas ready");

  // 6) Backlight last
  ledcAttach(LCD_BL_PIN, LEDC_BL_FREQ, LEDC_BL_RES_BITS);
  setBrightness(settings.brightness);
}

void setBrightness(uint8_t b) {
  if (b < BRIGHTNESS_MIN) b = BRIGHTNESS_MIN;
  ledcWrite(LCD_BL_PIN, b);
}

void PWR_Loop(void) {
  static uint16_t pressTime = 0;
  
  if(!digitalRead(PWR_KEY_Input_PIN)) {  // Button pressed
    pressTime++;
    if(pressTime >= 200) {  // 20 seconds = shutdown
      digitalWrite(PWR_Control_PIN, LOW);  // Power off
    }
  } else {
    pressTime = 0;
  }
}

// ================================================================
//                  Touch (SPD2010 over I2C)
// ----------------------------------------------------------------
// The SPD2010 touch controller is not a simple "read register, get
// X/Y" chip. It has a state machine with three modes (BIOS, CPU,
// point-mode) and uses 16-bit register addresses. To get coordinates
// we have to:
//   1. Read the 4-byte status word at register 0x2000
//   2. Based on the status flags, send the right command(s) to
//      transition between states until the chip is reporting points
//   3. Once a point report is available, read the HDP buffer at
//      register 0x0003 and unpack the X/Y nybbles
//   4. Read the HDP status at 0xFC02 and clear the touch interrupt
//
// This code mirrors Waveshare's verified ESP-IDF driver for the
// same panel. We poll it from the gesture handler — the state
// machine advances on each call.
// ================================================================

// ---- Reset the touch IC via the I/O expander ----
void initTouch() {
  tca9554DigitalWrite(EXIO_TP_RST, 0);   // assert reset
  delay(50);                              // Waveshare uses 50ms
  tca9554DigitalWrite(EXIO_TP_RST, 1);   // release
  delay(50);
  pinMode(TOUCH_INT_PIN, INPUT_PULLUP);
  Serial.println("[Touch] SPD2010 reset complete");
}

// ---- Small helpers for the 16-bit-register protocol ----

// Send a 16-bit register address followed by data bytes.
static bool spdTouchWrite(uint16_t reg, const uint8_t* data, uint8_t len) {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write((uint8_t)(reg >> 8));         // register high byte first
  Wire.write((uint8_t)(reg & 0xFF));       // then low byte
  for (uint8_t i = 0; i < len; ++i) Wire.write(data[i]);
  return Wire.endTransmission() == 0;
}

// Send a 16-bit register address, then read N bytes.
static bool spdTouchRead(uint16_t reg, uint8_t* data, uint16_t len) {
  Wire.beginTransmission(TOUCH_I2C_ADDR);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  if (Wire.endTransmission(false) != 0) return false;     // repeated-start
  uint16_t got = Wire.requestFrom((uint16_t)TOUCH_I2C_ADDR, len);
  if (got != len) return false;
  for (uint16_t i = 0; i < len; ++i) data[i] = Wire.read();
  return true;
}

// Pre-built command payloads used by the state machine.
static const uint8_t SPD_CMD_ONE[2]  = { 0x01, 0x00 };   // "1" — used for clear-INT and CPU-start
static const uint8_t SPD_CMD_ZERO[2] = { 0x00, 0x00 };   // "0" — used for point-mode and start

static inline void spdClearInt()      { spdTouchWrite(0x0200, SPD_CMD_ONE,  2); }
static inline void spdCpuStart()      { spdTouchWrite(0x0400, SPD_CMD_ONE,  2); }
static inline void spdPointMode()     { spdTouchWrite(0x5000, SPD_CMD_ZERO, 2); }
static inline void spdStart()         { spdTouchWrite(0x4600, SPD_CMD_ZERO, 2); }

// Main poll. Returns true and fills *out when a finger is on screen.
bool readTouch(TouchPoint* out) {
  out->down = false;

  // ── 1. Read 4-byte status word at 0x2000 ──
  uint8_t st[4];
  if (!spdTouchRead(0x2000, st, 4)) return false;

  bool pt_exist    = st[0] & 0x01;   // there is a point report waiting
  bool gesture     = st[0] & 0x02;   // (we ignore gestures here)
  bool aux         = st[0] & 0x08;
  bool tic_in_bios = st[1] & 0x40;   // chip is still in boot code
  bool tic_in_cpu  = st[1] & 0x20;   // CPU running but no mode set yet
  bool cpu_run     = st[1] & 0x08;
  uint16_t read_len = (uint16_t)st[2] | ((uint16_t)st[3] << 8);

  // uncomment the section below for debugging touch
  /* Serial.printf("[Touch] st=%02X %02X len=%u  pt=%d gest=%d bios=%d cpu=%d run=%d\n",
              st[0], st[1], read_len, pt_exist, gesture,
              tic_in_bios, tic_in_cpu, cpu_run);*/

  // ── 2. State machine: drive the chip toward point-reporting mode ──
  if (tic_in_bios) {
    spdClearInt();
    spdCpuStart();
    return false;
  }
  if (tic_in_cpu) {
    spdPointMode();
    spdStart();
    spdClearInt();
    return false;
  }
  if (cpu_run && read_len == 0) {
    spdClearInt();
    return false;
  }
  if (!(pt_exist || gesture)) {
    if (cpu_run && aux) spdClearInt();
    return false;
  }

  // ── 3. Read HDP packet (header + 6 bytes per finger) ──
  if (read_len < 10 || read_len > 64) {
    spdClearInt();
    return false;
  }
  uint8_t hdp[64];
  if (!spdTouchRead(0x0003, hdp, read_len)) return false;

  uint8_t check_id = hdp[4];
  if (check_id > 0x0A || !pt_exist) {
    spdClearInt();
    return false;
  }

  // First finger only — index 0 in the report
  uint16_t x = ((uint16_t)(hdp[7] & 0xF0) << 4) | hdp[5];
  uint16_t y = ((uint16_t)(hdp[7] & 0x0F) << 8) | hdp[6];
  uint8_t  weight = hdp[8];

  // ── 4. Acknowledge by clearing INT if HDP-status says we should ──
  uint8_t hs[8];
  if (spdTouchRead(0xFC02, hs, 8) && hs[5] == 0x82) {
    spdClearInt();
  }

  if (weight == 0) return false;               // touch released

  // Clamp and apply current screen rotation
  if (x >= LCD_WIDTH)  x = LCD_WIDTH  - 1;
  if (y >= LCD_HEIGHT) y = LCD_HEIGHT - 1;
  uint8_t r = gfx ? gfx->getRotation() : 0;
  int16_t lx = x, ly = y;
  switch (r) {
    case 1:  lx = y;                  ly = LCD_WIDTH  - 1 - x; break;
    case 2:  lx = LCD_WIDTH  - 1 - x; ly = LCD_HEIGHT - 1 - y; break;
    case 3:  lx = LCD_HEIGHT - 1 - y; ly = x;                  break;
    default: break;
  }
  out->x = lx;
  out->y = ly;
  out->down = true;
  return true;
}

// ---- IMU (QMI8658) ---------------------------------------------
void initIMU() {
  if (!imu.begin(Wire, IMU_I2C_ADDR, I2C_SDA_PIN, I2C_SCL_PIN)) {
    Serial.println("[IMU] QMI8658 not detected!");
    return;
  }
  imu.configAccelerometer(SensorQMI8658::ACC_RANGE_4G,
                          SensorQMI8658::ACC_ODR_125Hz,
                          SensorQMI8658::LPF_MODE_0);
  imu.enableAccelerometer();
  Serial.println("[IMU] QMI8658 ready");
}

// ---- RTC (PCF85063) --------------------------------------------
// We bit-bang I2C directly — keeps things transparent for the class.
// Register layout: 0x04..0x0A = seconds, minutes, hours, day, weekday,
// month, year (all BCD).
static uint8_t bcd2dec(uint8_t b) { return ((b >> 4) * 10) + (b & 0x0F); }
static uint8_t dec2bcd(uint8_t d) { return ((d / 10) << 4) | (d % 10); }

bool rtcRead(struct tm* out) {
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x04);
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((uint16_t)RTC_I2C_ADDR, (uint8_t)7);
  if (Wire.available() < 7) return false;
  uint8_t s  = Wire.read() & 0x7F;
  uint8_t mn = Wire.read() & 0x7F;
  uint8_t h  = Wire.read() & 0x3F;
  uint8_t d  = Wire.read() & 0x3F;
  Wire.read();                            // weekday — unused
  uint8_t mo = Wire.read() & 0x1F;
  uint8_t y  = Wire.read();
  out->tm_sec  = bcd2dec(s);
  out->tm_min  = bcd2dec(mn);
  out->tm_hour = bcd2dec(h);
  out->tm_mday = bcd2dec(d);
  out->tm_mon  = bcd2dec(mo) - 1;         // tm_mon is 0-based
  out->tm_year = bcd2dec(y) + 100;        // chip stores YY (20YY)
  return true;
}

bool rtcWrite(const struct tm* t) {
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x04);
  Wire.write(dec2bcd(t->tm_sec));
  Wire.write(dec2bcd(t->tm_min));
  Wire.write(dec2bcd(t->tm_hour));
  Wire.write(dec2bcd(t->tm_mday));
  Wire.write(0);                          // weekday placeholder
  Wire.write(dec2bcd(t->tm_mon + 1));
  Wire.write(dec2bcd(t->tm_year % 100));
  return Wire.endTransmission() == 0;
}

void initRTC() {
  // Make sure oscillator runs (clear STOP bit in control 1)
  Wire.beginTransmission(RTC_I2C_ADDR);
  Wire.write(0x00); Wire.write(0x00);
  Wire.endTransmission();

  // Pre-seed system time from RTC so the clock screen has data
  // even before WiFi/NTP comes up.
  struct tm t;
  if (rtcRead(&t)) {
    time_t tt = mktime(&t);
    if (tt > 1577836800) {                // > 2020-01-01
      timeval tv = { .tv_sec = tt, .tv_usec = 0 };
      settimeofday(&tv, nullptr);
    }
  }
}

void syncRtcFromSystem() {
  time_t now = time(nullptr);
  if (now < 1577836800) return;
  struct tm t;
  localtime_r(&now, &t);
  rtcWrite(&t);
}

// Returns true if DST is in effect for the given time
bool isDST(time_t t) {
  if (!settings.observe_dst) return false;
  
  struct tm* timeinfo = localtime(&t);
  int month = timeinfo->tm_mon + 1;  // 1-12
  int day = timeinfo->tm_mday;
  int wday = timeinfo->tm_wday;      // 0=Sunday
  
  // US DST: Second Sunday March to First Sunday November
  if (month < 3 || month > 11) return false;  // Jan, Feb, Dec
  if (month > 3 && month < 11) return true;     // Apr-Oct
  
  // March: DST starts second Sunday
  if (month == 3) {
    int secondSunday = 8 + (7 - ((1 + 8) % 7)) % 7;  // Day of month for 2nd Sunday
    if (day < secondSunday) return false;
    if (day > secondSunday) return true;
    // On transition day, check hour (assume 2 AM)
    return timeinfo->tm_hour >= 2;
  }
  
  // November: DST ends first Sunday
  if (month == 11) {
    int firstSunday = 1 + (7 - ((1 + 1) % 7)) % 7;  // Day of month for 1st Sunday
    if (day < firstSunday) return true;
    if (day > firstSunday) return false;
    // On transition day, check hour (assume 2 AM)
    return timeinfo->tm_hour < 2;
  }
  
  return false;
}

// ---- Audio (I2S → PCM5101 DAC → speaker) -----------------------
#define I2S_SAMPLE_RATE 16000

void initAudio() {
  i2s_config_t cfg = {};
  cfg.mode              = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  cfg.sample_rate       = I2S_SAMPLE_RATE;
  cfg.bits_per_sample   = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format    = I2S_CHANNEL_FMT_RIGHT_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags  = 0;
  cfg.dma_buf_count     = 4;
  cfg.dma_buf_len       = 256;
  cfg.use_apll          = false;
  cfg.tx_desc_auto_clear = true;

  i2s_pin_config_t pins = {};
  pins.bck_io_num   = I2S_BCK_PIN;
  pins.ws_io_num    = I2S_LRCK_PIN;
  pins.data_out_num = I2S_DIN_PIN;
  pins.data_in_num  = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr);
  i2s_set_pin(I2S_NUM_0, &pins);
  i2s_zero_dma_buffer(I2S_NUM_0);
}

// Square-wave tone. Cheap and audible — fine for alert chirps.
void playTone(uint16_t freq_hz, uint16_t duration_ms) {
  if (!settings.audio_enabled || freq_hz == 0) return;
  const int samples     = (I2S_SAMPLE_RATE * duration_ms) / 1000;
  const int half_period = I2S_SAMPLE_RATE / (freq_hz * 2);
  const int16_t amp     = 6000;     // ~18% volume — gentle
  int16_t buf[64];
  int idx = 0;
  for (int i = 0; i < samples; ++i) {
    int16_t v = ((i / half_period) & 1) ? amp : -amp;
    buf[idx++] = v;   // left
    buf[idx++] = v;   // right
    if (idx >= 64) {
      size_t w;
      i2s_write(I2S_NUM_0, buf, sizeof(buf), &w, portMAX_DELAY);
      idx = 0;
    }
  }
  if (idx > 0) {
    size_t w;
    i2s_write(I2S_NUM_0, buf, idx * 2, &w, portMAX_DELAY);
  }
}

void chirpNewAircraft() { playTone(1200,  60); }
void chirpTap()         { playTone(2000,  30); }
void chirpEmergency()   {
  playTone(800, 120); delay(40);
  playTone(800, 120); delay(40);
  playTone(800, 120);
}

// ---- LittleFS (flight log) -------------------------------------
void initStorage() {
  if (!LittleFS.begin(true)) {                    // true = format on fail
    Serial.println("[FS] LittleFS mount failed");
    return;
  }
  Serial.printf("[FS] LittleFS ready — %u KB free\n",
                (unsigned)(LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
}

void logAircraftToFs(const Aircraft& a) {
  if (!settings.log_to_fs) return;
  File f = LittleFS.open("/flights.csv", FILE_APPEND);
  if (!f) return;
  // Write a CSV header the first time the file is created
  if (f.size() == 0) {
    f.println("timestamp,icao,callsign,type,altitude_ft,gs_kt,dist,units,squawk");
  }
  time_t now = time(nullptr);
  struct tm tm; localtime_r(&now, &tm);
  char ts[32];
  strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
  f.printf("%s,%s,%s,%s,%ld,%.0f,%.2f,%s,%s\n",
           ts, a.hex, a.callsign, a.type,
           (long)a.altitude_ft, a.ground_speed,
           a.distance_units, unitsLabel(), a.squawk);
  f.close();
}

// ================================================================
//                  Statistics tracking
// ----------------------------------------------------------------
// Updated each time we merge a new ADS-B response into the list.
// ================================================================
bool icaoSeenBefore(uint32_t hexValue) {
  for (uint16_t i = 0; i < UNIQUE_ICAO_CACHE; ++i) {
    if (uniqueIcaoCache[i] == hexValue) return true;
  }
  return false;
}

void recordIcao(uint32_t hexValue) {
  if (hexValue == 0) return;
  if (icaoSeenBefore(hexValue)) return;
  uniqueIcaoCache[uniqueIcaoHead] = hexValue;
  uniqueIcaoHead = (uniqueIcaoHead + 1) % UNIQUE_ICAO_CACHE;
  stats.aircraft_seen_total++;
}

static uint32_t hexStringToUint(const char* s) {
  uint32_t v = 0;
  while (*s) {
    char c = *s++;
    v <<= 4;
    if      (c >= '0' && c <= '9') v |= (c - '0');
    else if (c >= 'a' && c <= 'f') v |= (c - 'a' + 10);
    else if (c >= 'A' && c <= 'F') v |= (c - 'A' + 10);
  }
  return v;
}

// ================================================================
//                       ADS-B Fetch & Parse
// ----------------------------------------------------------------
// Runs in its own FreeRTOS task so the slow HTTP request never
// freezes the UI. After parsing, handleAdsbResults() takes the
// mutex and atomically updates the shared aircraftList[].
// ================================================================

// Compute distance + bearing from home (called for each aircraft
// every refresh). Haversine for distance, standard great-circle
// formula for bearing.
static void computeRelative(Aircraft& a) {
  const float toRad = 0.01745329252f;
  float lat1 = settings.home_lat * toRad;
  float lon1 = settings.home_lon * toRad;
  float lat2 = a.lat * toRad;
  float lon2 = a.lon * toRad;
  float dLat = lat2 - lat1;
  float dLon = lon2 - lon1;

  float aa = sinf(dLat/2)*sinf(dLat/2) +
             cosf(lat1)*cosf(lat2)*sinf(dLon/2)*sinf(dLon/2);
  float c  = 2 * atan2f(sqrtf(aa), sqrtf(1-aa));
  float dist_km = 6371.0f * c;
  switch(settings.unit_mode) {
    case 0: // Native - nautical miles
      a.distance_units = dist_km / 1.852f;
      break;
    case 1: // Miles
      a.distance_units = kmToMiles(dist_km);
      break;
    case 2: // Kilometers
    default:
      a.distance_units = dist_km;
      break;
  }

  float y = sinf(dLon) * cosf(lat2);
  float x = cosf(lat1)*sinf(lat2) - sinf(lat1)*cosf(lat2)*cosf(dLon);
  float brg = atan2f(y, x) / toRad;
  if (brg < 0) brg += 360.0f;
  a.bearing = brg;
}

void handleAdsbResults(Aircraft* incoming, uint8_t count) {
  if (!aircraftMutex) return;
  xSemaphoreTake(aircraftMutex, portMAX_DELAY);

  uint8_t inRange = 0;
  int rangeLimit = currentRangeUnits();

  for (uint8_t i = 0; i < count; ++i) {
    Aircraft& a = incoming[i];

    // Minimum-altitude filter (suppress ground vehicles)
    if (a.altitude_ft < settings.min_altitude_ft && a.altitude_ft >= 0) continue;

    computeRelative(a);
    a.in_range = (a.distance_units <= rangeLimit);
    if (a.in_range) inRange++;

    // Records
    if (a.distance_units > 0 &&
        (stats.closest_distance == 0 || a.distance_units < stats.closest_distance)) {
      stats.closest_distance = a.distance_units;
      strlcpy(stats.closest_callsign, a.callsign, sizeof(stats.closest_callsign));
    }
    if (a.altitude_ft > stats.highest_altitude_ft) {
      stats.highest_altitude_ft = a.altitude_ft;
      strlcpy(stats.highest_callsign, a.callsign, sizeof(stats.highest_callsign));
    }
    float spd;
    switch(settings.unit_mode) {
      case 0: spd = a.ground_speed; break;           // knots
      case 1: spd = a.ground_speed * 1.15078f; break; // mph
      case 2: spd = a.ground_speed * 1.852f; break;   // km/h
      default: spd = a.ground_speed;
    }
    if (spd > stats.fastest_speed) {
      stats.fastest_speed = spd;
      strlcpy(stats.fastest_callsign, a.callsign, sizeof(stats.fastest_callsign));
    }

    // Unique-ICAO counter
    recordIcao(hexStringToUint(a.hex));

    // Emergency squawk → audible alert (once per refresh)
    if (a.squawk[0] == '7' &&
        (!strcmp(a.squawk, "7500") || !strcmp(a.squawk, "7600") ||
         !strcmp(a.squawk, "7700"))) {
      static uint32_t lastEmergencyMs = 0;
      if (millis() - lastEmergencyMs > 30000) {
        chirpEmergency();
        lastEmergencyMs = millis();
      }
    }

    // Optional logging
    if (a.in_range) logAircraftToFs(a);
  }

  // Copy filtered, in-range aircraft into the shared list
  uint8_t out = 0;
  for (uint8_t i = 0; i < count && out < ADSB_MAX_AIRCRAFT; ++i) {
    if (incoming[i].altitude_ft < settings.min_altitude_ft &&
        incoming[i].altitude_ft >= 0) continue;
    aircraftList[out] = incoming[i];
    aircraftList[out].intensity = 255;
    aircraftList[out].last_seen_ms = millis();
    out++;
  }
  aircraftCount               = out;
  stats.aircraft_in_range_now = inRange;
  lastAdsbOk                  = true;

  xSemaphoreGive(aircraftMutex);
}

void parseAdsbResponse(const String& body) {
  JsonDocument doc;                              // ArduinoJson v7
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    Serial.printf("[ADS-B] JSON parse failed: %s\n", err.c_str());
    return;
  }

  JsonArray ac = doc["ac"].as<JsonArray>();
  if (ac.isNull()) {
    Serial.println("[ADS-B] No 'ac' array in response");
    return;
  }

  Aircraft incoming[ADSB_MAX_AIRCRAFT];
  uint8_t  n = 0;

  for (JsonObject obj : ac) {
    if (n >= ADSB_MAX_AIRCRAFT) break;
    if (!obj["lat"].is<float>() || !obj["lon"].is<float>()) continue;

    Aircraft& a = incoming[n];
    memset(&a, 0, sizeof(a));

    strlcpy(a.hex,      obj["hex"]    | "", sizeof(a.hex));
    strlcpy(a.callsign, obj["flight"] | "", sizeof(a.callsign));
    // adsb.fi pads callsigns to 8 chars — strip trailing spaces
    for (int i = (int)strlen(a.callsign) - 1; i >= 0; --i) {
      if (a.callsign[i] == ' ') a.callsign[i] = '\0'; else break;
    }
    strlcpy(a.type,   obj["t"]      | "", sizeof(a.type));
    strlcpy(a.squawk, obj["squawk"] | "", sizeof(a.squawk));

    a.lat = obj["lat"] | 0.0f;
    a.lon = obj["lon"] | 0.0f;
    // alt_baro may be the literal "ground" — treat as 0
    if (obj["alt_baro"].is<int>())  a.altitude_ft = obj["alt_baro"].as<int>();
    else                            a.altitude_ft = 0;
    a.ground_speed = obj["gs"]    | -1.0f;
    a.track        = obj["track"] | obj["true_heading"] | 0.0f;

    // Map ADS-B "category" string to our internal enum
    const char* cat = obj["category"] | "";
    if      (!strcmp(cat, "A1")) a.category = CAT_LIGHT;
    else if (!strcmp(cat, "A2")) a.category = CAT_MEDIUM;
    else if (!strcmp(cat, "A3")) a.category = CAT_MEDIUM;
    else if (!strcmp(cat, "A4")) a.category = CAT_HEAVY;
    else if (!strcmp(cat, "A5")) a.category = CAT_HEAVY;
    else if (!strcmp(cat, "A7")) a.category = CAT_HELI;
    else if (!strcmp(cat, "B1")) a.category = CAT_GLIDER;
    else                         a.category = CAT_UNKNOWN;

    // Fall back to the bundled ICAO type table for category guess
    if (a.category == CAT_UNKNOWN) {
      const AircraftType* t = lookupAircraftType(a.type);
      if (t) a.category = (uint8_t)pgm_read_byte(&t->category);
    }
    ++n;
  }

  handleAdsbResults(incoming, n);
}

void fetchOnce() {
  if (settings.demo_mode) { generateDemoAircraft(); return; }
  if (!wifiConnected)     return;

  // adsb.fi wants the search radius in nautical miles.
  int range_mi = currentRangeMi();
  int range_nm = (int)(milesToNm((float)range_mi) + 0.5f);

  char path[96];
  snprintf(path, sizeof(path), ADSB_PATH_FMT,
           settings.home_lat, settings.home_lon, range_nm);

  String url = String("https://") + ADSB_HOST + path;

  HTTPClient http;
  http.setTimeout(6000);
  http.setReuse(false);
  http.begin(url);
  http.addHeader("User-Agent", "FlightRadar-ESP32/1.0");

  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    parseAdsbResponse(body);
    lastAdsbFetchMs = millis();
  } else {
    Serial.printf("[ADS-B] HTTP %d\n", code);
    lastAdsbOk = false;
  }
  http.end();
}

void fetchAdsbTask(void* /*arg*/) {
  for (;;) {
    fetchOnce();
    vTaskDelay(pdMS_TO_TICKS(ADSB_FETCH_INTERVAL_MS));
  }
}

// ================================================================
//                          Demo Mode
// ----------------------------------------------------------------
// Generates a handful of plausible aircraft orbiting the home point.
// Useful for class demos, indoor presentations, or when WiFi is
// unavailable. Switches off automatically when demo_mode = false.
// ================================================================
void generateDemoAircraft() {
  static const struct {
    const char* call; const char* type; uint8_t cat; int altitude;
    float speed; const char* sq;
  } templates[] = {
    { "AAL1234", "B738", CAT_HEAVY,  34000, 450, "1200" },
    { "DAL567",  "A320", CAT_HEAVY,  28000, 420, "1234" },
    { "UAL890",  "B789", CAT_HEAVY,  38000, 480, "2000" },
    { "SKW3421", "E75L", CAT_MEDIUM, 22000, 380, "1100" },
    { "N123AB",  "C172", CAT_LIGHT,   3500, 110, "1200" },
    { "LIFE1",   "EC35", CAT_HELI,    1500,  90, "1200" },
    { "N911XX",  "B407", CAT_HELI,     900,  95, "7700" },  // demo emergency
  };
  const uint8_t TPL_COUNT = sizeof(templates) / sizeof(templates[0]);

  if (!aircraftMutex) return;
  xSemaphoreTake(aircraftMutex, portMAX_DELAY);

  uint32_t now = millis();
  float spinSeconds = now / 1000.0f;
  int rangeUnits = currentRangeUnits();

  aircraftCount = TPL_COUNT;
  for (uint8_t i = 0; i < TPL_COUNT; ++i) {
    Aircraft& a = aircraftList[i];
    memset(&a, 0, sizeof(a));
    strlcpy(a.callsign, templates[i].call, sizeof(a.callsign));
    strlcpy(a.type,     templates[i].type, sizeof(a.type));
    strlcpy(a.squawk,   templates[i].sq,   sizeof(a.squawk));
    snprintf(a.hex, sizeof(a.hex), "demo%02d", i);
    a.altitude_ft  = templates[i].altitude;
    a.ground_speed = templates[i].speed;
    a.category     = templates[i].cat;

    // Each one orbits at a different speed/radius/phase
    float radius = (rangeUnits * 0.25f) + (i * rangeUnits * 0.1f);
    if (radius > rangeUnits * 0.95f) radius = rangeUnits * 0.85f;
    float angle = (spinSeconds * (8.0f + i * 2.0f)) + i * 45.0f;
    float rad   = angle * 0.01745329252f;

    // Convert radius+angle (relative to home) back into lat/lon
    float radius_km;
    switch(settings.unit_mode) {
      case 0: radius_km = nmToMiles(radius) * 1.609344f; break; // nm->mi->km
      case 1: radius_km = milesToKm(radius); break;
      case 2: radius_km = radius; break;
      default: radius_km = radius;
    }
    float dx_km = radius_km * sinf(rad);
    float dy_km = radius_km * cosf(rad);
    a.lat = settings.home_lat + (dy_km / 111.0f);
    a.lon = settings.home_lon + (dx_km / (111.0f * cosf(settings.home_lat * 0.01745f)));

    a.distance_units = radius;
    a.bearing        = fmodf(angle, 360.0f);
    a.track          = fmodf(angle + 90.0f, 360.0f);
    a.in_range       = true;
    a.intensity      = 255;
    a.last_seen_ms   = now;
  }
  stats.aircraft_in_range_now = TPL_COUNT;
  lastAdsbOk = true;
  lastAdsbFetchMs = now;
  xSemaphoreGive(aircraftMutex);
}

// ============== End of Part 1 — continue with Part 2 ============
// ================================================================
// FlightRadar.ino  —  Part 2 of 3
// ----------------------------------------------------------------
// Coordinate math + sweep animation + all screen rendering.
// Continues directly below Part 1 in the same FlightRadar.ino file.
// ================================================================

// "Drawing target": canvas if we have one, else the display directly.
inline Arduino_GFX* dgfx() { return canvas ? (Arduino_GFX*)canvas : gfx; }
inline void flushFrame()   { if (canvas) canvas->flush(); }

// ================================================================
//                    Coordinate Helpers
// ================================================================

// Geometric center of the round screen
static const int16_t CX = LCD_WIDTH  / 2;
static const int16_t CY = LCD_HEIGHT / 2;

// Outermost radar ring radius (leaves room for N/S/E/W labels)
static const int16_t RADAR_RADIUS = 180;

// Convert (bearing deg, distance in user units) → screen pixel.
// Bearing 0° = North (straight up); clockwise positive.
static void polarToScreen(float bearingDeg, float distance,
                          int16_t& outX, int16_t& outY)
{
  float maxRange = (float)currentRangeUnits();
  if (maxRange <= 0) maxRange = 1;

  float r = (distance / maxRange) * RADAR_RADIUS;
  if (r > RADAR_RADIUS + 8) r = RADAR_RADIUS + 8;   // pin just past the edge

  float rad = bearingDeg * 0.01745329252f;
  outX = CX + (int16_t)(r * sinf(rad));
  outY = CY - (int16_t)(r * cosf(rad));     // screen Y grows downward
}

// Scale a 16-bit RGB565 by intensity (0..255). Used to fade blips
// behind the sweep line like an old phosphor CRT.
static uint16_t fadeColor(uint16_t c, uint8_t intensity) {
  uint8_t r = ((c >> 11) & 0x1F);
  uint8_t g = ((c >>  5) & 0x3F);
  uint8_t b = ( c        & 0x1F);
  r = (uint8_t)((uint16_t)r * intensity / 255);
  g = (uint8_t)((uint16_t)g * intensity / 255);
  b = (uint8_t)((uint16_t)b * intensity / 255);
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// ================================================================
//                Sweep + Aircraft Intensity
// ----------------------------------------------------------------
// One full rotation every ~6 seconds. As the sweep line crosses an
// aircraft, its intensity snaps to 255, then decays based on how
// far behind the sweep the aircraft has fallen.
// ================================================================
void advanceSweep() {
  uint32_t now = millis();
  uint32_t dt  = now - lastSweepMs;
  lastSweepMs  = now;

  sweepAngleDeg += dt * 0.060f;                 // ~60°/sec
  if (sweepAngleDeg >= 360.0f) sweepAngleDeg -= 360.0f;

  if (!aircraftMutex) return;
  if (xSemaphoreTake(aircraftMutex, 0) != pdTRUE) return;

  for (uint8_t i = 0; i < aircraftCount; ++i) {
    Aircraft& a = aircraftList[i];
    // Angular distance from sweep line, going *backwards* (the part
    // of the screen the sweep has already painted).
    float ang = sweepAngleDeg - a.bearing;
    while (ang < 0)    ang += 360.0f;
    while (ang >= 360) ang -= 360.0f;

    if (ang < 15) {
      // Sweep is right on top of it
      a.intensity = 255;
    } else {
      // Decay smoothly so it never disappears completely
      int v = (int)(255.0f - (ang / 360.0f) * 220.0f);
      if (v < 35) v = 35;
      if (v < a.intensity - 4) a.intensity = (uint8_t)v;
    }
  }
  xSemaphoreGive(aircraftMutex);
}

// Draw WiFi icon at (x,y) - simple arcs representing signal waves
void drawWiFiIcon(Arduino_GFX* g, int16_t x, int16_t y, uint16_t color, bool strong) {
  // Draw dot at bottom
  g->fillCircle(x, y + 8, 2, color);

  // Arcs above the dot (opening upward like a rainbow)
  // Arc angles: 225° to 315° draws the upper semicircle
  g->drawArc(x, y + 8, 5, 3, 225, 315, color);   // inner arc
  
  if (strong) {
    g->drawArc(x, y + 8, 9, 7, 225, 315, color); // outer arc for strong signal
  }
}

// ================================================================
//                  Aircraft Color & Icon
// ================================================================

// Pick a blip color: emergency squawks override altitude band.
static uint16_t blipColorFor(const Aircraft& a, const RadarTheme& th) {
  if (a.squawk[0] == '7') {
    if (!strcmp(a.squawk, "7500")) return th.squawk7500;
    if (!strcmp(a.squawk, "7600")) return th.squawk7600;
    if (!strcmp(a.squawk, "7700")) return th.squawk7700;
  }
  if      (a.altitude_ft <  5000) return th.blipLow;
  else if (a.altitude_ft < 18000) return th.blipMid;
  else if (a.altitude_ft < 35000) return th.blipHigh;
  else                            return th.blipVeryHigh;
}

// Draw the icon for one aircraft at (x,y), rotated to its heading,
// plus a short velocity vector showing direction and speed.
static void drawAircraftIcon(Arduino_GFX* g, int16_t x, int16_t y,
                             const Aircraft& a, uint16_t color)
{
  float rad = a.track * 0.01745329252f;
  float sr  = sinf(rad), cr = cosf(rad);

  switch (a.category) {
    case CAT_HEAVY: {                    // filled triangle, nose forward
      int16_t x1 = x + (int16_t)( 6 * sr);
      int16_t y1 = y - (int16_t)( 6 * cr);
      int16_t x2 = x + (int16_t)(-4 * cr - 3 * sr);
      int16_t y2 = y - (int16_t)(-4 * -sr - 3 * cr);
      int16_t x3 = x + (int16_t)( 4 * cr - 3 * sr);
      int16_t y3 = y - (int16_t)( 4 * -sr - 3 * cr);
      g->fillTriangle(x1, y1, x2, y2, x3, y3, color);
      break;
    }
    case CAT_MEDIUM:                     // diamond
      g->fillTriangle(x, y - 4, x - 3, y, x + 3, y, color);
      g->fillTriangle(x, y + 4, x - 3, y, x + 3, y, color);
      break;
    case CAT_HELI:                       // rotor cross
      g->drawLine(x - 4, y, x + 4, y, color);
      g->drawLine(x, y - 4, x, y + 4, color);
      g->fillCircle(x, y, 1, color);
      break;
    case CAT_MILITARY:                   // bullseye
      g->fillCircle(x, y, 3, color);
      g->drawCircle(x, y, 5, color);
      break;
    case CAT_GLIDER: {                   // long thin outline triangle
      int16_t x1 = x + (int16_t)( 6 * sr);
      int16_t y1 = y - (int16_t)( 6 * cr);
      int16_t x2 = x + (int16_t)(-3 * cr - 2 * sr);
      int16_t y2 = y - (int16_t)(-3 * -sr - 2 * cr);
      int16_t x3 = x + (int16_t)( 3 * cr - 2 * sr);
      int16_t y3 = y - (int16_t)( 3 * -sr - 2 * cr);
      g->drawTriangle(x1, y1, x2, y2, x3, y3, color);
      break;
    }
    case CAT_LIGHT:                      // small circle with wings
      g->fillCircle(x, y, 2, color);
      g->drawLine(x - 3, y, x + 3, y, color);
      break;
    default:                             // unknown — plain dot
      g->fillCircle(x, y, 2, color);
      break;
  }

  // Velocity vector: line in heading direction, length scaled to speed
  if (a.ground_speed > 0) {
    float speedNorm = a.ground_speed / 500.0f;        // 500 kt ≈ full length
    if (speedNorm > 1.0f) speedNorm = 1.0f;
    int len = 6 + (int)(speedNorm * 14.0f);
    int16_t ex = x + (int16_t)(len * sr);
    int16_t ey = y - (int16_t)(len * cr);
    g->drawLine(x, y, ex, ey, color);
  }
}

// ================================================================
//                   Top Status Strip
// ----------------------------------------------------------------
// Compact AC count, range, and WiFi indicator across the top arc.
// ================================================================
static void drawTopStatusStrip(Arduino_GFX* g) {
  const RadarTheme& th = currentTheme();

  // Aircraft count badge (left)
//  g->setTextSize(TXT_SMALL);
//  g->setTextColor(th.textPrimary);
//  g->setCursor(80, 28);
//  g->printf("AC:%d", (int)stats.aircraft_in_range_now);

  // Range label (center)
  char rngBuf[16];
  snprintf(rngBuf, sizeof(rngBuf), "%d %s",
           currentRangeUnits(), unitsLabel());
  int16_t tx = CX - ((int16_t)strlen(rngBuf) * 6) / 2;
  g->setTextColor(th.textSecondary);
  g->setCursor(tx, 28);
  g->print(rngBuf);

  // WiFi indicator (right) - replace text with icon
  int rssi = WiFi.RSSI();
  uint16_t wcolor;
  bool strongSignal = false;
  
  if      (apMode)         { wcolor = th.squawk7600; }
  else if (!wifiConnected) { wcolor = th.squawk7700; }
  else if (rssi > -70)     { wcolor = th.battHigh; strongSignal = true; }
  else                     { wcolor = th.battMid; }
  
  drawWiFiIcon(g, LCD_WIDTH - 125, 28, wcolor, strongSignal);
}

// Battery indicator near the bottom of the screen.
// Skipped if BATT_ADC_PIN < 0 (the production schematic should be
// checked to confirm which pin reads the battery divider).
static void drawBatteryIndicator(Arduino_GFX* g) {
  if (BATT_ADC_PIN < 0) return;
  int raw = analogReadMilliVolts(BATT_ADC_PIN);
  float v = (raw * 3.0 / 1000.0) / Measurement_offset;
  // Crude linear map: 4.2 V = 100 %, 3.3 V = 0 %
  int pct = (int)((v - 3.3f) / 0.9f * 100.0f);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;

  const RadarTheme& th = currentTheme();
  uint16_t color = (pct > 60) ? th.battHigh
                              : (pct > 20 ? th.battMid : th.battLow);
  int bx = CX - 18, by = LCD_HEIGHT - 40;
  g->drawRect(bx, by, 32, 14, color);            // body
  g->fillRect(bx + 32, by + 4, 2, 6, color);     // nub
  g->fillRect(bx + 2, by + 2, (28 * pct) / 100, 10, color);
}

// ================================================================
//                       Radar Screen
// ================================================================
void renderRadar() {
  Arduino_GFX* g = dgfx();
  const RadarTheme& th = currentTheme();

  g->fillScreen(th.background);

  // ── Range rings at 25/50/75/100 % ───────────────────────────
  for (int i = 1; i <= 4; ++i) {
    int r = (RADAR_RADIUS * i) / 4;
    uint16_t c = (i == 4) ? th.radarRing : th.radarRingDim;
    g->drawCircle(CX, CY, r, c);
  }

  // ── Crosshair / N-S, E-W axes ───────────────────────────────
  g->drawLine(CX - RADAR_RADIUS, CY, CX + RADAR_RADIUS, CY, th.crosshair);
  g->drawLine(CX, CY - RADAR_RADIUS, CX, CY + RADAR_RADIUS, th.crosshair);

  // ── Compass labels ──────────────────────────────────────────
  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.compassRose);
  g->setCursor(CX - 5,                 CY - RADAR_RADIUS - 22); g->print("N");
  g->setCursor(CX - 5,                 CY + RADAR_RADIUS + 6);  g->print("S");
  g->setCursor(CX - RADAR_RADIUS - 18, CY - 7);                 g->print("W");
  g->setCursor(CX + RADAR_RADIUS + 6,  CY - 7);                 g->print("E");

  // ── Range scale labels on each ring ─────────────────────────
  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textDim);
  int maxR = currentRangeUnits();
  for (int i = 1; i <= 4; ++i) {
    int r = (RADAR_RADIUS * i) / 4;
    int val = (maxR * i) / 4;
    char buf[8]; snprintf(buf, sizeof(buf), "%d", val);
    g->setCursor(CX + 4, CY - r - 3);
    g->print(buf);
  }

  // ── Sweep line + 2-line trail for that CRT feel ─────────────
  for (int i = 0; i < 3; ++i) {
    float a = sweepAngleDeg - i * 5.0f;
    if (a < 0) a += 360.0f;
    float rad = a * 0.01745329252f;
    int16_t ex = CX + (int16_t)(RADAR_RADIUS * sinf(rad));
    int16_t ey = CY - (int16_t)(RADAR_RADIUS * cosf(rad));
    uint16_t c = (i == 0) ? th.sweepLine
                          : fadeColor(th.sweepLine, 180 - i * 60);
    g->drawLine(CX, CY, ex, ey, c);
  }

  // ── Aircraft blips ──────────────────────────────────────────
  if (aircraftMutex &&
      xSemaphoreTake(aircraftMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    for (uint8_t i = 0; i < aircraftCount; ++i) {
      Aircraft& a = aircraftList[i];
      if (!a.in_range) continue;
      int16_t x, y;
      polarToScreen(a.bearing, a.distance_units, x, y);

      uint16_t base = blipColorFor(a, th);
      uint16_t col  = fadeColor(base, a.intensity);

      // Ring around the currently selected aircraft
      if (selectedAircraft == i) {
        g->drawCircle(x, y, 9, th.blipSelected);
      }

      drawAircraftIcon(g, x, y, a, col);

      // Callsign label (only when blip is bright enough to read)
      if (a.callsign[0] && a.intensity > 80) {
        g->setTextSize(TXT_SMALL);
        g->setTextColor(fadeColor(th.textSecondary, a.intensity));
        g->setCursor(x + 6, y - 6);
        g->print(a.callsign);
      }
    }
    xSemaphoreGive(aircraftMutex);
  }

  drawTopStatusStrip(g);
//  drawBatteryIndicator(g);

  flushFrame();
}

// ================================================================
//                  Flight Detail Screen
// ----------------------------------------------------------------
// Shown after tapping a blip. Pulls a snapshot of the selected
// aircraft under the mutex so it doesn't change mid-render.
// ================================================================
void renderFlightDetail() {
  Arduino_GFX* g = dgfx();
  const RadarTheme& th = currentTheme();
  g->fillScreen(th.background);

  // Round border
  g->drawCircle(CX, CY, LCD_RADIUS - 6, th.panelBorder);
  g->drawCircle(CX, CY, LCD_RADIUS - 7, th.panelBorder);

  if (selectedAircraft < 0 || selectedAircraft >= aircraftCount) {
    g->setTextSize(TXT_MEDIUM);
    g->setTextColor(th.textPrimary);
    g->setCursor(CX - 70, CY - 8);
    g->print("No flight");
    flushFrame();
    return;
  }

  Aircraft a;
  if (aircraftMutex &&
      xSemaphoreTake(aircraftMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
    a = aircraftList[selectedAircraft];
    xSemaphoreGive(aircraftMutex);
  } else {
    return;
  }

  const AircraftType* t2 = lookupAircraftType(a.type);
  if (!t2) {
    Serial.printf("Lookup failed for type: '%s' (len=%d) hex:", a.type, strlen(a.type));
    for (int i = 0; i < 5; i++) Serial.printf(" %02X", (unsigned char)a.type[i]);
    Serial.println();
  }

  // Resolve ICAO type → human name from the PROGMEM table
  char typeName[24] = "Unknown type";
  const AircraftType* t = lookupAircraftType(a.type);
  if (t) {
    strncpy_P(typeName, t->name, sizeof(typeName) - 1);
    typeName[sizeof(typeName) - 1] = '\0';
  }

  // Compass-direction string from numeric heading
  const char* dirs[8] = { "N","NE","E","SE","S","SW","W","NW" };
  int dirIdx = ((int)((a.track + 22.5f) / 45.0f)) % 8;
  if (dirIdx < 0) dirIdx += 8;

  // Convert ground speed knots → user units
  float speed;
  const char* spdUnit;
  switch(settings.unit_mode) {
    case 0: // Native - knots
      speed = a.ground_speed;
      spdUnit = "kt";
      break;
    case 1: // Miles
      speed = a.ground_speed * 1.15078f;
      spdUnit = "mph";
      break;
    case 2: // Kilometers
    default:
      speed = a.ground_speed * 1.852f;
      spdUnit = "km/h";
      break;
  }

  int y = 80;
  g->setTextSize(TXT_LARGE);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 75, y);
  g->print(a.callsign[0] ? a.callsign : "------");

  y += 40;
  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textSecondary);
  g->setCursor(CX - 90, y);
  g->printf("%s  %s", a.type, typeName);

  y += 24;
  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 90, y); g->printf("ALT: %ld ft", (long)a.altitude_ft);
  y += 26;
  g->setCursor(CX - 90, y); g->printf("SPD: %.0f %s", speed, spdUnit);
  y += 26;
  g->setCursor(CX - 90, y);
  g->printf("DST: %.1f %s", a.distance_units, unitsLabel());
  y += 26;
  g->setCursor(CX - 90, y); g->printf("HDG: %s %.0f", dirs[dirIdx], a.track);

  // Squawk row — coloured if emergency code
  y += 26;
  if (a.squawk[0]) {
    uint16_t sc = th.textSecondary;
    if      (!strcmp(a.squawk, "7500")) sc = th.squawk7500;
    else if (!strcmp(a.squawk, "7600")) sc = th.squawk7600;
    else if (!strcmp(a.squawk, "7700")) sc = th.squawk7700;
    g->setTextColor(sc);
    g->setCursor(CX - 90, y); g->printf("SQK: %s", a.squawk);
  }

  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textDim);
  g->setCursor(CX - 75, LCD_HEIGHT - 30);
//  g->print("Swipe right to go back");

  flushFrame();
}

// ================================================================
//                       Clock Screen
// ================================================================
void renderClock() {
  Arduino_GFX* g = dgfx();
  const RadarTheme& th = currentTheme();
  g->fillScreen(th.background);

  time_t now = time(nullptr);
  // Apply DST if enabled
  if (settings.observe_dst && isDST(now)) {
    now += 3600;  // Add 1 hour for DST
  }
  struct tm tm; localtime_r(&now, &tm);

  const char* days[7]    = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
  const char* months[12] = {"JAN","FEB","MAR","APR","MAY","JUN",
                            "JUL","AUG","SEP","OCT","NOV","DEC"};

  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.clockDate);
  g->setCursor(CX - 18, 90);
  g->print(days[tm.tm_wday]);

  // Big HH:MM
  char timeBuf[12];
  if (settings.use_24h) {
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", tm.tm_hour, tm.tm_min);
  } else {
    int h = tm.tm_hour % 12; if (h == 0) h = 12;
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d", h, tm.tm_min);
  }
  g->setTextSize(TXT_XXLARGE);
  g->setTextColor(th.clockFace);
  g->setCursor(CX - 105, CY - 28);
  g->print(timeBuf);

  // Seconds (smaller, right of minutes)
  g->setTextSize(TXT_LARGE);
  g->setTextColor(th.clockSecond);
  g->setCursor(CX + 108, CY + 18);
  char secBuf[4]; snprintf(secBuf, sizeof(secBuf), "%02d", tm.tm_sec);
  g->print(secBuf);

  // AM/PM in 12-hour mode
  if (!settings.use_24h) {
    g->setTextSize(TXT_MEDIUM);
    g->setTextColor(th.clockSecond);
    g->setCursor(CX + 108, CY - 28);
    g->print(tm.tm_hour < 12 ? "AM" : "PM");
  }

  // Date
  char dateBuf[24];
  snprintf(dateBuf, sizeof(dateBuf), "%s %d  %d",
           months[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);
  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.clockDate);
  g->setCursor(CX - 80, CY + 60);
  g->print(dateBuf);

  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textDim);
  g->setCursor(CX - 75, LCD_HEIGHT - 30);
//  g->print("Swipe right to go back");

  drawBatteryIndicator(g);
  flushFrame();
}

// ================================================================
//                     Statistics Screen
// ================================================================
void renderStats() {
  Arduino_GFX* g = dgfx();
  const RadarTheme& th = currentTheme();
  g->fillScreen(th.background);

  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 50, 70);
  g->print("STATS");

  int y = 110;
  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textSecondary);
  g->setCursor(CX - 90, y);
  g->printf("Seen total:   %lu", (unsigned long)stats.aircraft_seen_total);
  y += 18;
  g->setCursor(CX - 90, y);
  g->printf("In range now: %lu", (unsigned long)stats.aircraft_in_range_now);
  y += 26;

  g->setTextColor(th.textPrimary); g->setCursor(CX - 90, y); g->print("Closest:");
  y += 16;
  g->setTextColor(th.textSecondary); g->setCursor(CX - 90, y);
  if (stats.closest_distance > 0) {
    const char* spdLabels[] = {"kt", "mph", "km/h"};
    g->printf("  %s @ %.0f %s",
              stats.fastest_callsign[0] ? stats.fastest_callsign : "----",
              stats.fastest_speed, spdLabels[settings.unit_mode]);
  } else g->print("  --");
  y += 22;

  g->setTextColor(th.textPrimary); g->setCursor(CX - 90, y); g->print("Highest:");
  y += 16;
  g->setTextColor(th.textSecondary); g->setCursor(CX - 90, y);
  if (stats.highest_altitude_ft > 0) {
    g->printf("  %s @ %ld ft",
              stats.highest_callsign[0] ? stats.highest_callsign : "----",
              (long)stats.highest_altitude_ft);
  } else g->print("  --");
  y += 22;

  g->setTextColor(th.textPrimary); g->setCursor(CX - 90, y); g->print("Fastest:");
  y += 16;
  g->setTextColor(th.textSecondary); g->setCursor(CX - 90, y);
  if (stats.fastest_speed > 0) {
    const char* spdLabels[] = {"kt", "mph", "km/h"};
    g->printf("  %s @ %.0f %s",
              stats.fastest_callsign[0] ? stats.fastest_callsign : "----",
              stats.fastest_speed, spdLabels[settings.unit_mode]);
  } else g->print("  --");

  y += 28;
  uint32_t uptimeSec = millis() / 1000;
  g->setTextColor(th.textDim);
  g->setCursor(CX - 90, y);
  g->printf("Uptime: %luh %lum",
            (unsigned long)(uptimeSec / 3600),
            (unsigned long)((uptimeSec / 60) % 60));

  g->setTextSize(TXT_SMALL);
  g->setCursor(CX - 75, LCD_HEIGHT - 30);
//  g->print("Swipe right to go back");

  flushFrame();
}

// ================================================================
//                  Settings Overlay (long-press)
// ----------------------------------------------------------------
// Eight toggleable rows. Tap a row to cycle its value; the change
// is saved to NVS immediately so settings persist across reboots.
// ================================================================
static String settingsValueStr(int row) {
  char buf[24];
  switch (row) {
    case 0: snprintf(buf, sizeof(buf), "%s", THEMES[settings.theme_idx]->name); break;
    case 1: snprintf(buf, sizeof(buf), "%d %s", currentRangeUnits(), unitsLabel()); break;
    case 2: {
      const char* modeStr;
      switch(settings.unit_mode) {
        case 0: modeStr = "Native"; break;
        case 1: modeStr = "Miles"; break;
        case 2: modeStr = "Metric"; break;
        default: modeStr = "Native";
      }
      snprintf(buf, sizeof(buf), "%s", modeStr);
      break;
    }
    case 3: snprintf(buf, sizeof(buf), "%s", settings.use_24h  ? "24h" : "12h");   break;
    case 4: snprintf(buf, sizeof(buf), "%s", settings.audio_enabled ? "ON" : "OFF"); break;
    case 5: snprintf(buf, sizeof(buf), "%s", settings.demo_mode     ? "ON" : "OFF"); break;
    case 6: snprintf(buf, sizeof(buf), "%d", settings.brightness); break;
    case 7: snprintf(buf, sizeof(buf), "%s", settings.log_to_fs    ? "ON" : "OFF"); break;
    case 8: snprintf(buf, sizeof(buf), "%s", settings.observe_dst ? "ON" : "OFF"); break;
    default: buf[0] = '\0';
  }
  return String(buf);
}

// Hit-test helper used by the touch handler in Part 3
int settingsRowHit(int16_t ty) {
  if (ty < 105 || ty > 105 + 8 * 18) return -1;
  return (ty - 105) / 18;
}

void renderSettingsOverlay() {
  Arduino_GFX* g = dgfx();
  const RadarTheme& th = currentTheme();
  g->fillScreen(th.background);
  g->drawCircle(CX, CY, LCD_RADIUS - 6, th.panelBorder);

  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 60, 60);
  g->print("SETTINGS");

  const char* labels[9] = {
    "Theme:", "Range:", "Units:", "Time:",
    "Audio:", "Demo:",  "Bright:", "Log:", "DST:" 
  };
  int y = 100;
  g->setTextSize(TXT_SMALL);
  for (int i = 0; i < 8; ++i) {
    g->setTextColor(th.textSecondary);
    g->setCursor(CX - 120, y); g->print(labels[i]);
    g->setTextColor(th.textPrimary);
    g->setCursor(CX - 20, y); g->print(settingsValueStr(i));
    y += 30;
  }

  g->setTextColor(th.textDim);
  g->setCursor(CX - 100, LCD_HEIGHT - 60);
  g->print("Tap row to change");
  //g->setCursor(CX - 100, LCD_HEIGHT - 40);
  //g->print("Swipe right to exit");
  flushFrame();
}

// Called by the touch handler when a row in the overlay is tapped.
void cycleSettingsRow(int row) {
  switch (row) {
    case 0: settings.theme_idx     = (settings.theme_idx + 1) % THEME_COUNT; break;
    case 1: settings.range_idx     = (settings.range_idx + 1) & 3;           break;
    case 2: settings.unit_mode     = (settings.unit_mode + 1) % 3;           break;
    case 3: settings.use_24h       = !settings.use_24h;                      break;
    case 4: settings.audio_enabled = !settings.audio_enabled;                break;
    case 5: settings.demo_mode     = !settings.demo_mode;                    break;
    case 6: {
      int b = settings.brightness + 30;
      if (b > 255) b = BRIGHTNESS_MIN;
      settings.brightness = (uint8_t)b;
      setBrightness(settings.brightness);
      break;
    }
    case 7: settings.log_to_fs = !settings.log_to_fs; break;
    case 8: settings.observe_dst = !settings.observe_dst; break;
  }
  saveSettings();
}

// ================================================================
//                      AP Setup Screen
// ----------------------------------------------------------------
// Shown while WiFiManager's captive portal is open. Tells the user
// the unique AP SSID and the portal IP to connect to.
// ================================================================
void renderApMode() {
  Arduino_GFX* g = dgfx();
  const RadarTheme& th = currentTheme();
  g->fillScreen(th.background);
  g->drawCircle(CX, CY, LCD_RADIUS - 6, th.panelBorder);

  g->setTextSize(TXT_LARGE);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 65, 90);
  g->print("SETUP");

  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textSecondary);
  g->setCursor(CX - 100, 150);
  g->print("Connect WiFi to:");

  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.textPrimary);
  int16_t sx = CX - ((int16_t)apSsid.length() * 12) / 2;
  g->setCursor(sx, 175);
  g->print(apSsid);

  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textSecondary);
  g->setCursor(CX - 95, 215);
  g->print("Then open a browser at:");

  g->setTextSize(TXT_MEDIUM);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 80, 240);
  g->print("192.168.4.1");

  g->setTextSize(TXT_SMALL);
  g->setTextColor(th.textDim);
  g->setCursor(CX - 110, LCD_HEIGHT - 50);
  g->print("Portal stays open for 5 min");

  flushFrame();
}

// ================================================================
//             Aircraft Selection (tap-to-detail)
// ----------------------------------------------------------------
// Given a touch in screen space, find the closest blip within 25 px
// and return its index, or -1 if no match.
// ================================================================
int8_t findAircraftNearTouch(int16_t tx, int16_t ty) {
  if (!aircraftMutex) return -1;
  if (xSemaphoreTake(aircraftMutex, pdMS_TO_TICKS(10)) != pdTRUE) return -1;

  int8_t bestIdx = -1;
  int    bestD2  = 25 * 25;
  for (uint8_t i = 0; i < aircraftCount; ++i) {
    if (!aircraftList[i].in_range) continue;
    int16_t x, y;
    polarToScreen(aircraftList[i].bearing,
                  aircraftList[i].distance_units, x, y);
    int dx = x - tx, dy = y - ty;
    int d2 = dx * dx + dy * dy;
    if (d2 < bestD2) { bestD2 = d2; bestIdx = (int8_t)i; }
  }
  xSemaphoreGive(aircraftMutex);
  return bestIdx;
}

// ============== End of Part 2 — continue with Part 3 ============
// ================================================================
// FlightRadar.ino  —  Part 3 of 3
// ----------------------------------------------------------------
// Touch/gesture handling, IMU rotation + shake-to-wake,
// WiFi/OTA/mDNS/web portal, setup() and loop().
// ================================================================

// ================================================================
//                Touch Gesture State Machine
// ----------------------------------------------------------------
// We see the touch controller from readTouch() as a stream of
// (down/up + x,y) samples. A small state machine turns that into:
//   tap, long-press, swipe (4 directions), and edge-brightness drag.
// ================================================================
enum TouchState : uint8_t {
  TS_IDLE = 0,
  TS_DOWN,           // finger down, not yet committed to a gesture
  TS_DRAGGING,       // movement detected — will resolve to swipe on release
  TS_EDGE_BRIGHT,    // dragging around the outer ring → brightness control
};

struct TouchTracker {
  TouchState state;
  int16_t   startX, startY;
  int16_t   lastX,  lastY;
  uint32_t  startMs;
  bool      longPressFired;
  float     lastAngleDeg;     // for edge-brightness rotational drag
} touch;

inline float angleFromCenter(int16_t x, int16_t y) {
  // 0° = up, +clockwise — matches our radar bearing convention
  return atan2f((float)(x - CX), -(float)(y - CY)) * 57.295779513f;
}
inline int16_t distFromCenter(int16_t x, int16_t y) {
  int dx = x - CX, dy = y - CY;
  return (int16_t)sqrtf((float)(dx * dx + dy * dy));
}

// Any user input also wakes a dimmed screen
void noteInteraction() {
  lastInteractionMs = millis();
  if (screenDimmed) {
    screenDimmed = false;
    setBrightness(preDimBrightness);
  }
}

// ---- gesture handlers --------------------------------------------
void onTap(int16_t x, int16_t y) {
  // Serial.printf("[Tap] screen=%d at (%d,%d)\n", (int)currentScreen, x, y);  // Uncomment line for tap debugging
  noteInteraction();
  chirpTap();
  switch (currentScreen) {
    case SCREEN_RADAR: {
      int8_t idx = findAircraftNearTouch(x, y);
      if (idx >= 0) {
        selectedAircraft = idx;
        currentScreen    = SCREEN_FLIGHT_DETAIL;
      }
      break;
    }
    case SCREEN_FLIGHT_DETAIL:
      currentScreen = SCREEN_RADAR;
      break;
    case SCREEN_SETTINGS: {
      int row = settingsRowHit(y);
      if (row >= 0) cycleSettingsRow(row);
      break;
    }
    default: break;
  }
}

void onLongPress() {
  noteInteraction();
  chirpTap();
  if      (currentScreen == SCREEN_RADAR)    currentScreen = SCREEN_SETTINGS;
  else if (currentScreen == SCREEN_SETTINGS) currentScreen = SCREEN_RADAR;
  else if (currentScreen == SCREEN_CLOCK)    currentScreen = SCREEN_SETTINGS;
  else if (currentScreen == SCREEN_STATS)    currentScreen = SCREEN_SETTINGS;
}

// dir: 0=up, 1=right, 2=down, 3=left
void onSwipe(uint8_t dir) {
  noteInteraction();
  switch (currentScreen) {
    case SCREEN_RADAR:
      if      (dir == 3) currentScreen = SCREEN_STATS;
      else if (dir == 2) currentScreen = SCREEN_CLOCK;
      break;
    case SCREEN_CLOCK:
      if      (dir == 1) currentScreen = SCREEN_STATS;
      else if (dir == 0) currentScreen = SCREEN_RADAR;
      else if (dir == 2) currentScreen = SCREEN_RADAR;
      break;
    case SCREEN_STATS:
      if      (dir == 3) currentScreen = SCREEN_RADAR;
      else if (dir == 1) currentScreen = SCREEN_RADAR;
      else if (dir == 0) currentScreen = SCREEN_CLOCK;
      break;
    case SCREEN_FLIGHT_DETAIL:
    case SCREEN_SETTINGS:
      if (dir == 1 || dir == 2) currentScreen = SCREEN_RADAR;
      break;
    default: break;
  }
}

// Edge-brightness: drag around the outer ring, clockwise brightens.
void updateEdgeBrightness(int16_t x, int16_t y) {
  float ang = angleFromCenter(x, y);
  float delta = ang - touch.lastAngleDeg;
  if (delta >  180.0f) delta -= 360.0f;
  if (delta < -180.0f) delta += 360.0f;
  touch.lastAngleDeg = ang;

  int newB = (int)settings.brightness + (int)(delta * 0.8f);
  if (newB < BRIGHTNESS_MIN) newB = BRIGHTNESS_MIN;
  if (newB > BRIGHTNESS_MAX) newB = BRIGHTNESS_MAX;
  settings.brightness = (uint8_t)newB;
  setBrightness(settings.brightness);
}

// ---- main poll routine -------------------------------------------
void pollTouchAndHandleGestures() {
  TouchPoint tp;
  bool down = readTouch(&tp);
  uint32_t now = millis();

  if (down) {
    if (touch.state == TS_IDLE) {
      touch.state          = TS_DOWN;
      touch.startX         = tp.x;
      touch.startY         = tp.y;
      touch.lastX          = tp.x;
      touch.lastY          = tp.y;
      touch.startMs        = now;
      touch.longPressFired = false;
      // Touched near the outer edge? Switch to brightness-drag mode.
      if (distFromCenter(tp.x, tp.y) > LCD_RADIUS - 30) {
        touch.state        = TS_EDGE_BRIGHT;
        touch.lastAngleDeg = angleFromCenter(tp.x, tp.y);
        noteInteraction();
      }
    } else if (touch.state == TS_DOWN) {
      int dx = tp.x - touch.startX;
      int dy = tp.y - touch.startY;
      // Long press: held > 500ms with minimal movement
      if (!touch.longPressFired && (now - touch.startMs) > 500 &&
          abs(dx) < 15 && abs(dy) < 15) {
        touch.longPressFired = true;
        onLongPress();
      }
      // Promote to dragging once finger moves enough
      if (abs(dx) > 25 || abs(dy) > 25) {
        touch.state = TS_DRAGGING;
        touch.lastX = tp.x;
        touch.lastY = tp.y;
      }
    } else if (touch.state == TS_DRAGGING) {
      touch.lastX = tp.x;
      touch.lastY = tp.y;
    } else if (touch.state == TS_EDGE_BRIGHT) {
      updateEdgeBrightness(tp.x, tp.y);
    }
  } else {
    // Touch released — single unified resolver.
    //   - edge-brightness drag: just persist the new brightness
    //   - long-press already fired: do nothing on release
    //   - otherwise: swipe if the finger moved far enough in one axis,
    //     tap if it didn't. No dead zone between the two.
    if (touch.state == TS_EDGE_BRIGHT) {
      saveSettings();
    } else if (!touch.longPressFired && touch.state != TS_IDLE) {
      // Total movement from touch-down to wherever the finger ended up.
      // In TS_DRAGGING the last sample lives in lastX/Y; in TS_DOWN it's
      // still the start position, which means dx=dy=0 and we treat it
      // as a tap.
      int endX = (touch.state == TS_DRAGGING) ? touch.lastX : touch.startX;
      int endY = (touch.state == TS_DRAGGING) ? touch.lastY : touch.startY;
      int dx = endX - touch.startX;
      int dy = endY - touch.startY;

      if (abs(dx) > 60 && abs(dx) >= abs(dy)) {
        onSwipe(dx > 0 ? 1 : 3);                 // left/right
      } else if (abs(dy) > 60 && abs(dy) > abs(dx)) {
        onSwipe(dy > 0 ? 2 : 0);                 // down/up
      } else {
        onTap(touch.startX, touch.startY);       // anything else = tap
      }
    }
    touch.state = TS_IDLE;
}
}

// ================================================================
//                  IMU: Auto-Rotate + Shake-to-Wake
// ================================================================
struct ImuState {
  uint8_t  pendingRotation;
  uint32_t pendingSince;
  float    lastMag;
} imuState;

void updateImuRotation() {
  float ax, ay, az;
  if (!imu.getAccelerometer(ax, ay, az)) return;

  // Shake-to-wake only — auto-rotate disabled for now.
  float mag  = sqrtf(ax * ax + ay * ay + az * az);
  float jerk = fabsf(mag - imuState.lastMag);
  imuState.lastMag = mag;
  if (/*screenDimmed &&*/ jerk > 0.05f) noteInteraction();

  //  Re-enable when ready:
  uint8_t want = displayRotation;
  if (fabsf(az) < 0.7f) {
    bool ax_dom = fabsf(ax) > 0.5f && fabsf(ax) > fabsf(ay) * 1.3f;
    bool ay_dom = fabsf(ay) > 0.5f && fabsf(ay) > fabsf(ax) * 1.3f;
    if      (ax_dom) want = (ax > 0) ? 0 : 2;
    else if (ay_dom) want = (ay > 0) ? 1 : 3;
  }
  uint32_t now = millis();
  if (want != displayRotation) {
    if (imuState.pendingRotation != want) {
      imuState.pendingRotation = want;
      imuState.pendingSince    = now;
    } else if (now - imuState.pendingSince > 500) {
      displayRotation = want;
      if (gfx)    gfx->setRotation(want);
      if (canvas) canvas->setRotation(want);
    }
  } else {
    imuState.pendingRotation = want;
  }
  //
}

// ================================================================
//                  Idle / Auto-Dim
// ----------------------------------------------------------------
// If no input for `idle_timeout_sec`, drop the backlight to minimum.
// Any tap, swipe, or shake calls noteInteraction() which restores it.
// ================================================================
void checkIdleDim() {
  if (settings.idle_timeout_sec == 0) return;
  uint32_t idleMs = millis() - lastInteractionMs;
  if (!screenDimmed && idleMs > settings.idle_timeout_sec * 1000UL) {
    preDimBrightness = settings.brightness;
    screenDimmed = true;
    setBrightness(BRIGHTNESS_MIN);
  }
}

// ================================================================
//                  WiFi + Captive Portal
// ----------------------------------------------------------------
// On first boot (or after "Forget WiFi"), WiFiManager opens an
// access point named "FlightRadar-XXXX" (last 4 hex of MAC). Joining
// it pops up a captive portal where the user enters WiFi creds plus
// our custom location/timezone parameters.
// ================================================================
void buildApSsid() {
  uint64_t mac = ESP.getEfuseMac();
  // High 16 bits of the upper 32 give us a stable 4-hex unique
  // suffix that differs board-to-board (so a classroom full of these
  // boards each gets its own SSID).
  uint16_t suffix = (uint16_t)((mac >> 24) & 0xFFFF);
  char buf[24];
  snprintf(buf, sizeof(buf), AP_PREFIX "%04X", suffix);
  apSsid = String(buf);
}

void initWiFiAndPortal() {
  buildApSsid();

  WiFiManager wm;
  wm.setConfigPortalTimeout(300);          // close portal after 5 min
  wm.setHostname(MDNS_HOSTNAME);
  wm.setBreakAfterConfig(true);

  // Custom fields shown beside the WiFi credentials in the portal
  char latBuf[12], lonBuf[12], tzBuf[8];
  snprintf(latBuf, sizeof(latBuf), "%.4f", settings.home_lat);
  snprintf(lonBuf, sizeof(lonBuf), "%.4f", settings.home_lon);
  snprintf(tzBuf,  sizeof(tzBuf),  "%d",   settings.tz_offset_seconds / 3600);

  WiFiManagerParameter pLat("lat", "Home latitude  (e.g. 41.0793)",  latBuf, 11);
  WiFiManagerParameter pLon("lon", "Home longitude (e.g. -85.1394)", lonBuf, 11);
  WiFiManagerParameter pTz ("tz",  "Timezone offset hours (e.g. -5)", tzBuf,  4);
  wm.addParameter(&pLat);
  wm.addParameter(&pLon);
  wm.addParameter(&pTz);

  // Show our own AP-mode screen while the portal is open
  apMode        = true;
  currentScreen = SCREEN_AP_MODE;
  renderApMode();

  bool ok = wm.autoConnect(apSsid.c_str());
  apMode = false;

  // Pull any edits to our custom params back into settings
  settings.home_lat          = atof(pLat.getValue());
  settings.home_lon          = atof(pLon.getValue());
  settings.tz_offset_seconds = atoi(pTz.getValue()) * 3600;
  saveSettings();

  wifiConnected = ok;
  if (ok) {
    currentScreen = SCREEN_RADAR;
    Serial.printf("[WiFi] connected — IP=%s\n",
                  WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[WiFi] no creds saved, running offline");
  }
}

// Configure timezone + start the SNTP client.  When NTP lands, we
// back up the time to the PCF85063 RTC so the clock keeps running
// even if WiFi drops out later.
void initTime() {
  configTime(settings.tz_offset_seconds, 0,
             "pool.ntp.org", "time.nist.gov");
  sntp_set_time_sync_notification_cb([](timeval*) { syncRtcFromSystem(); });
}

// ================================================================
//                          OTA Updates
// ----------------------------------------------------------------
// ArduinoOTA listens on UDP/3232 once WiFi is up. From the Arduino
// IDE: Tools → Port → "flightradar at <ip>" appears as a network
// port.  Password is whatever's saved in NVS (default "flightradar").
// ================================================================
void initOTA() {
  ArduinoOTA.setHostname(MDNS_HOSTNAME);
  if (settings.ota_password[0])
    ArduinoOTA.setPassword(settings.ota_password);

  ArduinoOTA.onStart([]() {
    if (!gfx) return;
    gfx->fillScreen(BLACK);
    gfx->setTextSize(TXT_LARGE);
    gfx->setTextColor(WHITE);
    gfx->setCursor(CX - 80, CY - 10);
    gfx->print("OTA...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if (!gfx) return;
    int pct = (progress * 100) / total;
    gfx->fillRect(CX - 100, CY + 30, 200, 14, BLACK);
    gfx->drawRect(CX - 100, CY + 30, 200, 14, WHITE);
    gfx->fillRect(CX -  98, CY + 32, (196 * pct) / 100, 10, WHITE);
  });
  ArduinoOTA.onEnd([]() {
    if (!gfx) return;
    gfx->fillScreen(BLACK);
    gfx->setTextSize(TXT_MEDIUM);
    gfx->setTextColor(WHITE);
    gfx->setCursor(CX - 60, CY - 8);
    gfx->print("Updated!");
  });
  ArduinoOTA.onError([](ota_error_t e) {
    Serial.printf("[OTA] error %u\n", e);
  });
  ArduinoOTA.begin();
  Serial.println("[OTA] ready");
}

// ================================================================
//                          Web Portal
// ----------------------------------------------------------------
// Once on WiFi, browse to http://flightradar.local/ (or the device
// IP). The page renders the current settings, lets you change them,
// and saves to NVS.  PORTAL_HTML lives in flash to keep RAM free.
// ================================================================
const char THEME_GREEN_CSS[] PROGMEM = R"rawliteral(
  :root {
    --bg: #0a0f0a;
    --fg: #33ff33;
    --accent: #1a331a;
    --input-bg: #0f1a0f;
    --input-border: #33ff33;
    --button-bg: #33ff33;
    --button-fg: #0a0f0a;
    --link: #66ff66;
  }
  body::after {
    content: "";
    position: fixed;
    top: 0; left: 0; width: 100%; height: 100%;
    background: repeating-linear-gradient(
      0deg,
      rgba(0,0,0,0.15),
      rgba(0,0,0,0.15) 1px,
      transparent 1px,
      transparent 2px
    );
    pointer-events: none;
    z-index: 999;
  }
)rawliteral";

const char THEME_AMBER_CSS[] PROGMEM = R"rawliteral(
  :root {
    --bg: #1a0f00;
    --fg: #ffb000;
    --accent: #331a00;
    --input-bg: #1a0f00;
    --input-border: #ffb000;
    --button-bg: #ffb000;
    --button-fg: #1a0f00;
    --link: #ffcc33;
  }
  body::after {
    content: "";
    position: fixed;
    top: 0; left: 0; width: 100%; height: 100%;
    background: repeating-linear-gradient(
      0deg,
      rgba(0,0,0,0.15),
      rgba(0,0,0,0.15) 1px,
      transparent 1px,
      transparent 2px
    );
    pointer-events: none;
    z-index: 999;
  }
)rawliteral";

static const char PORTAL_HTML[] PROGMEM = R"HTML(
<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>FlightRadar</title>
<style>
 {{THEME_CSS}}
 body{
   font-family:'Courier New', monospace;
   background:var(--bg);
   color: var(--fg);
   max-width:520px;
   margin:0 auto;
   padding:14px }
 h1 {
   color:var(--fg);
   text-shadow: 0 0 5px var(--fg);
   margin:0 0 8px}
 h2 {
   color:var(--fg);
   margin-top:18px;
   text-shadow: 0 0 5px var(--fg);
   font-size:1.05em}
 label{display:block;margin-top:10px;font-size:.9em;color:#bdb}
 input,select {
   width:100%;
   background: var(--input-bg);
   border: 1px solid var(--input-border);
   color: var(--fg);
   padding:6px;
   border-radius:4px;
   box-sizing:border-box }
 input[type=checkbox]{width:auto}
 button{
   background: var(--button-bg);
   border: 1px solid var(--input-border);
   color: var(--button-fg);
   padding: 9px 14px;
   margin-top: 14px;
   border-radius: 4px;
   font-size: 1em;
   cursor: pointer}
 .row{display:flex;gap:8px} .row>div{flex:1}
 small {color:var(--fg)}
 a { color: var(--link) }
</style></head><body>
<h1>FlightRadar</h1>
<small>{{HOSTNAME}} &middot; IP {{IP}} &middot; uptime {{UPT}}</small>
<form method="POST" action="/save">
 <h2>Location</h2>
 <div class="row">
  <div><label>Latitude</label> <input name="lat" value="{{LAT}}"></div>
  <div><label>Longitude</label><input name="lon" value="{{LON}}"></div>
 </div>
 <h2>Display</h2>
 <label>Theme</label>           <select name="theme">{{THEME_OPTS}}</select>
 <label>Range</label>           <select name="range">{{RANGE_OPTS}}</select>
 <label>Units</label>           <select name="units">{{UNIT_OPTS}}</select>
 <label>Brightness (10-255)</label>
 <input type="number" name="bright" min="10" max="255" value="{{BRIGHT}}">
 <h2>Clock</h2>
 <label>Timezone hours from UTC (e.g. -5)</label>
 <input type="number" name="tz" value="{{TZ}}">
 <label><input type="checkbox" name="h24" {{H24}}> 24-hour format</label>
 <label><input type="checkbox" name="dst" {{DST}}> Observe DST</label>
 <h2>Behavior</h2>
 <label>Min altitude filter (ft)</label>
 <input type="number" name="minalt" value="{{MINALT}}">
 <label>Idle dim timeout (seconds, 0 = never)</label>
 <input type="number" name="idle" value="{{IDLE}}">
 <label><input type="checkbox" name="audio" {{AUDIO}}> Audio alerts</label>
 <label><input type="checkbox" name="demo"  {{DEMO}}> Demo mode</label>
 <label><input type="checkbox" name="log"   {{LOG}}> Log flights to flash</label>
 <h2>Updates</h2>
 <label>OTA password</label>
 <input name="otapw" value="{{OTAPW}}">
 <button type="submit">Save &amp; Apply</button>
</form>
<form method="POST" action="/restart">   <button>Reboot device</button></form>
<form method="POST" action="/wifi-reset"><button>Forget WiFi</button></form>
</body></html>
)HTML";

static String chk(bool v) { return v ? String("checked") : String(""); }

// Build a <select> option list with the right option marked selected.
static String buildOpts(const char* const* labels, int count, int selectedIdx) {
  String s;
  for (int i = 0; i < count; ++i) {
    s += "<option value=\"" + String(i) + "\"";
    if (i == selectedIdx) s += " selected";
    s += ">" + String(labels[i]) + "</option>";
  }
  return s;
}

static String renderPortalPage() {
  String html = FPSTR(PORTAL_HTML);

  // Theme options (from the THEMES array)
  String themeOpts;
  for (int i = 0; i < THEME_COUNT; ++i) {
    themeOpts += "<option value=\"" + String(i) + "\"";
    if (i == settings.theme_idx) themeOpts += " selected";
    themeOpts += ">" + String(THEMES[i]->name) + "</option>";
  }
  // Range options
  static const char* RANGE_LABELS[4] = {"5", "10", "25", "50"};
  String rangeOpts = buildOpts(RANGE_LABELS, 4, settings.range_idx);
  // Units options
  static const char* UNIT_LABELS[3] = {"Native (nm/kt)", "Miles (mi/mph)", "Metric (km/kmh)"};
  String unitOpts = buildOpts(UNIT_LABELS, 3, settings.unit_mode);

  uint32_t up = millis() / 1000;
  char uptBuf[24];
  snprintf(uptBuf, sizeof(uptBuf), "%luh %lum",
           (unsigned long)(up / 3600), (unsigned long)((up / 60) % 60));

  // Theme CSS - 0=Green Phosphor, 1=Amber CRT
  const char* themeCss = (settings.theme_idx == 1) ? THEME_AMBER_CSS : THEME_GREEN_CSS;
  html.replace("{{THEME_CSS}}",  themeCss);

  html.replace("{{HOSTNAME}}",   String(MDNS_HOSTNAME) + ".local");
  html.replace("{{IP}}",         WiFi.localIP().toString());
  html.replace("{{UPT}}",        uptBuf);
  html.replace("{{LAT}}",        String(settings.home_lat, 4));
  html.replace("{{LON}}",        String(settings.home_lon, 4));
  html.replace("{{THEME_OPTS}}", themeOpts);
  html.replace("{{RANGE_OPTS}}", rangeOpts);
  html.replace("{{UNIT_OPTS}}",  unitOpts);
  html.replace("{{BRIGHT}}",     String(settings.brightness));
  html.replace("{{TZ}}",         String(settings.tz_offset_seconds / 3600));
  html.replace("{{H24}}",        chk(settings.use_24h));
  html.replace("{{DST}}",        chk(settings.observe_dst));
  html.replace("{{MINALT}}",     String(settings.min_altitude_ft));
  html.replace("{{IDLE}}",       String(settings.idle_timeout_sec));
  html.replace("{{AUDIO}}",      chk(settings.audio_enabled));
  html.replace("{{DEMO}}",       chk(settings.demo_mode));
  html.replace("{{LOG}}",        chk(settings.log_to_fs));
  html.replace("{{OTAPW}}",      String(settings.ota_password));
  return html;
}

void handlePortalGet() {
  webServer.send(200, "text/html", renderPortalPage());
}

void handlePortalSave() {
  // Pull each form field, fall back to current value if missing.
  if (webServer.hasArg("lat"))    settings.home_lat   = webServer.arg("lat").toFloat();
  if (webServer.hasArg("lon"))    settings.home_lon   = webServer.arg("lon").toFloat();
  if (webServer.hasArg("theme"))  settings.theme_idx  = (uint8_t)webServer.arg("theme").toInt();
  if (webServer.hasArg("range"))  settings.range_idx  = (uint8_t)(webServer.arg("range").toInt() & 3);
  if (webServer.hasArg("units"))  settings.unit_mode  = (uint8_t)webServer.arg("units").toInt();
  if (webServer.hasArg("bright")) {
    settings.brightness = (uint8_t)webServer.arg("bright").toInt();
    setBrightness(settings.brightness);
  }
  if (webServer.hasArg("tz"))     settings.tz_offset_seconds = webServer.arg("tz").toInt() * 3600;
  // Checkboxes: absent argument == unchecked
  settings.use_24h        = webServer.hasArg("h24");
  settings.observe_dst    = webServer.hasArg("dst");
  settings.audio_enabled  = webServer.hasArg("audio");
  settings.demo_mode      = webServer.hasArg("demo");
  settings.log_to_fs      = webServer.hasArg("log");
  if (webServer.hasArg("minalt")) settings.min_altitude_ft  = webServer.arg("minalt").toInt();
  if (webServer.hasArg("idle"))   settings.idle_timeout_sec = webServer.arg("idle").toInt();
  if (webServer.hasArg("otapw"))
    strlcpy(settings.ota_password, webServer.arg("otapw").c_str(),
            sizeof(settings.ota_password));
  saveSettings();

  // Re-apply timezone immediately so the clock updates without reboot
  configTime(settings.tz_offset_seconds, 0,
             "pool.ntp.org", "time.nist.gov");

  // 302 → redirect back to the form so the user sees their values
  webServer.sendHeader("Location", "/", true);
  webServer.send(302, "text/plain", "Saved");
}

void handleRestart() {
  webServer.send(200, "text/plain", "Rebooting...");
  delay(500);
  ESP.restart();
}

void handleWifiReset() {
  webServer.send(200, "text/plain", "Clearing WiFi credentials...");
  WiFiManager wm;
  wm.resetSettings();
  delay(500);
  ESP.restart();
}

void initWebServer() {
  webServer.on("/",           HTTP_GET,  handlePortalGet);
  webServer.on("/save",       HTTP_POST, handlePortalSave);
  webServer.on("/restart",    HTTP_POST, handleRestart);
  webServer.on("/wifi-reset", HTTP_POST, handleWifiReset);
  webServer.begin();
  Serial.println("[HTTP] web server up on port 80");
}

// Uncomment for additional debugging
/*void setup_test() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[MIN] starting");

  // Bring up I2C + expander first ONLY because initDisplay() needs
  // the expander to have already released the LCD_RST line. Nothing
  // else.
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);
  initExpander();
  loadSettings();              // needed so setBrightness() has a value
  initDisplay();

  if (!gfx || !canvas) {
    Serial.println("[MIN] display init failed — stopping");
    while (1) delay(1000);
  }

  Serial.println("[MIN] drawing RED");
  gfx->fillScreen(0xF800);
  canvas->flush();
  delay(2500);

  Serial.println("[MIN] drawing GREEN");
  gfx->fillScreen(0x07E0);
  canvas->flush();
  delay(2500);

  Serial.println("[MIN] drawing BLUE");
  gfx->fillScreen(0x001F);
  canvas->flush();
  delay(2500);

  Serial.println("[MIN] drawing HELLO");
  gfx->fillScreen(0x0000);
  gfx->setTextSize(TXT_XLARGE);
  gfx->setTextColor(0xFFFF);
  gfx->setCursor(70, 200);
  gfx->print("HELLO");
  canvas->flush();
  Serial.println("[MIN] done — holding");
}*/

// ================================================================
//                          setup()
// ----------------------------------------------------------------
// Bring up hardware in the right order: I2C → expander → display →
// touch → sensors → storage → audio → WiFi → time → OTA → web.
// Then spawn the background ADS-B fetch task on core 0.
// ================================================================
void setup() {
  // Keep power on (required for battery operation on Waveshare boards)
  pinMode(PWR_Control_PIN, OUTPUT);
  digitalWrite(PWR_Control_PIN, HIGH);
  pinMode(PWR_KEY_Input_PIN, INPUT);  // Initialize button monitoring
  Serial.begin(115200);
  delay(300);
  Serial.println("\n[MIN] starting");
  Serial.printf("[Mem] PSRAM size: %u bytes\n", (unsigned)ESP.getPsramSize());
  Serial.printf("[Mem] Free PSRAM: %u bytes\n", (unsigned)ESP.getFreePsram());
  Serial.printf("[Mem] Free heap:  %u bytes\n", (unsigned)ESP.getFreeHeap());
  delay(200);
  Serial.println("\n[Boot] FlightRadar starting…");
  Serial.printf("[Boot] %s\n", BOARD_NAME);

  aircraftMutex = xSemaphoreCreateMutex();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQ);

  initExpander();
  loadSettings();

  // Peripherals first so I2S/etc. can't clobber the QSPI DMA setup
  initTouch();
  initIMU();
  initRTC();
  initStorage();
  initAudio();

  // Display LAST so its DMA allocation is the final one and nothing
  // overwrites it on the way to loop().
  initDisplay();

  // Stats baseline
  memset(&stats, 0, sizeof(stats));
  stats.boot_time_unix = time(nullptr);
  lastInteractionMs    = millis();
  lastSweepMs          = millis();

  // Splash so the user sees something while WiFi negotiates
  if (gfx) {
    gfx->fillScreen(BLACK);
    gfx->setTextSize(TXT_MEDIUM);
    gfx->setTextColor(WHITE);
    gfx->setCursor(CX - 70, CY - 8);
    gfx->print("FlightRadar");
    canvas->flush();
  }

  initWiFiAndPortal();

  if (wifiConnected) {
    if (MDNS.begin(MDNS_HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("[mDNS] %s.local\n", MDNS_HOSTNAME);
    }
    initTime();
    initOTA();
    initWebServer();
  }

  xTaskCreatePinnedToCore(
      fetchAdsbTask, "adsb_fetch",
      12288, nullptr, 1, nullptr, 0);

  Serial.println("[Boot] ready");
}

// ================================================================
//                          loop()
// ----------------------------------------------------------------
// Order matters here:
//   1. Service network handlers so OTA & web stay responsive
//   2. Read input (touch + IMU)
//   3. Auto-dim check
//   4. Advance sweep animation
//   5. Render one frame (capped at ~30 fps)
//   6. yield()
// ================================================================
void loop() {
  if (wifiConnected) {
    ArduinoOTA.handle();
    webServer.handleClient();
  }

  pollTouchAndHandleGestures();
  updateImuRotation();
  checkIdleDim();
  advanceSweep();
  PWR_Loop();  // Handles long-press detection

  // Render at ~30 fps — plenty smooth, leaves CPU for everything else
  static uint32_t lastFrameMs = 0;
  uint32_t now = millis();
  if (now - lastFrameMs >= 33) {
    lastFrameMs = now;
    switch (currentScreen) {
      case SCREEN_RADAR:         renderRadar();           break;
      case SCREEN_FLIGHT_DETAIL: renderFlightDetail();    break;
      case SCREEN_CLOCK:         renderClock();           break;
      case SCREEN_STATS:         renderStats();           break;
      case SCREEN_SETTINGS:      renderSettingsOverlay(); break;
      case SCREEN_AP_MODE:       renderApMode();          break;
    }
  }

  // Yield so the ADS-B task, mDNS, OTA etc. get CPU time
  delay(2);
}

// ============== End of sketch ===================================