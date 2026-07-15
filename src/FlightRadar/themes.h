#ifndef THEMES_H
#define THEMES_H

#include <Arduino_GFX.h>

// Radar theme structure - defines colors for the display
struct RadarTheme {
  const char* name;           // Theme name shown in menus
  
  // Background and rings
  uint16_t background;        // Screen background
  uint16_t radarRing;         // Outer radar rings
  uint16_t radarRingDim;      // Dimmed rings
  
  // Sweep line
  uint16_t sweepLine;         // Active sweep line
  uint16_t sweepFade;         // Sweep trail fade
  
  // Crosshair and compass
  uint16_t crosshair;         // Center crosshair
  uint16_t compassRose;         // N/S/E/W markers
  
  // Aircraft blips by altitude
  uint16_t blipLow;           // < 5,000 ft
  uint16_t blipMid;           // 5,000-18,000 ft
  uint16_t blipHigh;          // 18,000-35,000 ft
  uint16_t blipVeryHigh;      // > 35,000 ft
  uint16_t blipSelected;      // Selected aircraft
  uint16_t velocityVector;      // Direction/speed line
  
  // Emergency squawk codes
  uint16_t squawk7700;        // General emergency
  uint16_t squawk7600;        // Radio failure
  uint16_t squawk7500;        // Hijack
  
  // Text colors
  uint16_t textPrimary;       // Main text
  uint16_t textSecondary;     // Secondary text
  uint16_t textDim;           // Dimmed text
  
  // UI elements
  uint16_t panelBg;           // Panel background
  uint16_t panelBorder;       // Panel border
  uint16_t buttonBg;          // Button background
  uint16_t buttonText;        // Button text
  
  // Clock display
  uint16_t clockFace;         // Clock time
  uint16_t clockSecond;       // Clock seconds
  uint16_t clockDate;         // Clock date
  
  // Battery indicator
  uint16_t battHigh;          // High battery
  uint16_t battMid;           // Medium battery
  uint16_t battLow;           // Low battery
  
  // Spare fields for future use
  uint16_t spare[5];
};

// ================================================================
// THEME 1: Green Phosphor (classic radar CRT)
// ================================================================

// Display theme - RGB565 colors for the embedded display
static const RadarTheme THEME_GREEN_DATA PROGMEM = {
  "Green Phosphor",       // name - shown in settings menu
  RGB565(0, 8, 0),        // background - very dark green
  RGB565(0, 100, 0),      // radarRing - dark green
  RGB565(0, 40, 0),       // radarRingDim - dimmer green
  RGB565(0, 60, 0),       // sweepLine - medium green
  RGB565(0, 30, 0),       // sweepFade - sweep trail
  RGB565(80, 255, 80),    // crosshair - bright green
  RGB565(60, 180, 60),    // compassRose - medium green
  RGB565(0, 80, 0),       // blipLow - dark green (< 5,000 ft)
  RGB565(0, 160, 0),      // blipMid - medium green (5,000-18,000 ft)
  RGB565(30, 140, 30),    // blipHigh - lighter green (18,000-35,000 ft)
  RGB565(60, 220, 60),    // blipVeryHigh - bright green (> 35,000 ft)
  RGB565(120, 255, 120),  // blipSelected - selected aircraft
  RGB565(200, 255, 200),  // velocityVector - trail behind aircraft
  RGB565(255, 30, 30),    // squawk7700 - emergency (red)
  RGB565(255, 220, 0),    // squawk7600 - radio failure (yellow)
  RGB565(255, 120, 0),    // squawk7500 - hijack (orange)
  RGB565(200, 255, 200),  // textPrimary - main text
  RGB565(255, 255, 255),  // textSecondary - secondary text (white)
  RGB565(80, 200, 80),    // textDim - dim text
  RGB565(255, 120, 0),    // panelBg - panel background
  RGB565(0, 40, 0),       // panelBorder - panel border
  RGB565(0, 100, 0),      // buttonBg - button background
  RGB565(80, 255, 80),    // buttonText - button text
  RGB565(255, 220, 0),    // clockFace - clock display
  RGB565(80, 255, 80),    // clockSecond - clock seconds
  RGB565(255, 30, 30),    // clockDate - clock date
  RGB565(80, 255, 80),    // battHigh - battery high (green)
  RGB565(60, 180, 60),    // battMid - battery medium
  RGB565(30, 100, 30),    // battLow - battery low
  RGB565(0, 60, 0),       // spare
  RGB565(255, 30, 30),    // spare
  RGB565(255, 220, 0),     // spare
  RGB565(255, 120, 0),     // spare
  RGB565(80, 255, 80)      // spare
};

