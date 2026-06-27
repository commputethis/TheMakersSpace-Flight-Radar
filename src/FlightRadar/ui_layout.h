// ================================================================
// ui_layout.h
// ----------------------------------------------------------------
// Screen layout constants and gesture thresholds for FlightRadar.
// Keep layout math in one place so rendering and hit-testing
// stay in sync.
// ================================================================

#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <Arduino.h>

// ----------------------------------------------------------------
// Screen Geometry
// ----------------------------------------------------------------
static const int16_t SCREEN_CX = LCD_WIDTH / 2;
static const int16_t SCREEN_CY = LCD_HEIGHT / 2;

// ----------------------------------------------------------------
// Radar Screen
// ----------------------------------------------------------------
static const int16_t RADAR_RADIUS = 180;
static const int16_t RADAR_RING_COUNT = 4;

// ----------------------------------------------------------------
// Settings Overlay
// ----------------------------------------------------------------
// MUST stay in sync with settingsRowHit() in Part 3
static const int16_t SETTINGS_START_Y = 100;
static const int16_t SETTINGS_ROW_HEIGHT = 30;
static const int16_t SETTINGS_LABEL_X = -120;        // Relative to SCREEN_CX
static const int16_t SETTINGS_VALUE_X = -20;         // Relative to SCREEN_CX
static const int16_t SETTINGS_HIT_MARGIN = 15;       // Vertical padding for touches

// ----------------------------------------------------------------
// Status Indicators
// ----------------------------------------------------------------
static const int16_t WIFI_ICON_X = LCD_WIDTH - 125;
static const int16_t WIFI_ICON_Y = 28;
static const int16_t BATTERY_Y = LCD_HEIGHT - 40;
static const int16_t BATTERY_X = LCD_WIDTH - 125;
static const int16_t BATTERY_X_OFFSET = 18;

// ----------------------------------------------------------------
// Edge Brightness Control
// ----------------------------------------------------------------
static const int16_t EDGE_BRIGHTNESS_MARGIN = 30;    // Pixels from screen edge

// ----------------------------------------------------------------
// Touch/Gesture Thresholds (shared with Part 3)
// ----------------------------------------------------------------
static const int16_t AIRCRAFT_TAP_RADIUS_SQ = 25 * 25;    // Aircraft blip hit area
static const int16_t LONG_PRESS_DEADZONE = 15;           // Max movement for long press
static const uint32_t LONG_PRESS_DURATION_MS = 500;
static const int16_t DRAG_THRESHOLD = 25;                // Promote to dragging
static const int16_t SWIPE_THRESHOLD = 60;               // Minimum swipe distance

#endif