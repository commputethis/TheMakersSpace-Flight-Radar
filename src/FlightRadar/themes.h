#ifndef THEMES_H
#define THEMES_H

#include <Arduino_GFX.h>
#include "ui_layout.h"

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

// Theme index enum
enum ThemeIndex {
  THEME_GREEN = 0,
  THEME_AMBER,
  THEME_DEEP_BLUE,
  THEME_CRIMSON,
  THEME_NOIR,
  THEME_COUNT
};

// ---- Green Phosphor (classic radar CRT) ----------------------
static const RadarTheme THEME_GREEN_DATA PROGMEM = {
  "Green Phosphor",       // name
  RGB565(  0,   8,   0),  // background
  RGB565(  0, 100,   0),  // radarRing
  RGB565(  0,  40,   0),  // radarRingDim
  RGB565(  0,  60,   0),  // sweepLine
  RGB565(  0,  30,   0),  // sweepFade
  RGB565( 80, 255,  80),  // crosshair
  RGB565( 60, 180,  60),  // compassRose
  RGB565(  0,  80,   0),  // blipLow
  RGB565(  0, 160,   0),  // blipMid
  RGB565( 30, 140,  30),  // blipHigh
  RGB565( 60, 220,  60),  // blipVeryHigh
  RGB565(120, 255, 120),  // blipSelected
  RGB565(200, 255, 200),  // velocityVector
  RGB565(255,  30,  30),  // squawk7700
  RGB565(255, 220,   0),  // squawk7600
  RGB565(255, 120,   0),  // squawk7500
  RGB565(200, 255, 200),  // textPrimary
  RGB565(255, 255, 255),  // textSecondary
  RGB565( 80, 200,  80),  // textDim
  RGB565(  0,  40,   0),  // panelBg
  RGB565(  0, 100,   0),  // panelBorder
  RGB565( 80, 255,  80),  // buttonBg
  RGB565(  0,   0,   0),  // buttonText
  RGB565(255, 220,   0),  // clockFace
  RGB565( 80, 255,  80),  // clockSecond
  RGB565(255,  30,  30),  // clockDate
  RGB565( 80, 255,  80),  // battHigh
  RGB565( 60, 180,  60),  // battMid
  RGB565( 30, 100,  30),  // battLow
};

// ---- Amber CRT (vintage terminal) ----------------------------
static const RadarTheme THEME_AMBER_DATA PROGMEM = {
  "Amber CRT",            // name
  RGB565(  8,   4,   0),  // background
  RGB565(160,  80,   0),  // radarRing
  RGB565( 60,  30,   0),  // radarRingDim
  RGB565( 80,  40,   0),  // sweepLine
  RGB565( 40,  20,   0),  // sweepFade
  RGB565(255, 160,   0),  // crosshair
  RGB565(200, 110,   0),  // compassRose
  RGB565( 80,  40,   0),  // blipLow
  RGB565(200, 100,   0),  // blipMid
  RGB565(160,  70,   0),  // blipHigh
  RGB565(220, 120,   0),  // blipVeryHigh
  RGB565(255, 180,  30),  // blipSelected
  RGB565(255, 220, 120),  // velocityVector
  RGB565(255,  20,  20),  // squawk7700
  RGB565(255, 240,   0),  // squawk7600
  RGB565(255,  60,   0),  // squawk7500
  RGB565(255, 220, 120),  // textPrimary
  RGB565(255, 255, 255),  // textSecondary
  RGB565(220, 140,  20),  // textDim
  RGB565( 60,  30,   0),  // panelBg
  RGB565(160,  80,   0),  // panelBorder
  RGB565(255, 160,   0),  // buttonBg
  RGB565(  0,   0,   0),  // buttonText
  RGB565(255, 240,   0),  // clockFace
  RGB565(255, 160,   0),  // clockSecond
  RGB565(255,  20,  20),  // clockDate
  RGB565(255, 160,   0),  // battHigh
  RGB565(200, 110,   0),  // battMid
  RGB565(100,  50,   0),  // battLow
};

