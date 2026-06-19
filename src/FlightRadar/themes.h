// ================================================================
// themes.h
// ----------------------------------------------------------------
// Color palettes for the radar display. Each theme is a single
// struct so every color element is in one place — easy for the
// class to read, easy to add new themes later.
// ================================================================
#ifndef THEMES_H
#define THEMES_H

#include <Arduino.h>

// RGB565 packs a 24-bit color into 16 bits (5 red, 6 green, 5 blue)
#define RGB565(r,g,b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b) >> 3)))

#define THEME_GREEN_PHOSPHOR  0
#define THEME_AMBER_CRT       1
#define THEME_COUNT           2

struct RadarTheme {
  const char* name;

  uint16_t background;
  uint16_t radarRing;
  uint16_t radarRingDim;
  uint16_t sweepLine;
  uint16_t sweepFade;
  uint16_t crosshair;
  uint16_t compassRose;

  // Aircraft blip colors by altitude band
  uint16_t blipLow;        // < 5,000 ft
  uint16_t blipMid;        // 5,000–18,000 ft
  uint16_t blipHigh;       // 18,000–35,000 ft
  uint16_t blipVeryHigh;   // > 35,000 ft
  uint16_t blipSelected;
  uint16_t velocityVector;

  // Emergency squawk colors
  uint16_t squawk7700;     // General emergency
  uint16_t squawk7600;     // Radio failure
  uint16_t squawk7500;     // Hijack

  // UI
  uint16_t textPrimary;
  uint16_t textSecondary;
  uint16_t textDim;
  uint16_t panelBg;
  uint16_t panelBorder;
  uint16_t buttonBg;
  uint16_t buttonText;

  // Clock screen
  uint16_t clockFace;
  uint16_t clockSecond;
  uint16_t clockDate;

  // Battery indicator
  uint16_t battHigh;
  uint16_t battMid;
  uint16_t battLow;
};

// ---- Green Phosphor (classic radar CRT) ----------------------
static const RadarTheme THEME_GREEN = {
  "Green Phosphor",
  RGB565(  0,   8,   0),  // background
  RGB565(  0, 100,   0),  RGB565(  0,  40,   0),
  RGB565( 80, 255,  80),  RGB565(  0,  60,   0),
  RGB565(  0,  80,   0),  RGB565(  0, 160,   0),
  RGB565( 30, 140,  30),  RGB565( 60, 220,  60),
  RGB565(120, 255, 120),  RGB565(200, 255, 200),
  RGB565(255, 255, 255),  RGB565( 80, 200,  80),
  RGB565(255, 120,   0),  RGB565(255, 220,   0),  RGB565(255,  30,  30),
  RGB565( 80, 255,  80),  RGB565( 60, 180,  60),  RGB565( 30, 100,  30),
  RGB565(  0,  20,   0),  RGB565(  0, 120,   0),
  RGB565(  0,  60,   0),  RGB565( 80, 255,  80),
  RGB565( 80, 255,  80),  RGB565(200, 255, 200),  RGB565( 60, 180,  60),
  RGB565( 60, 220,  60),  RGB565(220, 220,  60),  RGB565(220,  60,  60),
};

// ---- Amber CRT (vintage terminal) ----------------------------
static const RadarTheme THEME_AMBER = {
  "Amber CRT",
  RGB565(  8,   4,   0),
  RGB565(160,  80,   0),  RGB565( 60,  30,   0),
  RGB565(255, 160,   0),  RGB565( 80,  40,   0),
  RGB565( 80,  40,   0),  RGB565(200, 100,   0),
  RGB565(160,  70,   0),  RGB565(220, 120,   0),
  RGB565(255, 180,  30),  RGB565(255, 220, 120),
  RGB565(255, 255, 255),  RGB565(220, 140,  20),
  RGB565(255,  60,   0),  RGB565(255, 240,   0),  RGB565(255,  20,  20),
  RGB565(255, 160,   0),  RGB565(200, 110,   0),  RGB565(100,  50,   0),
  RGB565( 20,  10,   0),  RGB565(160,  80,   0),
  RGB565( 60,  30,   0),  RGB565(255, 160,   0),
  RGB565(255, 160,   0),  RGB565(255, 220, 120),  RGB565(200, 110,   0),
  RGB565(220, 160,   0),  RGB565(220, 220,  60),  RGB565(220,  60,  60),
};

static const RadarTheme* const THEMES[THEME_COUNT] = {
  &THEME_GREEN,
  &THEME_AMBER,
};

#endif // THEMES_H