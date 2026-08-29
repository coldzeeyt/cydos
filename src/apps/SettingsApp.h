#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"
#include "core/Touch.h"
#include "core/UpdateChecker.h"

class AppManager;

// Global brightness (persisted across apps), WiFi setup for the update
// checker, and a touch test screen for dialing in Config.h's calibration
// constants.
class SettingsApp : public App {
public:
  SettingsApp(AppManager* mgr, Touch* touch, Preferences* prefs, UpdateChecker* updates)
      : _mgr(mgr), _touch(touch), _prefs(prefs), _updates(updates) {}

  const char* name() const override { return "Settings"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;
  void onTouchUp() override { _draggingSlider = false; }

private:
  enum Mode { MAIN, TOUCH_TEST, WIFI, WIFI_KEYBOARD };
  Mode _mode = MAIN;
  enum Field { FIELD_SSID, FIELD_PASS };
  Field _activeField = FIELD_SSID;

  AppManager* _mgr;
  Touch* _touch;
  Preferences* _prefs;
  UpdateChecker* _updates;

  UI::Slider _brightSlider{{40, 70, Cfg::SCREEN_W - 80, 30}, 80};
  UI::Button _touchTestBtn{{40, 122, Cfg::SCREEN_W - 80, 34}, "Touch Test"};
  UI::Button _wifiBtn{{40, 164, Cfg::SCREEN_W - 80, 34}, "WiFi Setup"};
  UI::Button _backBtn{{10, Cfg::STATUS_BAR_H + 6, 90, 30}, "<- Back"};
  UI::Button _wifiSaveBtn{{30, 158, 130, 34}, "Save"};
  UI::Button _wifiTestBtn{{170, 158, 120, 34}, "Test Now"};
  UI::Button _kbSpaceBtn{{4, 0, 100, 26}, "SPACE"};
  UI::Button _kbDelBtn{{108, 0, 90, 26}, "DEL"};
  UI::Button _kbClrBtn{{202, 0, 54, 26}, "CLR"};
  UI::Button _kbDoneBtn{{4, 0, Cfg::SCREEN_W - 8, 28}, "Done"};

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

  char* activeBuf() { return _activeField == FIELD_SSID ? _ssid : _pass; }
  size_t activeMax() { return _activeField == FIELD_SSID ? SSID_MAX : PASS_MAX; }
  UI::Rect kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  UI::Rect wifiSsidBox() const { return {30, Cfg::STATUS_BAR_H + 40, Cfg::SCREEN_W - 60, 34}; }
  UI::Rect wifiPassBox() const { return {30, Cfg::STATUS_BAR_H + 84, Cfg::SCREEN_W - 60, 34}; }
  void loadWifiCreds();
};
