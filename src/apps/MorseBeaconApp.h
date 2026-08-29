#pragma once
#include "App.h"
#include "core/UI.h"

// Type a short message, flash it out in Morse code with the screen (a
// visual light signal - handy for the flashlight-adjacent "getting
// someone's attention at a distance" use case). Includes a one-tap SOS.
class MorseBeaconApp : public App {
public:
  const char* name() const override { return "Morse"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Mode { EDIT, SENDING };
  Mode _mode = EDIT;

  static constexpr size_t MAX_LEN = 40;
  char _text[MAX_LEN + 1] = {0};
  size_t _len = 0;

  static constexpr uint8_t NUM_ROWS = 4;
  const char* _rows[NUM_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};

  UI::Button _spaceBtn{{4, 0, 90, 26}, "SPACE"};
  UI::Button _delBtn{{98, 0, 80, 26}, "DEL"};
  UI::Button _clrBtn{{182, 0, 60, 26}, "CLR"};
  UI::Button _sosBtn{{246, 0, 70, 26}, "SOS"};
  UI::Button _sendBtn{{4, 0, Cfg::SCREEN_W - 8, 30}, "Send"};
  UI::Button _stopBtn{{Cfg::SCREEN_W / 2 - 50, Cfg::SCREEN_H - 44, 100, 32}, "Stop"};

  // Precomputed on/off schedule for the current send: positive = light on
  // for this many ms, negative = light off for this many ms.
  static constexpr uint16_t MAX_STEPS = 512;
  int16_t _steps[MAX_STEPS];
  uint16_t _stepCount = 0;
  uint16_t _stepIndex = 0;
  uint32_t _stepStart = 0;
  bool _isOn = false;

  bool _dirty = true;

  UI::Rect keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  void buildSchedule(const char* msg);
  void startSend(const char* msg);
  void drawEdit(TFT_eSPI& tft);
  void drawSending(TFT_eSPI& tft);
};
