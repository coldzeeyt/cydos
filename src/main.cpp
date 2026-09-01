#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <SD.h>

#include "Config.h"
#include "core/UI.h"
#include "core/Touch.h"
#include "core/Battery.h"
#include "core/Display.h"
#include "core/AppManager.h"
#include "core/UpdateChecker.h"
#include "core/WallClock.h"
#include "core/SdCard.h"
#include "core/SdAppPool.h"
#include "core/Wallpaper.h"

#include "apps/HomeApp.h"
#include "apps/WifiRadarApp.h"
#include "apps/FlashlightApp.h"
#include "apps/ClockApp.h"
#include "apps/QrBeamerApp.h"
#include "apps/DiceApp.h"
#include "apps/CalculatorApp.h"
#include "apps/PasswordGenApp.h"
#include "apps/MorseBeaconApp.h"
#include "apps/BrowserApp.h"
#include "apps/ObsApp.h"
#include "apps/SpotifyApp.h"
#include "apps/SettingsApp.h"
#include "apps/FileManagerApp.h"
#include "apps/DiagnosticsApp.h"
#include "apps/CommunityStoreApp.h"
#include "apps/NotesApp.h"
#include "apps/UnitConverterApp.h"
#include "apps/MediaPlayerApp.h"

TFT_eSPI tft;
Touch touch;
Battery battery;
AppManager appManager;
Preferences prefs;
UpdateChecker updateChecker;
WallClock wallClock;
SdCard sdCard;
SdAppPool sdAppPool;

HomeApp homeApp(&appManager);
WifiRadarApp wifiApp;
FlashlightApp flashApp(&appManager);
ClockApp clockApp(&wallClock);
QrBeamerApp qrApp;
DiceApp diceApp;
CalculatorApp calcApp;
PasswordGenApp passwordApp;
MorseBeaconApp morseApp;
BrowserApp browserApp(&prefs);
ObsApp obsApp(&prefs);
SpotifyApp spotifyApp(&prefs);
SettingsApp settingsApp(&appManager, &touch, &prefs, &updateChecker, &wallClock, &sdCard);
FileManagerApp fileManagerApp(&sdCard);
DiagnosticsApp diagApp(&battery, &sdCard, &touch);
CommunityStoreApp communityStoreApp(&appManager, &prefs, &sdCard, &sdAppPool);
NotesApp notesApp(&prefs);
UnitConverterApp convertApp;
MediaPlayerApp mediaPlayerApp(&prefs);

uint8_t g_lastSavedBrightness = 80;
uint32_t g_lastBrightnessCheck = 0;

// Reads and shows nothing but raw touch ADC values, full-screen, forever -
// no calibration, no navigation, no target you need to hit. For a touch
// panel miscalibrated badly enough that even Home's biggest tile isn't
// reliably tappable (see Settings > Touch Test, which needs working touch
// to reach in the first place - this doesn't).
//
// Entered by holding the board's physical BOOT button (GPIO0, active-low)
// for about a second - but only *after* CydOs has already booted normally
// (see the debounced check in loop()), never during power-on/reset itself:
// the ESP32's own boot ROM samples GPIO0 at that exact moment to decide
// whether to enter USB flashing mode, before any of this firmware's code
// runs at all, so holding BOOT while plugging in or resetting the board
// skips straight past this entirely and puts the chip in download mode
// instead. tft/touch are already initialized by normal setup() by the
// time this can run, so it doesn't redo that. Never returns; power-cycle
// (without touching BOOT) to boot normally again.
// Two guesses at the SD slot's real wiring (touch-shared, then a separate
// dedicated-pins guess) have both failed to detect a real, known-good
// card on real hardware - rather than guess a third pinout blind, try
// several documented CYD SD wiring variants in turn and report which one
// (if any) actually works, straight from the device.
struct SdPinCandidate { const char* label; int8_t clk, miso, mosi; };
static const SdPinCandidate SD_PIN_CANDIDATES[] = {
  {"touch-shared 25/39/32", 25, 39, 32},
  {"18/19/23", 18, 19, 23},
  {"display-shared 14/12/13", 14, 12, 13},
};

// Tries each candidate in turn (CS fixed at Cfg::SD_CS - every wiring
// reference agrees on that pin even where they disagree on the rest);
// returns the label of the first one SD.begin() succeeds on, or nullptr
// if none did. Fills outSizeMb with the card size when one works.
const char* scanSdPins(uint32_t* outSizeMb) {
  for (const SdPinCandidate& cand : SD_PIN_CANDIDATES) {
    SD.end();
    SPI.begin(cand.clk, cand.miso, cand.mosi, Cfg::SD_CS);
    if (SD.begin(Cfg::SD_CS, SPI, 4000000)) {
      if (outSizeMb) *outSizeMb = SD.cardSize() / (1024UL * 1024UL);
      return cand.label;
    }
  }
  return nullptr;
}

