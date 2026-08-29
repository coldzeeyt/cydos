#pragma once
#include "App.h"
#include "core/UI.h"
#include "core/Touch.h"

class AppManager;

// Global brightness (persisted across apps) + a touch test screen for
// dialing in Config.h's calibration constants.
class SettingsApp : public App {
public:
  SettingsApp(AppManager* mgr, Touch* touch) : _mgr(mgr), _touch(touch) {}

  const char* name() const override { return "Settings"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;
  void onTouchUp() override { _draggingSlider = false; }

private:
  enum Mode { MAIN, TOUCH_TEST };
  Mode _mode = MAIN;

  AppManager* _mgr;
  Touch* _touch;

  UI::Slider _brightSlider{{40, 70, Cfg::SCREEN_W - 80, 30}, 80};
  UI::Button _touchTestBtn{{40, 130, Cfg::SCREEN_W - 80, 36}, "Touch Test"};
  UI::Button _backBtn{{10, Cfg::STATUS_BAR_H + 6, 90, 30}, "<- Back"};

  bool _draggingSlider = false;
  bool _dirty = true;
  int16_t _lastTouchX = -1, _lastTouchY = -1;
  int16_t _lastRawX = -1, _lastRawY = -1;

  void drawMain(TFT_eSPI& tft);
  void drawTouchTest(TFT_eSPI& tft);
};