// Web portal CSS - styles for the configuration web interface
const char THEME_GREEN_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg: #0a0f0a;           /* Background - very dark green */
  --fg: #33ff33;           /* Foreground/text - bright green */
  --accent: #1a331a;       /* Accent color - dark green */
  --input-bg: #0f1a0f;     /* Input background */
  --input-border: #33ff33; /* Input border */
  --button-bg: #33ff33;    /* Button background */
  --button-fg: #0a0f0a;    /* Button text */
  --link: #66ff66;         /* Link color */
}
/* Scanline overlay for CRT effect */
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

// ================================================================
// THEME 2: Amber CRT (vintage terminal look)
// ================================================================

// Display theme - RGB565 colors for the embedded display
static const RadarTheme THEME_AMBER_DATA PROGMEM = {
  "Amber CRT",            // name - shown in settings menu
  RGB565(8, 4, 0),        // background - very dark amber
  RGB565(160, 80, 0),     // radarRing - dark amber
  RGB565(60, 30, 0),      // radarRingDim - dimmer amber
  RGB565(80, 40, 0),      // sweepLine - medium amber
  RGB565(40, 20, 0),      // sweepFade - sweep trail
  RGB565(255, 160, 0),    // crosshair - bright amber
  RGB565(200, 110, 0),    // compassRose - medium amber
  RGB565(80, 40, 0),      // blipLow - dark amber (< 5,000 ft)
  RGB565(200, 100, 0),    // blipMid - medium amber (5,000-18,000 ft)
  RGB565(160, 70, 0),     // blipHigh - lighter amber (18,000-35,000 ft)
  RGB565(220, 120, 0),    // blipVeryHigh - bright amber (> 35,000 ft)
  RGB565(255, 180, 30),   // blipSelected - selected aircraft
  RGB565(255, 220, 120),  // velocityVector - trail behind aircraft
  RGB565(255, 20, 20),    // squawk7700 - emergency (red)
  RGB565(255, 240, 0),    // squawk7600 - radio failure (yellow)
  RGB565(255, 60, 0),     // squawk7500 - hijack (orange-red)
  RGB565(255, 220, 120),  // textPrimary - main text
  RGB565(255, 255, 255),  // textSecondary - secondary text (white)
  RGB565(220, 140, 20),   // textDim - dim text
  RGB565(255, 60, 0),     // panelBg - panel background
  RGB565(60, 30, 0),      // panelBorder - panel border
  RGB565(160, 80, 0),     // buttonBg - button background
  RGB565(255, 160, 0),    // buttonText - button text
  RGB565(255, 240, 0),    // clockFace - clock display
  RGB565(255, 160, 0),    // clockSecond - clock seconds
  RGB565(255, 20, 20),    // clockDate - clock date
  RGB565(255, 160, 0),    // battHigh - battery high (amber)
  RGB565(200, 110, 0),    // battMid - battery medium
  RGB565(100, 50, 0),     // battLow - battery low
  RGB565(60, 30, 0),      // spare
  RGB565(255, 20, 20),     // spare
  RGB565(255, 240, 0),     // spare
  RGB565(255, 60, 0),      // spare
  RGB565(255, 160, 0)      // spare
};

// Web portal CSS - styles for the configuration web interface
const char THEME_AMBER_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg: #1a0f00;           /* Background - very dark amber */
  --fg: #ffb000;           /* Foreground/text - bright amber */
  --accent: #331a00;       /* Accent color - dark amber */
  --input-bg: #1a0f00;     /* Input background */
  --input-border: #ffb000; /* Input border */
  --button-bg: #ffb000;    /* Button background */
  --button-fg: #1a0f00;    /* Button text */
  --link: #ffcc33;         /* Link color */
}
/* Scanline overlay for CRT effect */
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

// ================================================================
// THEME 3: Deep Blue (cool cyan phosphor)
// ================================================================

