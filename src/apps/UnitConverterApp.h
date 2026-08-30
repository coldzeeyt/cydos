#pragma once
#include "App.h"
#include "core/UI.h"

// Length (m<->ft), Weight (kg<->lb) and Temperature (C<->F) conversion,
// one pair of units at a time. Type a number on the keypad, read the
// converted value live above it; Swap flips which unit you're typing in.
class UnitConverterApp : public App {
public:
  const char* name() const override { return "Converter"; }

  void onEnter(TFT_eSPI& tft) override { clearEntry(); _dirty = true; }
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Unit { LENGTH, WEIGHT, TEMP, UNIT_COUNT };
  Unit _unit = LENGTH;
  bool _swapped = false; // false: display[0] is the "from" unit, true: reversed

  static constexpr size_t DISPLAY_MAX = 16;
  char _display[DISPLAY_MAX + 1] = "0";
  bool _freshEntry = true;
  bool _dirty = true;

  UI::Button _tabBtns[UNIT_COUNT] = {
      {{0, Cfg::STATUS_BAR_H + 2, (int16_t)(Cfg::SCREEN_W / 3), 22}, "Length"},
      {{(int16_t)(Cfg::SCREEN_W / 3), Cfg::STATUS_BAR_H + 2, (int16_t)(Cfg::SCREEN_W / 3), 22}, "Weight"},
      {{(int16_t)(2 * Cfg::SCREEN_W / 3), Cfg::STATUS_BAR_H + 2, (int16_t)(Cfg::SCREEN_W - 2 * (Cfg::SCREEN_W / 3)), 22}, "Temp"},
  };
  UI::Button _swapBtn{{(int16_t)(Cfg::SCREEN_W / 2 - 28), Cfg::STATUS_BAR_H + 86, 56, 20}, "Swap"};

  const char* fromLabel() const;
  const char* toLabel() const;
  double convert(double v) const;
  void pressDigit(char d);
  void pressDot();
  void clearEntry();
};
