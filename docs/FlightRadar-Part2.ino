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
  g->setTextSize(1);
  g->setTextColor(th.textPrimary);
  g->setCursor(70, 28);
  g->printf("AC:%d", (int)stats.aircraft_in_range_now);

  // Range label (center)
  char rngBuf[16];
  snprintf(rngBuf, sizeof(rngBuf), "%d %s",
           currentRangeUnits(), unitsLabel());
  int16_t tx = CX - ((int16_t)strlen(rngBuf) * 6) / 2;
  g->setTextColor(th.textSecondary);
  g->setCursor(tx, 28);
  g->print(rngBuf);

  // WiFi indicator (right)
  int rssi = WiFi.RSSI();
  uint16_t wcolor; const char* wlabel;
  if      (apMode)         { wcolor = th.squawk7600; wlabel = "AP";   }
  else if (!wifiConnected) { wcolor = th.squawk7700; wlabel = "--";   }
  else if (rssi > -70)     { wcolor = th.battHigh;   wlabel = "WiFi"; }
  else                     { wcolor = th.battMid;    wlabel = "WiFi"; }
  g->setTextColor(wcolor);
  g->setCursor(LCD_WIDTH - 100, 28);
  g->print(wlabel);
}

// Battery indicator near the bottom of the screen.
// Skipped if BATT_ADC_PIN < 0 (the production schematic should be
// checked to confirm which pin reads the battery divider).
static void drawBatteryIndicator(Arduino_GFX* g) {
  if (BATT_ADC_PIN < 0) return;
  int raw = analogRead(BATT_ADC_PIN);
  float v = (raw / 4095.0f) * 3.3f * BATT_DIVIDER;
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
  g->setTextSize(2);
  g->setTextColor(th.compassRose);
  g->setCursor(CX - 5,                 CY - RADAR_RADIUS - 22); g->print("N");
  g->setCursor(CX - 5,                 CY + RADAR_RADIUS + 6);  g->print("S");
  g->setCursor(CX - RADAR_RADIUS - 18, CY - 7);                 g->print("W");
  g->setCursor(CX + RADAR_RADIUS + 6,  CY - 7);                 g->print("E");

  // ── Range scale labels on each ring ─────────────────────────
  g->setTextSize(1);
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
        g->setTextSize(1);
        g->setTextColor(fadeColor(th.textSecondary, a.intensity));
        g->setCursor(x + 6, y - 6);
        g->print(a.callsign);
      }
    }
    xSemaphoreGive(aircraftMutex);
  }

  drawTopStatusStrip(g);
  drawBatteryIndicator(g);

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
    g->setTextSize(2);
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
  g->setTextSize(3);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 75, y);
  g->print(a.callsign[0] ? a.callsign : "------");

  y += 40;
  g->setTextSize(1);
  g->setTextColor(th.textSecondary);
  g->setCursor(CX - 90, y);
  g->printf("%s  %s", a.type, typeName);

  y += 24;
  g->setTextSize(2);
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

  g->setTextSize(1);
  g->setTextColor(th.textDim);
  g->setCursor(CX - 75, LCD_HEIGHT - 30);
  g->print("Swipe right to go back");

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

  g->setTextSize(2);
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
  g->setTextSize(7);
  g->setTextColor(th.clockFace);
  g->setCursor(CX - 105, CY - 28);
  g->print(timeBuf);

  // Seconds (smaller, right of minutes)
  g->setTextSize(3);
  g->setTextColor(th.clockSecond);
  g->setCursor(CX + 108, CY + 18);
  char secBuf[4]; snprintf(secBuf, sizeof(secBuf), "%02d", tm.tm_sec);
  g->print(secBuf);

  // AM/PM in 12-hour mode
  if (!settings.use_24h) {
    g->setTextSize(2);
    g->setTextColor(th.clockSecond);
    g->setCursor(CX + 108, CY - 28);
    g->print(tm.tm_hour < 12 ? "AM" : "PM");
  }

  // Date
  char dateBuf[24];
  snprintf(dateBuf, sizeof(dateBuf), "%s %d  %d",
           months[tm.tm_mon], tm.tm_mday, tm.tm_year + 1900);
  g->setTextSize(2);
  g->setTextColor(th.clockDate);
  g->setCursor(CX - 80, CY + 60);
  g->print(dateBuf);

  g->setTextSize(1);
  g->setTextColor(th.textDim);
  g->setCursor(CX - 75, LCD_HEIGHT - 30);
  g->print("Swipe right to go back");

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

  g->setTextSize(2);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 50, 70);
  g->print("STATS");

  int y = 110;
  g->setTextSize(1);
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

  g->setTextSize(1);
  g->setCursor(CX - 75, LCD_HEIGHT - 30);
  g->print("Swipe right to go back");

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

  g->setTextSize(2);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 50, 70);
  g->print("SETTINGS");

  const char* labels[9] = {
    "Theme:", "Range:", "Units:", "Time:",
    "Audio:", "Demo:",  "Bright:", "Log:", "DST:" 
  };
  int y = 110;
  g->setTextSize(1);
  for (int i = 0; i < 8; ++i) {
    g->setTextColor(th.textSecondary);
    g->setCursor(CX - 80, y); g->print(labels[i]);
    g->setTextColor(th.textPrimary);
    g->setCursor(CX - 10, y); g->print(settingsValueStr(i));
    y += 18;
  }

  g->setTextColor(th.textDim);
  g->setCursor(CX - 100, LCD_HEIGHT - 30);
  g->print("Tap row to change   Swipe \xc3\xa2\xc2\x86\xc2\x92 exit");
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

  g->setTextSize(3);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 65, 90);
  g->print("SETUP");

  g->setTextSize(1);
  g->setTextColor(th.textSecondary);
  g->setCursor(CX - 100, 150);
  g->print("Connect WiFi to:");

  g->setTextSize(2);
  g->setTextColor(th.textPrimary);
  int16_t sx = CX - ((int16_t)apSsid.length() * 12) / 2;
  g->setCursor(sx, 175);
  g->print(apSsid);

  g->setTextSize(1);
  g->setTextColor(th.textSecondary);
  g->setCursor(CX - 95, 215);
  g->print("Then open a browser at:");

  g->setTextSize(2);
  g->setTextColor(th.textPrimary);
  g->setCursor(CX - 80, 240);
  g->print("192.168.4.1");

  g->setTextSize(1);
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