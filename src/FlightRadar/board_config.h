// ================================================================
// board_config.h
// ----------------------------------------------------------------
// Hardware pin and feature configuration. To support a new board,
// add a new `#elif defined(BOARD_xxx)` block below — no other file
// needs to change.
// ================================================================
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

// ── Board selection ──────────────────────────────────────────────
// Pick exactly ONE board. For class we use the Waveshare 1.46C.
#define BOARD_WAVESHARE_S3_146C   // SKU 33837

// ================================================================
// Waveshare ESP32-S3-Touch-LCD-1.46C  (SKU 33837)
// Source: https://docs.waveshare.com/ESP32-S3-Touch-LCD-1.46
// ================================================================
#if defined(BOARD_WAVESHARE_S3_146C)

  #define BOARD_NAME "Waveshare ESP32-S3-Touch-LCD-1.46C"

  // ── Display (SPD2010 controller, QSPI bus, 412x412 round) ──────
  // QSPI sends 4 data bits per clock — much faster than regular SPI.
  #define LCD_WIDTH       412
  #define LCD_HEIGHT      412
  #define LCD_RADIUS      206         // Half of 412 — center is (206,206)
  #define LCD_BL_PIN       5          // Backlight (PWM via LEDC)
  #define LCD_CS_PIN      21
  #define LCD_SCK_PIN     40
  #define LCD_D0_PIN      46
  #define LCD_D1_PIN      45
  #define LCD_D2_PIN      42
  #define LCD_D3_PIN      41
  #define LCD_TE_PIN      18          // Tearing-effect signal (vsync)
  // LCD_RST is on the I/O expander (EXIO2), not a real GPIO.

  // ── Touch (SPD2010 over I2C) ───────────────────────────────────
  // The display and touch are the same chip. Display uses QSPI,
  // touch uses the shared I2C bus. The exact touch I2C address and
  // register map should be cross-checked against Waveshare's
  // example sketch — placeholder values are noted below.
  #define TOUCH_I2C_ADDR  0x53        // VERIFY against Waveshare demo
  #define TOUCH_INT_PIN    4          // Active-low interrupt
  // TOUCH_RST is on the I/O expander (EXIO1).

  // ── Shared I2C bus (Touch + IMU + RTC + I/O expander) ──────────
  #define I2C_SDA_PIN     11
  #define I2C_SCL_PIN     10
  #define I2C_FREQ        400000      // 400 kHz fast mode

  // ── I/O expander: TCA9554PWR ──────────────────────────────────
  // Adds 8 extra "EXIO" pins that we control via I2C writes.
  // A0=A1=A2=GND on this board → base address 0x20.
  #define TCA9554_ADDR    0x20
  #define EXIO_TP_RST     1           // Touch reset
  #define EXIO_LCD_RST    2           // Display reset
  #define EXIO_SD_CS      3           // SD card chip-select
  #define EXIO_IMU_INT2   4           // IMU interrupt 2
  #define EXIO_IMU_INT1   5           // IMU interrupt 1

  // ── IMU: QMI8658 (6-axis: accel + gyro) ────────────────────────
  // Used for auto-rotate and shake-to-wake.
  #define IMU_I2C_ADDR    0x6B        // SDO pulled high on this board

  // ── RTC: PCF85063ATL ───────────────────────────────────────────
  // Keeps time when WiFi/NTP is unavailable, backed by a coin cell.
  #define RTC_I2C_ADDR    0x51
  #define RTC_INT_PIN      9

  // ── TF (microSD) card over SPI ────────────────────────────────
  // The CS pin is on the I/O expander, so we use LittleFS as the
  // primary log target and treat SD logging as optional/advanced.
  #define SD_MISO_PIN     16
  #define SD_MOSI_PIN     17
  #define SD_SCK_PIN      14
  // SD_CS lives on EXIO3 → toggled manually around SPI transactions.

  // ── Audio out: I2S to PCM5101 DAC → onboard speaker ────────────
  #define I2S_DIN_PIN     47
  #define I2S_LRCK_PIN    38
  #define I2S_BCK_PIN     48

  // ── Microphone (unused in this project but documented) ─────────
  #define MIC_WS_PIN       2
  #define MIC_SCK_PIN     15
  #define MIC_SD_PIN      39

  // ── Buttons ───────────────────────────────────────────────────
  #define BTN_BOOT_PIN     0          // BOOT button — used as user button

  // ── Battery monitoring ────────────────────────────────────────
  // The schematic shows a battery sense divider; verify the pin on
  // the production schematic before enabling.
  #define BATT_ADC_PIN    -1          // -1 = disabled until confirmed
  #define BATT_DIVIDER    2.0f

#else
  #error "No board selected — define BOARD_WAVESHARE_S3_146C or add a new board block."
#endif

// ================================================================
// Board-independent constants
// ================================================================
#define BRIGHTNESS_MIN      10
#define BRIGHTNESS_MAX     255
#define BRIGHTNESS_DEFAULT 180
#define LEDC_BL_CHANNEL      0
#define LEDC_BL_FREQ      5000
#define LEDC_BL_RES_BITS     8

// Default home location: Fort Wayne, IN (Maker's Space)
#define DEFAULT_LAT       41.0793f
#define DEFAULT_LON      -85.1394f

// ADS-B fetch behavior
#define ADSB_FETCH_INTERVAL_MS  5000   // adsb.fi update cadence
#define ADSB_MAX_AIRCRAFT       64     // Per refresh — RAM budget
#define ADSB_HOST       "opendata.adsb.fi"
#define ADSB_PATH_FMT   "/api/v3/lat/%.4f/lon/%.4f/dist/%d"  // distance in NM

// Network identity
#define AP_PREFIX        "FlightRadar-"
#define MDNS_HOSTNAME    "flightradar"
#define OTA_DEFAULT_PASS "flightradar"   // Override in web portal

// ================================================================
// Shared types that need to be visible before Arduino IDE
// auto-generates function prototypes. Anything used in a top-level
// function signature must live in a header, not in the .ino body.
// ================================================================

// ---- Touch (SPD2010 over I2C) ----------------------------------
// NOTE: The SPD2010 touch register map is not in any public datasheet
// I can point to. The protocol below follows the same shape as the
// Waveshare LVGL demo. If you see no touches, the most likely culprit
// is TOUCH_I2C_ADDR or the start register (0x02). Cross-check with
// the Waveshare example sketch.
struct TouchPoint {
  int16_t x;
  int16_t y;
  bool    down;
};

// Arduino_GFX doesn't define these as plain macros in this version
#ifndef BLACK
  #define BLACK  0x0000
#endif
#ifndef WHITE
  #define WHITE  0xFFFF
#endif

#endif // BOARD_CONFIG_H