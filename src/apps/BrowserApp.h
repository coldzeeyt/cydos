#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"

// A "browser" in the loosest possible sense: type a URL, fetch it over
// WiFi, strip out every tag, and show whatever text is left, word-wrapped
// and scrollable. No CSS, no images, no JS - just enough to read an
// article or check a status page from a 320x240 screen.
class BrowserApp : public App {
public:
  explicit BrowserApp(Preferences* prefs) : _prefs(prefs) {}

  const char* name() const override { return "Browser"; }

  void onEnter(TFT_eSPI& tft) override;
  void onExit() override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;
  void onTouchUp() override { _dragging = false; }

private:
  enum Mode { URL_EDIT, PAGE };
  Mode _mode = URL_EDIT;

  Preferences* _prefs;

  static constexpr size_t URL_MAX = 96;
  char _url[URL_MAX + 1] = "example.com";
  size_t _urlLen = 11;

  static constexpr size_t TEXT_MAX = 6000;
  char _text[TEXT_MAX + 1] = {0};
  size_t _textLen = 0;

  // HTML-stripper state, reset at the start of every fetch.
  bool _stripInTag = false, _stripInScript = false, _stripInStyle = false;
  bool _stripLastSpace = true;

  struct Line { uint16_t start, len; };
  static constexpr uint16_t MAX_LINES = 400;
  Line _lines[MAX_LINES];
  uint16_t _lineCount = 0;
  static constexpr uint8_t CHARS_PER_LINE = 50;
  static constexpr int16_t LINE_H = 14;
  int _scrollLine = 0;
  int _linesVisible = 0;

  bool _dragging = false;
  int16_t _dragStartY = 0;
  int _dragStartScroll = 0;

  static constexpr uint8_t NUM_ROWS = 5;
  const char* _rows[NUM_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", ".:/-_?=&"};

  UI::Button _delBtn{{4, 0, 100, 26}, "DEL"};
  UI::Button _clrBtn{{108, 0, 90, 26}, "CLR"};
  UI::Button _goBtn{{202, 0, 114, 26}, "Go"};
  UI::Button _backToUrlBtn{{4, Cfg::STATUS_BAR_H + 3, 66, 22}, "<-URL"};

  bool _dirty = true;

  UI::Rect keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  void drawUrlEdit(TFT_eSPI& tft);
  void drawPage(TFT_eSPI& tft);
  void doFetch(TFT_eSPI& tft);
  void appendPlain(const char* s);
  void stripChunk(const char* buf, size_t n);
  void wrapText();
};
