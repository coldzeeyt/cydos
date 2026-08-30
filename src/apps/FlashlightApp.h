#pragma once
#include "App.h"
#include "core/UI.h"

class AppManager;

// Full-screen colored light with a brightness slider that drives the
// backlight PWM directly, plus a Sleep mode (screen goes black to save
// battery/avoid blinding someone at night - tap anywhere to wake back up
// to whatever color/brightness you had).
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

  static constexpr uint8_t NUM_COLORS = 5;

private:
  // Reuses the theme's existing palette rather than inventing new colors -
  // white/red/amber/cyan/green cover the useful flashlight cases (general
  // light, preserving night vision, a softer warm tone, and two options
  // for signaling/color-matching) without adding anything new to Theme.
  static const uint16_t COLOR_VALUES[NUM_COLORS];
  static const char* const COLOR_NAMES[NUM_COLORS];

  AppManager* _mgr;
  uint8_t _colorIdx = 0;
  bool _asleep = false;
  UI::Slider _slider{{40, Cfg::SCREEN_H - 40, Cfg::SCREEN_W - 80, 30}, 100};
  UI::Button _sleepBtn{{Cfg::SCREEN_W / 2 - 50, Cfg::STATUS_BAR_H + 10, 100, 32}, "Sleep"};
  UI::Rect swatchRect(uint8_t i) const;
  bool _dirty = true;
  bool _draggingSlider = false;

  void applyBacklight();
};
