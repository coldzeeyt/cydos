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
#include "apps/SdCardApp.h"

TFT_eSPI tft;
Touch touch;
Battery battery;
AppManager appManager;
Preferences prefs;
UpdateChecker updateChecker;
WallClock wallClock;

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
SettingsApp settingsApp(&appManager, &touch, &prefs, &updateChecker, &wallClock);

// SD Card Apps: simple no-code screens (see src/apps/SdCardApp.h) loaded
// from /cydos_apps/*.cydapp at boot. A fixed pool, same reasoning as the
// community app registration - only slots that actually loaded a file get
// a Home tile. Editing the SD card takes effect on the next power-on.
SdCard sdCard;
static constexpr uint8_t MAX_SD_APPS = 6;
SdCardApp sdApps[MAX_SD_APPS];

uint8_t g_lastSavedBrightness = 80;
uint32_t g_lastBrightnessCheck = 0;

void setup() {
  Serial.begin(115200);

  prefs.begin("cydos", false);
  uint8_t savedBrightness = prefs.getUChar("bright", 80);
  g_lastSavedBrightness = savedBrightness;

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

  // Community apps (see community-apps/ and scripts/generate_community.py):
  // this file is a no-op stub in a normal checkout, so `pio run -e cyd`
  // builds the standard firmware untouched. The Community Edition build
  // (CI, or `python3 scripts/generate_community.py` run locally first)
  // overwrites it with one registerApp()+addTile() block per submitted app.
#include "community_registration.inc"

  // SD Card Apps - scanned once here at boot. touch.begin() above already
  // configured the shared SPI bus (see Cfg::SD_CS); if there's no card, or
  // no /cydos_apps folder, this is a no-op and every slot stays unused.
  if (sdCard.begin()) {
    File dir = SD.open("/cydos_apps");
    if (dir && dir.isDirectory()) {
      uint8_t slot = 0;
      File entry = dir.openNextFile();
      while (entry && slot < MAX_SD_APPS) {
        if (!entry.isDirectory()) {
          String fname = entry.name();
          String lower = fname; lower.toLowerCase();
          if (lower.endsWith(".cydapp")) {
            String path = fname.startsWith("/") ? fname : String("/cydos_apps/") + fname;
            entry.close();
            if (sdApps[slot].load(path.c_str())) {
              uint8_t sdIdx = appManager.registerApp(&sdApps[slot]);
              homeApp.addTile(UI::iconPuzzle, sdIdx);
              slot++;
            }
            entry = dir.openNextFile();
            continue;
          }
        }
        entry.close();
        entry = dir.openNextFile();
      }
    }
  }

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
      appManager.currentApp() != &morseApp) {
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
