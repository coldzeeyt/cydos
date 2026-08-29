#pragma once
#include "App.h"
#include "core/UI.h"

// Type a short message on an on-screen keyboard, then beam it as a QR
// code for another phone's camera to scan.
class QrBeamerApp : public App {
public:
  const char* name() const override { return "QR Beamer"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  static constexpr size_t MAX_LEN = 100;
  enum Mode { EDIT, SHOW };
  Mode _mode = EDIT;

  char _text[MAX_LEN + 1] = {0};
  size_t _len = 0;

  static constexpr uint8_t NUM_ROWS = 5;
  const char* _rows[NUM_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", "./:-_@"};

  UI::Button _spaceBtn{{4, 0, 100, 26}, "SPACE"};
  UI::Button _delBtn{{108, 0, 90, 26}, "DEL"};
  UI::Button _clrBtn{{202, 0, 54, 26}, "CLR"};
  UI::Button _genBtn{{4, 0, 312, 28}, "Generate QR ->"};
  UI::Button _backBtn{{10, Cfg::STATUS_BAR_H + 6, 90, 30}, "<- Edit"};

  bool _dirty = true;

  UI::Rect keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  void drawKeyboard(TFT_eSPI& tft);
  void drawQr(TFT_eSPI& tft);
};
