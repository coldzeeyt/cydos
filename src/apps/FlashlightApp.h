#pragma once
#include "App.h"
#include "core/UI.h"

class AppManager;

// Full-screen white or red light with a brightness slider that drives the
// backlight PWM directly. Dumb, but you'll use it constantly.
class FlashlightApp : public App {
public:
  explicit FlashlightApp(AppManager* mgr) : _mgr(mgr) {}

  const char* name() const override { return "Flashlight"; }

  void onEnter(TFT_eSPI& tft) override;
  void onExit() override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;
  void onTouchUp() override { _draggingSlider = false; }

private:
  AppManager* _mgr;
  bool _red = false;
  UI::Slider _slider{{40, Cfg::SCREEN_H - 40, Cfg::SCREEN_W - 80, 30}, 100};
  UI::Button _toggle{{Cfg::SCREEN_W / 2 - 60, Cfg::STATUS_BAR_H + 10, 120, 36}, "Switch: RED"};
  bool _dirty = true;
  bool _draggingSlider = false;

  void applyBacklight();
};