// Display theme - RGB565 colors for the embedded display
static const RadarTheme THEME_DEEP_BLUE_DATA PROGMEM = {
  "Deep Blue",            // name - shown in settings menu
  RGB565(0, 0, 8),        // background - very dark blue
  RGB565(0, 64, 128),     // radarRing - brighter cyan-blue (was RGB565(0, 0, 32))
  RGB565(0, 32, 64),      // radarRingDim - medium blue (was RGB565(0, 0, 16))
  RGB565(0, 64, 64),      // sweepLine - cyan-blue
  RGB565(0, 32, 32),      // sweepFade - sweep trail
  RGB565(0, 64, 64),      // crosshair - cyan-blue
  RGB565(0, 64, 32),      // compassRose - medium blue
  RGB565(0, 0, 40),       // blipLow - dark blue (< 5,000 ft)
  RGB565(0, 128, 255),    // blipMid - medium blue (5,000-18,000 ft)
  RGB565(0, 200, 255),    // blipHigh - light blue (18,000-35,000 ft)
  RGB565(0, 255, 240),    // blipVeryHigh - cyan-white (> 35,000 ft)
  RGB565(0, 255, 255),    // blipSelected - selected aircraft
  RGB565(0, 255, 255),    // velocityVector - trail behind aircraft
  RGB565(255, 100, 0),    // squawk7700 - emergency (orange)
  RGB565(255, 255, 0),    // squawk7600 - radio failure (yellow)
  RGB565(255, 100, 0),    // squawk7500 - hijack (orange)
  RGB565(0, 255, 255),    // textPrimary - main text (bright cyan)
  RGB565(0, 200, 255),    // textSecondary - secondary text (light blue)
  RGB565(0, 64, 128),     // textDim - dim text
  RGB565(0, 64, 32),      // panelBg - panel background
  RGB565(0, 0, 16),       // panelBorder - panel border
  RGB565(0, 0, 32),       // buttonBg - button background
  RGB565(0, 255, 255),    // buttonText - button text (bright cyan)
  RGB565(0, 255, 255),    // clockFace - clock display
  RGB565(0, 128, 255),    // clockSecond - clock seconds
  RGB565(255, 100, 0),    // clockDate - clock date
  RGB565(0, 255, 240),    // battHigh - battery high (cyan)
  RGB565(0, 200, 255),    // battMid - battery medium
  RGB565(0, 64, 128),     // battLow - battery low
  RGB565(0, 200, 255),    // spare
  RGB565(0, 128, 255),     // spare
  RGB565(0, 255, 255),     // spare
  RGB565(255, 255, 0),     // spare
  RGB565(255, 100, 0)      // spare
};

// Web portal CSS - styles for the configuration web interface
const char THEME_BLUE_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg: #000510;           /* Background - very dark blue */
  --fg: #00ffff;           /* Foreground/text - bright cyan */
  --accent: #001a33;       /* Accent color - dark blue */
  --input-bg: #000a1a;     /* Input background */
  --input-border: #00ffff; /* Input border */
  --button-bg: #00ffff;    /* Button background */
  --button-fg: #000510;    /* Button text */
  --link: #66ffff;         /* Link color */
}
/* Scanline overlay for CRT effect */
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

// ================================================================
// THEME 4: Crimson (intense red phosphor)
// ================================================================

// Display theme - RGB565 colors for the embedded display
static const RadarTheme THEME_CRIMSON_DATA PROGMEM = {
  "Crimson",              // name - shown in settings menu
  RGB565(32, 0, 0),       // background - very dark red
  RGB565(128, 0, 0),      // radarRing - brighter red (was RGB565(64, 0, 0))
  RGB565(64, 0, 0),       // radarRingDim - medium red (was RGB565(48, 0, 0))
  RGB565(88, 0, 0),       // sweepLine - medium red
  RGB565(44, 0, 0),       // sweepFade - sweep trail
  RGB565(88, 0, 0),       // crosshair - medium red
  RGB565(88, 0, 0),       // compassRose - medium red
  RGB565(72, 0, 0),       // blipLow - dark red (< 5,000 ft)
  RGB565(120, 0, 0),      // blipMid - medium red (5,000-18,000 ft)
  RGB565(176, 0, 0),      // blipHigh - lighter red (18,000-35,000 ft)
  RGB565(255, 0, 0),      // blipVeryHigh - bright red (> 35,000 ft)
  RGB565(255, 0, 0),      // blipSelected - selected aircraft
  RGB565(255, 0, 0),      // velocityVector - trail behind aircraft
  RGB565(255, 255, 0),    // squawk7700 - emergency (yellow - stands out)
  RGB565(255, 200, 0),    // squawk7600 - radio failure (yellow-orange)
  RGB565(255, 100, 0),    // squawk7500 - hijack (orange)
  RGB565(255, 0, 0),      // textPrimary - main text (bright red)
  RGB565(255, 200, 200),  // textSecondary - secondary text (pinkish white)
  RGB565(88, 0, 0),       // textDim - dim text
  RGB565(88, 0, 0),       // panelBg - panel background
  RGB565(48, 0, 0),       // panelBorder - panel border
  RGB565(64, 0, 0),       // buttonBg - button background
  RGB565(255, 0, 0),      // buttonText - button text
  RGB565(255, 0, 0),      // clockFace - clock display
  RGB565(255, 0, 0),      // clockSecond - clock seconds
  RGB565(255, 255, 0),    // clockDate - clock date (yellow for contrast)
  RGB565(255, 0, 0),      // battHigh - battery high (red)
  RGB565(176, 0, 0),      // battMid - battery medium
  RGB565(120, 0, 0),      // battLow - battery low
  RGB565(176, 0, 0),       // spare
  RGB565(120, 0, 0),       // spare
  RGB565(88, 0, 0),        // spare
  RGB565(255, 255, 0),     // spare
  RGB565(255, 0, 0)        // spare
};

