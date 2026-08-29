#pragma once
#include "App.h"
#include "core/UI.h"

class DiceApp : public App {
public:
  const char* name() const override { return "Dice"; }

  void onEnter(TFT_eSPI& tft) override { _dirty = true; _rolling = false; }
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Mode { D6, D20, COIN, MODE_COUNT };
  Mode _mode = D6;

  UI::Button _modeBtn{{10, Cfg::STATUS_BAR_H + 10, 130, 34}, "Mode: D6"};
  UI::Button _rollBtn{{170, Cfg::STATUS_BAR_H + 10, 140, 34}, "ROLL"};

  int _result = 1;
  bool _rolling = false;
  uint32_t _rollStart = 0;
  uint32_t _lastFlicker = 0;
  bool _dirty = true;

  void startRoll();
  void drawResult(TFT_eSPI& tft);
  void drawPips(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t size, int value);
};
