#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"
#include "core/Touch.h"
#include "core/UpdateChecker.h"
#include "core/WallClock.h"

class AppManager;
class SdCard;

// Global brightness (persisted across apps), WiFi setup for the update
// checker, manual time setting, a touch test screen for dialing in
// Config.h's calibration constants, a Wallpapers picker for switching
// between preinstalled SD-card wallpapers, and the Dev Mode toggle that
// gates whether the Diagnostics app gets a Home tile.
class SettingsApp : public App {
public:
  SettingsApp(AppManager* mgr, Touch* touch, Preferences* prefs, UpdateChecker* updates, WallClock* clock, SdCard* sd)
      : _mgr(mgr), _touch(touch), _prefs(prefs), _updates(updates), _clock(clock), _sd(sd) {}

  const char* name() const override { return "Settings"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;
  void onTouchUp() override { _draggingSlider = false; }

private:
  enum Mode { MAIN, TOUCH_TEST, WIFI, WIFI_KEYBOARD, SET_TIME, WALLPAPERS, WALLPAPER_PREVIEW };
  Mode _mode = MAIN;
  enum Field { FIELD_SSID, FIELD_PASS };
  Field _activeField = FIELD_SSID;

  AppManager* _mgr;
  Touch* _touch;
  Preferences* _prefs;
  UpdateChecker* _updates;
  WallClock* _clock;
  SdCard* _sd;
  bool _devModeEnabled = false;

  UI::Slider _brightSlider{{40, 58, Cfg::SCREEN_W - 80, 26}, 80};
  UI::Button _touchTestBtn{{40, 106, 116, 28}, "Touch Test"};
  UI::Button _setTimeBtn{{164, 106, 116, 28}, "Set Time"};
  UI::Button _wifiBtn{{40, 140, Cfg::SCREEN_W - 80, 28}, "WiFi Setup"};
  UI::Button _battToggleBtn{{40, 174, 116, 24}, "Batt: ON"};
  UI::Button _lockToggleBtn{{164, 174, 116, 24}, "Lock: OFF"};
  UI::Button _wallpapersBtn{{40, 204, 116, 22}, "Wallpapers"};
  UI::Button _devModeBtn{{164, 204, 116, 22}, "Dev: OFF"};
  UI::Button _backBtn{{10, Cfg::STATUS_BAR_H + 6, 90, 30}, "<- Back"};
  UI::Button _wpUseBtn{{50, 190, 100, 34}, "Use"};
  UI::Button _wpCancelBtn{{170, 190, 100, 34}, "Cancel"};
  UI::Button _timeHourUp{{40, Cfg::STATUS_BAR_H + 90, 40, 34}, "H+"};
  UI::Button _timeHourDn{{90, Cfg::STATUS_BAR_H + 90, 40, 34}, "H-"};
  UI::Button _timeMinUp{{190, Cfg::STATUS_BAR_H + 90, 40, 34}, "M+"};
  UI::Button _timeMinDn{{240, Cfg::STATUS_BAR_H + 90, 40, 34}, "M-"};
  UI::Button _wifiSaveBtn{{30, 158, 130, 34}, "Save"};
  UI::Button _wifiTestBtn{{170, 158, 120, 34}, "Test Now"};
  UI::Button _kbSpaceBtn{{4, 0, 100, 26}, "SPACE"};
  UI::Button _kbDelBtn{{108, 0, 90, 26}, "DEL"};
  UI::Button _kbClrBtn{{202, 0, 54, 26}, "CLR"};
  UI::Button _kbShiftBtn{{258, 0, 58, 26}, "CAPS"};
  UI::Button _kbDoneBtn{{4, 0, Cfg::SCREEN_W - 8, 28}, "Done"};
  bool _kbShift = false; // WiFi SSID/password keyboard only - most networks need uppercase

  bool _draggingSlider = false;
  bool _dirty = true;
  int16_t _lastTouchX = -1, _lastTouchY = -1;
  int16_t _lastRawX = -1, _lastRawY = -1;

  static constexpr size_t SSID_MAX = 32;
  static constexpr size_t PASS_MAX = 64;
  char _ssid[SSID_MAX + 1] = {0};
  char _pass[PASS_MAX + 1] = {0};

  static constexpr uint8_t KB_ROWS = 4;
  const char* _kbRows[KB_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};
  const char* _kbSymbols = "-_.@!";

  void drawMain(TFT_eSPI& tft);
  void drawTouchTest(TFT_eSPI& tft);
  void drawWifi(TFT_eSPI& tft);
  void drawWifiKeyboard(TFT_eSPI& tft);
  void drawSetTime(TFT_eSPI& tft);
  void drawWallpapers(TFT_eSPI& tft);
  void drawWallpaperPreview(TFT_eSPI& tft);

  char* activeBuf() { return _activeField == FIELD_SSID ? _ssid : _pass; }
  size_t activeMax() { return _activeField == FIELD_SSID ? SSID_MAX : PASS_MAX; }
  UI::Rect kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  UI::Rect wifiSsidBox() const { return {30, Cfg::STATUS_BAR_H + 40, Cfg::SCREEN_W - 60, 34}; }
  UI::Rect wifiPassBox() const { return {30, Cfg::STATUS_BAR_H + 84, Cfg::SCREEN_W - 60, 34}; }
  void loadWifiCreds();

  // Preinstalled wallpapers: entry 0 is always the single default file
  // every existing doc/tool already tells people to copy to the SD card
  // root; entries 1+ come from scanning /cydos_wallpapers/*.bmp.
  static constexpr uint8_t MAX_WALLPAPERS = 9;
  static constexpr uint8_t WP_NAME_LEN = 28;
  static constexpr uint16_t WP_PATH_LEN = 48;
  char _wpNames[MAX_WALLPAPERS][WP_NAME_LEN];
  char _wpPaths[MAX_WALLPAPERS][WP_PATH_LEN];
  uint8_t _wpCount = 0;
  int8_t _wpPreviewIndex = -1;

  void scanWallpapers();
  UI::Rect wpRowRect(uint8_t i) const { return {6, (int16_t)(Cfg::STATUS_BAR_H + 40 + i * 26), (int16_t)(Cfg::SCREEN_W - 12), 22}; }
};
