#pragma once
#include <SD.h>
#include "App.h"
#include "core/UI.h"

// A single "no-code" screen loaded from a plain-text file on the SD card
// (see docs/store.html's Wallpapers/Apps tabs for the format, or
// CONTRIBUTING.md). Not a general programming interface - just a
// declarative static screen: a name, a background color, and a few lines
// of text. CydOs keeps a fixed pool of these (see main.cpp) and loads
// whichever .cydapp files it finds under /cydos_apps/ at boot - editing
// the SD card takes effect on the next power-on, not live.
class SdCardApp : public App {
public:
  const char* name() const override { return _loaded ? _name : "SD App"; }

  // Reads and parses one .cydapp file. Returns false (and leaves this
  // slot unloaded) if the file can't be opened - a missing or malformed
  // file is never a crash, just an empty slot nothing gets registered for.
  bool load(const char* path) {
    _loaded = false;
    File f = SD.open(path);
    if (!f) return false;

    _lineCount = 0;
    _bgColor = Theme::BG;
    _name[0] = 0;

    while (f.available()) {
      String raw = f.readStringUntil('\n');
      raw.trim();
      if (raw.length() == 0) continue;
      int eq = raw.indexOf('=');
      if (eq < 0) continue;
      String key = raw.substring(0, eq);
      String value = raw.substring(eq + 1);
      key.trim();
      value.trim();

      if (key == "name") {
        value.toCharArray(_name, sizeof(_name));
      } else if (key == "bg") {
        _bgColor = parseHexColor(value.c_str());
      } else if (key == "text" && _lineCount < MAX_LINES) {
        value.toCharArray(_lines[_lineCount], MAX_LINE_LEN + 1);
        _lineCount++;
      }
      // Unknown keys are ignored rather than rejected - keeps the format
      // forward-compatible with a future field this build doesn't know.
    }
    f.close();

    if (_name[0] == 0) strncpy(_name, "SD App", sizeof(_name));
    _loaded = true;
    _dirty = true;
    return true;
  }

  bool loaded() const { return _loaded; }

  void onEnter(TFT_eSPI& tft) override { _dirty = true; }

  void draw(TFT_eSPI& tft) override {
    if (!_dirty) return;
    _dirty = false;
    tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, _bgColor);
    int16_t y = Cfg::STATUS_BAR_H + (Cfg::SCREEN_H - Cfg::STATUS_BAR_H) / 2 - (int16_t)(_lineCount - 1) * 12;
    for (uint8_t i = 0; i < _lineCount; i++) {
      UI::centerText(tft, _lines[i], Cfg::SCREEN_W / 2, y, 2, Theme::TEXT);
      y += 24;
    }
    if (_lineCount == 0) {
      UI::centerText(tft, _name, Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 2, Theme::TEXT);
    }
  }

private:
  static constexpr uint8_t MAX_LINES = 6;
  static constexpr uint8_t MAX_LINE_LEN = 40;
  static constexpr uint8_t NAME_LEN = 24;

  // "#RRGGBB" (case-insensitive hex) -> RGB565. Anything malformed (wrong
  // length, no leading #, bad digits) falls back to the theme background
  // rather than guessing - strtol just reads what it can, so a bad string
  // quietly becomes black/near-black instead of erroring.
  static uint16_t parseHexColor(const char* s) {
    if (s[0] != '#' || strlen(s) != 7) return Theme::BG;
    long v = strtol(s + 1, nullptr, 16);
    uint8_t r = (v >> 16) & 0xFF, g = (v >> 8) & 0xFF, b = v & 0xFF;
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  char _name[NAME_LEN] = {0};
  uint16_t _bgColor = Theme::BG;
  char _lines[MAX_LINES][MAX_LINE_LEN + 1] = {{0}};
  uint8_t _lineCount = 0;
  bool _loaded = false;
  bool _dirty = true;
};