// ---- Deep Blue (cool cyan phosphor) -------------------------
static const RadarTheme THEME_DEEP_BLUE_DATA PROGMEM = {
  "Deep Blue",            // name
  RGB565(  0,   0,   8),  // background
  RGB565(  0,  64, 128),  // radarRing
  RGB565(  0,  32,  64),  // radarRingDim
  RGB565(  0,  60,   0),  // sweepLine
  RGB565(  0,  30,   0),  // sweepFade
  RGB565(  0,  64,  64),  // crosshair
  RGB565(  0,  64,  32),  // compassRose
  RGB565(  0,   0,  40),  // blipLow
  RGB565(  0, 128, 255),  // blipMid
  RGB565(  0, 200, 255),  // blipHigh
  RGB565(  0, 255, 240),  // blipVeryHigh
  RGB565(  0, 255, 255),  // blipSelected
  RGB565(  0, 255, 255),  // velocityVector
  RGB565(255,   0,   0),  // squawk7700
  RGB565(255, 255,   0),  // squawk7600
  RGB565(255, 100,   0),  // squawk7500
  RGB565(  0, 255, 255),  // textPrimary
  RGB565(  0, 200, 255),  // textSecondary
  RGB565(  0,  64, 128),  // textDim
  RGB565(  0,   0,  16),  // panelBg
  RGB565(  0,   0,  32),  // panelBorder
  RGB565(  0, 255, 255),  // buttonBg
  RGB565(  0,   0,   0),  // buttonText
  RGB565(  0, 255, 255),  // clockFace
  RGB565(  0, 128, 255),  // clockSecond
  RGB565(255, 100,   0),  // clockDate
  RGB565(  0, 255, 240),  // battHigh
  RGB565(  0, 200, 255),  // battMid
  RGB565(  0,  64, 128),  // battLow
};

// ---- Crimson (intense red phosphor) -------------------------
static const RadarTheme THEME_CRIMSON_DATA PROGMEM = {
  "Crimson",              // name
  RGB565( 32,   0,   0),  // background
  RGB565(128,   0,   0),  // radarRing
  RGB565( 64,   0,   0),  // radarRingDim
  RGB565( 88,   0,   0),  // sweepLine
  RGB565( 44,   0,   0),  // sweepFade
  RGB565( 88,   0,   0),  // crosshair
  RGB565( 88,   0,   0),  // compassRose
  RGB565( 72,   0,   0),  // blipLow
  RGB565(120,   0,   0),  // blipMid
  RGB565(176,   0,   0),  // blipHigh
  RGB565(255,   0,   0),  // blipVeryHigh
  RGB565(255,   0,   0),  // blipSelected
  RGB565(255,   0,   0),  // velocityVector
  RGB565(255, 255,   0),  // squawk7700 (yellow stands out)
  RGB565(255, 200,   0),  // squawk7600
  RGB565(255, 100,   0),  // squawk7500
  RGB565(255,   0,   0),  // textPrimary
  RGB565(255, 200, 200),  // textSecondary (pinkish white)
  RGB565( 88,   0,   0),  // textDim
  RGB565( 48,   0,   0),  // panelBg
  RGB565( 64,   0,   0),  // panelBorder
  RGB565(255,   0,   0),  // buttonBg
  RGB565(  0,   0,   0),  // buttonText
  RGB565(255,   0,   0),  // clockFace
  RGB565(255,   0,   0),  // clockSecond
  RGB565(255, 255,   0),  // clockDate (yellow)
  RGB565(255,   0,   0),  // battHigh
  RGB565(176,   0,   0),  // battMid
  RGB565(120,   0,   0),  // battLow
};

// ---- Noir (classic black & white) -------------------------
static const RadarTheme THEME_NOIR_DATA PROGMEM = {
  "Noir",                 // name
  RGB565(  0,   0,   0),  // background
  RGB565( 66,  66,  66),  // radarRing
  RGB565( 33,  33,  33),  // radarRingDim
  RGB565( 99,  99,  99),  // sweepLine
  RGB565( 50,  50,  50),  // sweepFade
  RGB565( 99,  99,  99),  // crosshair
  RGB565( 99,  99,  99),  // compassRose
  RGB565( 66,  66,  66),  // blipLow
  RGB565(132, 132, 132),  // blipMid
  RGB565(198, 198, 198),  // blipHigh
  RGB565(255, 255, 255),  // blipVeryHigh
  RGB565(255, 255, 255),  // blipSelected
  RGB565(255, 255, 255),  // velocityVector
  RGB565(255,   0,   0),  // squawk7700 (red stands out)
  RGB565(255, 255,   0),  // squawk7600 (yellow)
  RGB565(255, 100,   0),  // squawk7500 (orange)
  RGB565(255, 255, 255),  // textPrimary
  RGB565(220, 220, 220),  // textSecondary
  RGB565( 99,  99,  99),  // textDim
  RGB565( 33,  33,  33),  // panelBg
  RGB565( 66,  66,  66),  // panelBorder
  RGB565(255, 255, 255),  // buttonBg
  RGB565(  0,   0,   0),  // buttonText
  RGB565(255, 255, 255),  // clockFace
  RGB565(255, 255, 255),  // clockSecond
  RGB565(255, 200,   0),  // clockDate (amber for contrast)
  RGB565(255, 255, 255),  // battHigh
  RGB565(198, 198, 198),  // battMid
  RGB565(132, 132, 132),  // battLow
};

// Theme array using _DATA names
const RadarTheme* const THEMES[] PROGMEM = {
  &THEME_GREEN_DATA,
  &THEME_AMBER_DATA,
  &THEME_DEEP_BLUE_DATA,
  &THEME_CRIMSON_DATA,
  &THEME_NOIR_DATA
};

#endif