void runTouchDiagMode() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Touch Diagnostic Mode", 10, 8, 2);
  tft.drawString("(entered by holding BOOT for 1s after boot)", 10, 30, 1);
  tft.drawString("Press each corner, note the raw numbers below.", 10, 44, 1);
  tft.drawString("Power-cycle without touching BOOT to exit.", 10, 58, 1);

  uint32_t sdSizeMb = 0;
  const char* sdResult = scanSdPins(&sdSizeMb);
  tft.setTextColor(sdResult ? TFT_GREEN : TFT_RED, TFT_BLACK);
  char sdLine[64];
  if (sdResult) snprintf(sdLine, sizeof(sdLine), "SD: FOUND on %s (%lu MB)", sdResult, (unsigned long)sdSizeMb);
  else snprintf(sdLine, sizeof(sdLine), "SD: not found on any known pinout");
  tft.drawString(sdLine, 10, 68, 1);
  touch.begin(); // scanSdPins() left the shared bus pointed at whichever pins it tried last - hand it back

  int16_t minX = 32767, maxX = -32768, minY = 32767, maxY = -32768;
  int16_t lastRx = -1, lastRy = -1;

  while (true) {
    int16_t rx, ry;
    if (touch.readRaw(rx, ry) && (rx != lastRx || ry != lastRy)) {
      lastRx = rx;
      lastRy = ry;
      if (rx < minX) minX = rx;
      if (rx > maxX) maxX = rx;
      if (ry < minY) minY = ry;
      if (ry > maxY) maxY = ry;

      tft.fillRect(0, 80, Cfg::SCREEN_W, Cfg::SCREEN_H - 80, TFT_BLACK);
      char buf[48];
      tft.setTextColor(TFT_CYAN, TFT_BLACK);
      snprintf(buf, sizeof(buf), "raw X: %d", rx);
      tft.drawString(buf, 10, 90, 4);
      snprintf(buf, sizeof(buf), "raw Y: %d", ry);
      tft.drawString(buf, 10, 130, 4);

      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      snprintf(buf, sizeof(buf), "X seen so far: %d to %d", minX, maxX);
      tft.drawString(buf, 10, 180, 1);
      snprintf(buf, sizeof(buf), "Y seen so far: %d to %d", minY, maxY);
      tft.drawString(buf, 10, 196, 1);
    }
    delay(30);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(0, INPUT_PULLUP); // BOOT button - polled (debounced) from loop(), see runTouchDiagMode()

  prefs.begin("cydos", false);
  uint8_t savedBrightness = prefs.getUChar("bright", 80);
  g_lastSavedBrightness = savedBrightness;

  // Settings > Wallpapers picks a file under /cydos_wallpapers/ (or the
  // legacy default) and persists the choice here; Home reads it back via
  // Wallpaper::draw() every time it's shown.
  String savedWallpaper = prefs.getString("wallpaperPath", "/cydos_wallpaper.bmp");
  Wallpaper::setActivePath(savedWallpaper.c_str());
  bool devMode = prefs.getBool("devmode", false);

  tft.init();
  tft.setRotation(Cfg::SCREEN_ROTATION);
  tft.fillScreen(Theme::BG);

  Display::beginBacklight();
  Display::setBrightnessPercent(savedBrightness);

  touch.begin();
  battery.begin();
  randomSeed(esp_random());

  updateChecker.begin(&prefs);
  appManager.begin(tft, &battery, &updateChecker);
  appManager.setBrightnessPercent(savedBrightness);
  appManager.setBatteryVisible(prefs.getBool("battshow", true));
  appManager.setLockScreenEnabled(prefs.getBool("lockscreen", false));

  uint8_t homeIdx = appManager.registerApp(&homeApp);
  uint8_t wifiIdx = appManager.registerApp(&wifiApp);
  uint8_t flashIdx = appManager.registerApp(&flashApp);
  uint8_t clockIdx = appManager.registerApp(&clockApp);
  uint8_t qrIdx = appManager.registerApp(&qrApp);
  uint8_t diceIdx = appManager.registerApp(&diceApp);
  uint8_t calcIdx = appManager.registerApp(&calcApp);
  uint8_t passwordIdx = appManager.registerApp(&passwordApp);
  uint8_t morseIdx = appManager.registerApp(&morseApp);
  uint8_t browserIdx = appManager.registerApp(&browserApp);
  uint8_t obsIdx = appManager.registerApp(&obsApp);
  uint8_t spotifyIdx = appManager.registerApp(&spotifyApp);
  uint8_t settingsIdx = appManager.registerApp(&settingsApp);
  uint8_t filesIdx = appManager.registerApp(&fileManagerApp);
  uint8_t diagIdx = appManager.registerApp(&diagApp);
  uint8_t storeIdx = appManager.registerApp(&communityStoreApp);
  uint8_t notesIdx = appManager.registerApp(&notesApp);
  uint8_t convertIdx = appManager.registerApp(&convertApp);
  uint8_t mediaIdx = appManager.registerApp(&mediaPlayerApp);

  // Settings goes first, not just conveniently early - it's the only way
  // to reach Touch Test, which is itself the fix for a touch panel bad
  // enough that swiping to a later page or hitting a small target isn't
  // reliable yet. A tile that size, in the corner every grid starts from,
  // is the easiest one to land on by accident even with miscalibrated touch.
  homeApp.addTile(UI::iconGear, settingsIdx);
  homeApp.addTile(UI::iconWifi, wifiIdx);
  homeApp.addTile(UI::iconFlash, flashIdx);
  homeApp.addTile(UI::iconClock, clockIdx);
  homeApp.addTile(UI::iconQR, qrIdx);
  homeApp.addTile(UI::iconDice, diceIdx);
  homeApp.addTile(UI::iconCalc, calcIdx);
  homeApp.addTile(UI::iconKey, passwordIdx);
  homeApp.addTile(UI::iconMorse, morseIdx);
  homeApp.addTile(UI::iconGlobe, browserIdx);
  homeApp.addTile(UI::iconBroadcast, obsIdx);
  homeApp.addTile(UI::iconMusic, spotifyIdx);
  homeApp.addTile(UI::iconFolder, filesIdx);
  homeApp.addTile(UI::iconStore, storeIdx);
  homeApp.addTile(UI::iconNotes, notesIdx);
  homeApp.addTile(UI::iconConvert, convertIdx);
  homeApp.addTile(UI::iconMediaPlayer, mediaIdx);
  // Diagnostics only gets a Home tile with Settings > Dev Mode on - a
  // full-screen solid-red test pattern isn't something a regular user
  // should be able to stumble into by accident.
  if (devMode) homeApp.addTile(UI::iconDiag, diagIdx);

  // Community apps (see community-apps/ and scripts/generate_community.py):
  // this file is a no-op stub in a normal checkout, so `pio run -e cyd`
  // builds the standard firmware untouched (communityStoreApp just shows
  // its empty state). The Community Edition build (CI, or
  // `python3 scripts/generate_community.py` run locally first) overwrites
  // it with one registerApp()+communityStoreApp.addTile() block per
  // submitted app - they all land inside the App Store tile above, not as
  // separate top-level Home tiles.
#include "community_registration.inc"

  // SD Card Apps - scanned once here at boot, and again any time the App
  // Store's Get More tab downloads a new one over WiFi (SdAppPool::rescan()
  // is safe to call repeatedly - see src/core/SdAppPool.h). touch.begin()
  // above already configured the shared SPI bus (see Cfg::SD_CS); if
  // there's no card, or no /cydos_apps folder, this is a no-op.
  sdAppPool.begin(&appManager, &homeApp);
  if (sdCard.begin()) sdAppPool.rescan();

  if (appManager.lockScreenEnabled()) {
    appManager.beginLocked(&wallClock);
  } else {
    appManager.openApp(homeIdx);
  }
}

