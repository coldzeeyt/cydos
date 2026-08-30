#include "UnitConverterApp.h"
#include <math.h>

static const char* KEYS[4][3] = {
    {"7", "8", "9"},
    {"4", "5", "6"},
    {"1", "2", "3"},
    {".", "0", "DEL"},
};
static constexpr int16_t KEY_TOP = Cfg::STATUS_BAR_H + 110;
static constexpr int16_t KEY_H = 24;
static constexpr int16_t KEY_W = Cfg::SCREEN_W / 3;

static UI::Rect keyRect(int row, int col) {
  return {(int16_t)(col * KEY_W + 2), (int16_t)(KEY_TOP + row * KEY_H + 2), (int16_t)(KEY_W - 4), (int16_t)(KEY_H - 4)};
}

const char* UnitConverterApp::fromLabel() const {
  switch (_unit) {
    case LENGTH: return _swapped ? "ft" : "m";
    case WEIGHT: return _swapped ? "lb" : "kg";
    default: return _swapped ? "F" : "C";
  }
}

const char* UnitConverterApp::toLabel() const {
  switch (_unit) {
    case LENGTH: return _swapped ? "m" : "ft";
    case WEIGHT: return _swapped ? "kg" : "lb";
    default: return _swapped ? "C" : "F";
  }
}

double UnitConverterApp::convert(double v) const {
  switch (_unit) {
    case LENGTH: return _swapped ? v / 3.28084 : v * 3.28084;
    case WEIGHT: return _swapped ? v / 2.20462 : v * 2.20462;
    default: return _swapped ? (v - 32) * 5.0 / 9.0 : v * 9.0 / 5.0 + 32;
  }
}

void UnitConverterApp::pressDigit(char d) {
  if (_freshEntry) {
    _display[0] = d;
    _display[1] = 0;
    _freshEntry = false;
  } else if (strlen(_display) < DISPLAY_MAX) {
    size_t n = strlen(_display);
    _display[n] = d;
    _display[n + 1] = 0;
  }
}

void UnitConverterApp::pressDot() {
  if (_freshEntry) {
    strcpy(_display, "0.");
    _freshEntry = false;
  } else if (!strchr(_display, '.') && strlen(_display) < DISPLAY_MAX) {
    strcat(_display, ".");
  }
}

void UnitConverterApp::clearEntry() {
  strcpy(_display, "0");
  _freshEntry = true;
}

void UnitConverterApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  UI::clearContent(tft);

  for (uint8_t i = 0; i < UNIT_COUNT; i++) {
    _tabBtns[i].active = (i == (uint8_t)_unit);
    _tabBtns[i].draw(tft);
  }

  double result = convert(atof(_display));

  tft.fillRoundRect(6, Cfg::STATUS_BAR_H + 28, Cfg::SCREEN_W - 12, 26, 4, Theme::PANEL);
  tft.setTextColor(Theme::TEXT, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(fromLabel(), 12, Cfg::STATUS_BAR_H + 41, 2);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(_display, Cfg::SCREEN_W - 16, Cfg::STATUS_BAR_H + 41, 2);
  tft.setTextDatum(TL_DATUM);

  tft.fillRoundRect(6, Cfg::STATUS_BAR_H + 58, Cfg::SCREEN_W - 12, 26, 4, Theme::PANEL2);
  tft.setTextColor(Theme::ACCENT, Theme::PANEL2);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(toLabel(), 12, Cfg::STATUS_BAR_H + 71, 2);
  tft.setTextDatum(MR_DATUM);
  char resBuf[24];
  snprintf(resBuf, sizeof(resBuf), "%.4g", result);
  tft.drawString(resBuf, Cfg::SCREEN_W - 16, Cfg::STATUS_BAR_H + 71, 2);
  tft.setTextDatum(TL_DATUM);

  _swapBtn.draw(tft);

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 3; col++) {
      UI::Rect kr = keyRect(row, col);
      const char* label = KEYS[row][col];
      bool isDel = (row == 3 && col == 2);
      uint16_t fill = isDel ? Theme::PANEL2 : Theme::PANEL;
      tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 6, fill);
      tft.setTextColor(isDel ? Theme::DANGER : Theme::TEXT, fill);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }
}

void UnitConverterApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  for (uint8_t i = 0; i < UNIT_COUNT; i++) {
    if (_tabBtns[i].hit(x, y)) {
      _unit = (Unit)i;
      _swapped = false;
      clearEntry();
      _dirty = true;
      return;
    }
  }

  if (_swapBtn.hit(x, y)) {
    _swapped = !_swapped;
    clearEntry();
    _dirty = true;
    return;
  }

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 3; col++) {
      if (!keyRect(row, col).contains(x, y)) continue;
      const char* label = KEYS[row][col];
      if (strcmp(label, "DEL") == 0) {
        size_t n = strlen(_display);
        if (n > 1) { _display[n - 1] = 0; }
        else clearEntry();
      } else if (label[0] == '.') {
        pressDot();
      } else {
        pressDigit(label[0]);
      }
      _dirty = true;
      return;
    }
  }
}
