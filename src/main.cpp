#include <Arduino.h>
#include <TFT_eSPI.h>
#include <Preferences.h>

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

uint8_t g_lastSavedBrightness = 80;
uint32_t g_lastBrightnessCheck = 0;

void setup() {
  Serial.begin(115200);

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
  homeApp.addTile(UI::iconGear, settingsIdx);
  homeApp.addTile(UI::iconFolder, filesIdx);
  homeApp.addTile(UI::iconStore, storeIdx);
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
      appManager.currentApp() != &morseApp && appManager.currentApp() != &communityStoreApp) {
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