void loop() {
  // Held for ~1s (debounced against a brief/accidental press), this drops
  // into runTouchDiagMode() and never returns to the rest of loop() - see
  // its comment for why the check lives here and not in setup().
  static uint32_t bootHeldSince = 0;
  if (digitalRead(0) == LOW) {
    if (bootHeldSince == 0) bootHeldSince = millis();
    else if (millis() - bootHeldSince > 1000) runTouchDiagMode();
  } else {
    bootHeldSince = 0;
  }

  static int16_t lastX = 0, lastY = 0;
  static bool wasDown = false;

  int16_t x, y;
  bool touched = touch.read(x, y);
  bool pressStart = touched && !wasDown;

  if (touched) { lastX = x; lastY = y; }
  appManager.loop(lastX, lastY, pressStart, touched);
  wasDown = touched;

  // Don't let a periodic update check steal the WiFi radio out from under
  // an active scan in WiFi Radar, or collide with Browser's, OBS's, or
  // Spotify's own WiFi/HTTP use. Also skip it in Morse: performCheck()
  // blocks the whole loop for several seconds (WiFi connect + HTTPS GET),
  // which would freeze appManager.loop() mid-flash and scramble the
  // on/off timing a Morse signal depends on.
  if (appManager.currentApp() != &wifiApp && appManager.currentApp() != &browserApp &&
      appManager.currentApp() != &obsApp && appManager.currentApp() != &spotifyApp &&
      appManager.currentApp() != &morseApp && appManager.currentApp() != &communityStoreApp &&
      appManager.currentApp() != &mediaPlayerApp) {
    updateChecker.update();
  }

  // Persist brightness to flash, but not on every tiny slider move.
  uint32_t now = millis();
  if (now - g_lastBrightnessCheck > 3000) {
    g_lastBrightnessCheck = now;
    uint8_t current = appManager.brightnessPercent();
    if (current != g_lastSavedBrightness) {
      prefs.putUChar("bright", current);
      g_lastSavedBrightness = current;
    }
  }
}
