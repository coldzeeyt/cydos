#pragma once
#include "App.h"
#include "core/UI.h"

// Generates a random password from a configurable character set and can
// beam it to a phone as a QR code (reuses the same QRCode render approach
// as QR Beamer) so you don't have to type it in by hand.
class PasswordGenApp : public App {
public:
  const char* name() const override { return "Password"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Mode { SETTINGS, QR };
  Mode _mode = SETTINGS;

  int _length = 16;
  bool _useUpper = true;
  bool _useDigits = true;
  bool _useSymbols = false;

  static constexpr size_t MAX_LEN = 32;
  char _password[MAX_LEN + 1] = {0};

  UI::Button _lenDown{{20, Cfg::STATUS_BAR_H + 6, 40, 32}, "-"};
  UI::Button _lenUp{{Cfg::SCREEN_W - 60, Cfg::STATUS_BAR_H + 6, 40, 32}, "+"};
  UI::Button _upperBtn{{10, Cfg::STATUS_BAR_H + 46, 96, 30}, "ABC"};
  UI::Button _digitBtn{{112, Cfg::STATUS_BAR_H + 46, 96, 30}, "123"};
  UI::Button _symBtn{{214, Cfg::STATUS_BAR_H + 46, 96, 30}, "#$%"};
  UI::Button _genBtn{{40, Cfg::STATUS_BAR_H + 84, Cfg::SCREEN_W - 80, 34}, "Generate"};
  UI::Button _qrBtn{{40, Cfg::STATUS_BAR_H + 158, Cfg::SCREEN_W - 80, 30}, "Show as QR"};
  UI::Button _backBtn{{10, Cfg::STATUS_BAR_H + 6, 90, 30}, "<- Edit"};

  bool _dirty = true;

  void generate();
  void drawSettings(TFT_eSPI& tft);
  void drawQr(TFT_eSPI& tft);
};
