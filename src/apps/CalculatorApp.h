#pragma once
#include "App.h"
#include "core/UI.h"

// A plain four-function calculator. Classic accumulator + pending-operator
// design (no expression parsing) - press a number, an operator, another
// number, then "=".
class CalculatorApp : public App {
public:
  const char* name() const override { return "Calculator"; }

  void onEnter(TFT_eSPI& tft) override { clearAll(); _dirty = true; }
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  static constexpr size_t DISPLAY_MAX = 16;
  char _display[DISPLAY_MAX + 1] = "0";
  double _acc = 0;
  char _pendingOp = 0;
  bool _freshEntry = true;
  bool _error = false;
  bool _dirty = true;

  void clearAll();
  void pressDigit(char d);
  void pressDot();
  void pressOp(char op);
  void pressEquals();
  void applyPending();
  void setDisplay(double v);
};
