#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"

// A single persistent plain-text note - type it once, it's still there
// next time you open the app, and across reboots (saved via Preferences).
// Not a rich notes app: one buffer, no formatting, no multiple notes.
class NotesApp : public App {
public:
  explicit NotesApp(Preferences* prefs) : _prefs(prefs) {}

  const char* name() const override { return "Notes"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Mode { VIEW, EDIT };
  Mode _mode = VIEW;

  Preferences* _prefs;
  static constexpr size_t MAX_LEN = 200;
  char _text[MAX_LEN + 1] = {0};

  static constexpr uint8_t KB_ROWS = 5;
  const char* _kbRows[KB_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", ".,:-_@!?"};
  UI::Button _editBtn{{Cfg::SCREEN_W - 70, Cfg::STATUS_BAR_H + 2, 60, 22}, "Edit"};
  UI::Button _kbSpaceBtn{{4, 0, 100, 26}, "SPACE"};
  UI::Button _kbDelBtn{{108, 0, 90, 26}, "DEL"};
  UI::Button _kbClrBtn{{202, 0, 54, 26}, "CLR"};
  UI::Button _kbDoneBtn{{4, 0, Cfg::SCREEN_W - 8, 28}, "Done"};

  bool _dirty = true;

  UI::Rect kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  void drawView(TFT_eSPI& tft);
  void drawEdit(TFT_eSPI& tft);
};
