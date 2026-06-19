// ================================================================
// FlightRadar.ino  —  Part 1 of 3
// ----------------------------------------------------------------
// Local Flight Radar for Waveshare ESP32-S3-Touch-LCD-1.46C
// The Maker's Space — educational class project
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
//   Board:       ESP32S3 Dev Module
//   Flash Size:  16MB
//   PSRAM:       OPI PSRAM
//   Partition:   16M Flash (3MB APP / 9.9MB FATFS)  or similar w/ LittleFS
//   USB Mode:    Hardware CDC and JTAG
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