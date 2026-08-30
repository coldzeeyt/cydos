#pragma once
#include "App.h"
#include "core/UI.h"

// Two tabs: Send (type a message, flash it out in Morse - a visual light
// signal, handy for the flashlight-adjacent "getting someone's attention
// at a distance" use case, includes a one-tap SOS) and Decode (key in
// Morse with big dot/dash buttons and read back the translated text - the
// reverse direction, for practicing or reading someone else's code).
class MorseBeaconApp : public App {
public:
  const char* name() const override { return "Morse"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Tab { TAB_SEND, TAB_DECODE, TAB_COUNT };
  Tab _tab = TAB_SEND;
  UI::Button _tabBtns[TAB_COUNT] = {
      {{0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W / 2, 22}, "Send"},
      {{Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H, Cfg::SCREEN_W / 2, 22}, "Decode"},
  };
  static constexpr int16_t CONTENT_TOP = Cfg::STATUS_BAR_H + 22;

  // ---- Send tab (encoder: text -> flashing light) ----
  enum Mode { EDIT, SENDING };
  Mode _mode = EDIT;

  static constexpr size_t MAX_LEN = 40;
  char _text[MAX_LEN + 1] = {0};
  size_t _len = 0;

  static constexpr uint8_t NUM_ROWS = 4;
  const char* _rows[NUM_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm"};

  UI::Button _spaceBtn{{4, 0, 90, 24}, "SPACE"};
  UI::Button _delBtn{{98, 0, 80, 24}, "DEL"};
  UI::Button _clrBtn{{182, 0, 60, 24}, "CLR"};
  UI::Button _sosBtn{{246, 0, 70, 24}, "SOS"};
  UI::Button _sendBtn{{4, 0, Cfg::SCREEN_W - 8, 24}, "Send"};
  UI::Button _stopBtn{{Cfg::SCREEN_W / 2 - 50, Cfg::SCREEN_H - 44, 100, 32}, "Stop"};

  // Precomputed on/off schedule for the current send: positive = light on
  // for this many ms, negative = light off for this many ms.
  static constexpr uint16_t MAX_STEPS = 512;
  int16_t _steps[MAX_STEPS];
  uint16_t _stepCount = 0;
  uint16_t _stepIndex = 0;
  uint32_t _stepStart = 0;
  bool _isOn = false;

  // ---- Decode tab (decoder: keyed dots/dashes -> text) ----
  static constexpr size_t DECODE_MAX_LEN = 40;
  char _decodedText[DECODE_MAX_LEN + 1] = {0};
  size_t _decodedLen = 0;

  static constexpr uint8_t CUR_SYMS_MAX = 7; // longest real code is 5 symbols; a little headroom
  char _curSymbols[CUR_SYMS_MAX + 1] = {0};
  uint8_t _curSymsLen = 0;

  // Mirrors what buildMorseText() would produce for the message being
  // keyed in, built live as symbols are pressed - display only.
  static constexpr size_t RAW_MAX = 255;
  char _rawMorse[RAW_MAX + 1] = {0};
  size_t _rawLen = 0;

  UI::Button _dotBtn{{4, 0, 150, 76}, "DOT"};
  UI::Button _dashBtn{{166, 0, 150, 76}, "DASH"};
  UI::Button _gapBtn{{4, 0, 74, 26}, "Gap"};
  UI::Button _wordBtn{{82, 0, 74, 26}, "Word"};
  UI::Button _decDelBtn{{160, 0, 74, 26}, "DEL"};
  UI::Button _decClrBtn{{238, 0, 74, 26}, "CLR"};

  bool _dirty = true;

  UI::Rect keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  void buildSchedule(const char* msg);
  void startSend(const char* msg);
  void drawTabs(TFT_eSPI& tft);
  void drawEdit(TFT_eSPI& tft);
  void drawSending(TFT_eSPI& tft);
  void drawDecode(TFT_eSPI& tft);
  void appendRaw(char c);
  void finalizeLetter();
  void onTouchSend(int16_t x, int16_t y);
  void onTouchDecode(int16_t x, int16_t y);
};