// Web portal CSS - styles for the configuration web interface
const char THEME_RED_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg: #1a0500;           /* Background - very dark red */
  --fg: #ff3333;           /* Foreground/text - bright red */
  --accent: #330a00;       /* Accent color - dark red */
  --input-bg: #1a0500;     /* Input background */
  --input-border: #ff3333; /* Input border */
  --button-bg: #ff3333;    /* Button background */
  --button-fg: #1a0500;    /* Button text */
  --link: #ff6666;         /* Link color */
}
/* Scanline overlay for CRT effect */
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

// ================================================================
// THEME 5: Noir (classic black & white)
// ================================================================

// Display theme - RGB565 colors for the embedded display
static const RadarTheme THEME_NOIR_DATA PROGMEM = {
  "Noir",                 // name - shown in settings menu
  RGB565(0, 0, 0),        // background - black
  RGB565(66, 66, 66),     // radarRing - dark gray
  RGB565(33, 33, 33),     // radarRingDim - dimmer gray
  RGB565(99, 99, 99),     // sweepLine - medium gray
  RGB565(50, 50, 50),     // sweepFade - sweep trail
  RGB565(99, 99, 99),     // crosshair - medium gray
  RGB565(99, 99, 99),     // compassRose - medium gray
  RGB565(66, 66, 66),     // blipLow - dark gray (< 5,000 ft)
  RGB565(132, 132, 132),  // blipMid - medium gray (5,000-18,000 ft)
  RGB565(198, 198, 198),  // blipHigh - light gray (18,000-35,000 ft)
  RGB565(255, 255, 255),  // blipVeryHigh - white (> 35,000 ft)
  RGB565(255, 255, 255),  // blipSelected - selected aircraft
  RGB565(255, 255, 255),  // velocityVector - trail behind aircraft
  RGB565(255, 0, 0),      // squawk7700 - emergency (red - stands out)
  RGB565(255, 255, 0),    // squawk7600 - radio failure (yellow)
  RGB565(255, 100, 0),    // squawk7500 - hijack (orange)
  RGB565(255, 255, 255),  // textPrimary - main text (white)
  RGB565(220, 220, 220),  // textSecondary - secondary text (light gray)
  RGB565(99, 99, 99),     // textDim - dim text
  RGB565(99, 99, 99),     // panelBg - panel background
  RGB565(66, 66, 66),     // panelBorder - panel border
  RGB565(33, 33, 33),     // buttonBg - button background
  RGB565(255, 255, 255),  // buttonText - button text
  RGB565(255, 255, 255),  // clockFace - clock display
  RGB565(255, 255, 255),  // clockSecond - clock seconds
  RGB565(255, 200, 0),    // clockDate - clock date (amber for contrast)
  RGB565(255, 255, 255),  // battHigh - battery high (white)
  RGB565(198, 198, 198),  // battMid - battery medium
  RGB565(132, 132, 132),  // battLow - battery low
  RGB565(198, 198, 198),   // spare
  RGB565(132, 132, 132),   // spare
  RGB565(99, 99, 99),      // spare
  RGB565(255, 255, 0),     // spare
  RGB565(255, 0, 0)        // spare
};

// Web portal CSS - styles for the configuration web interface
const char THEME_GRAY_CSS[] PROGMEM = R"rawliteral(
:root {
  --bg: #0a0a0a;           /* Background - black */
  --fg: #ffffff;           /* Foreground/text - white */
  --accent: #1a1a1a;       /* Accent color - dark gray */
  --input-bg: #0f0f0f;     /* Input background */
  --input-border: #ffffff; /* Input border */
  --button-bg: #ffffff;    /* Button background */
  --button-fg: #0a0a0a;    /* Button text */
  --link: #cccccc;         /* Link color */
}
/* Scanline overlay for CRT effect */
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

// ================================================================
// Theme Arrays and Index Enum
// ================================================================

// Display theme array - used by the embedded device
const RadarTheme* const THEMES[] PROGMEM = {
  &THEME_GREEN_DATA,
  &THEME_AMBER_DATA,
  &THEME_DEEP_BLUE_DATA,
  &THEME_CRIMSON_DATA,
  &THEME_NOIR_DATA
};

// Web portal CSS theme array - used by the web interface
const char* const THEME_CSS[] PROGMEM = {
  THEME_GREEN_CSS,
  THEME_AMBER_CSS,
  THEME_BLUE_CSS,
  THEME_RED_CSS,
  THEME_GRAY_CSS
};

// Theme index enum - used to reference themes by name
enum ThemeIndex {
  THEME_GREEN = 0,
  THEME_AMBER,
  THEME_DEEP_BLUE,
  THEME_CRIMSON,
  THEME_NOIR,
  THEME_COUNT
};

#endif