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