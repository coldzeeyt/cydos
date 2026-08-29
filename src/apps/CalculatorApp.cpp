#include "CalculatorApp.h"
#include <math.h>

static const char* KEYS[4][4] = {
    {"7", "8", "9", "/"},
    {"4", "5", "6", "*"},
    {"1", "2", "3", "-"},
    {"C", "0", ".", "+"},
};
static constexpr int16_t KEY_TOP = Cfg::STATUS_BAR_H + 40;
static constexpr int16_t KEY_H = 34;
static constexpr int16_t KEY_W = Cfg::SCREEN_W / 4;
static constexpr int16_t EQ_Y = KEY_TOP + 4 * KEY_H;

static UI::Rect keyRect(int row, int col) {
  return {(int16_t)(col * KEY_W + 2), (int16_t)(KEY_TOP + row * KEY_H + 2), (int16_t)(KEY_W - 4), (int16_t)(KEY_H - 4)};
}

void CalculatorApp::clearAll() {
  strcpy(_display, "0");
  _acc = 0;
  _pendingOp = 0;
  _freshEntry = true;
  _error = false;
}

void CalculatorApp::setDisplay(double v) {
  if (isnan(v) || isinf(v)) {
    _error = true;
    strcpy(_display, "Err");
    return;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%.10g", v);
  strncpy(_display, buf, DISPLAY_MAX);
  _display[DISPLAY_MAX] = 0;
}

void CalculatorApp::pressDigit(char d) {
  if (_error) clearAll();
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

void CalculatorApp::pressDot() {
  if (_error) clearAll();
  if (_freshEntry) {
    strcpy(_display, "0.");
    _freshEntry = false;
  } else if (!strchr(_display, '.') && strlen(_display) < DISPLAY_MAX) {
    strcat(_display, ".");
  }
}

void CalculatorApp::applyPending() {
  double entry = atof(_display);
  switch (_pendingOp) {
    case '+': _acc += entry; break;
    case '-': _acc -= entry; break;
    case '*': _acc *= entry; break;
    case '/':
      if (entry == 0) { setDisplay(NAN); return; }
      _acc /= entry;
      break;
    default: _acc = entry; break;
  }
}

void CalculatorApp::pressOp(char op) {
  if (_error) { clearAll(); }
  if (_pendingOp && !_freshEntry) {
    applyPending();
    if (!_error) setDisplay(_acc);
  } else {
    _acc = atof(_display);
  }
  _pendingOp = op;
  _freshEntry = true;
}

void CalculatorApp::pressEquals() {
  if (_error) return;
  if (_pendingOp) {
    applyPending();
    if (!_error) setDisplay(_acc);
  }
  _pendingOp = 0;
  _freshEntry = true;
}

void CalculatorApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  UI::clearContent(tft);

  tft.fillRoundRect(6, Cfg::STATUS_BAR_H + 4, Cfg::SCREEN_W - 12, 30, 4, Theme::PANEL);
  tft.setTextColor(_error ? Theme::DANGER : Theme::TEXT, Theme::PANEL);
  tft.setTextDatum(MR_DATUM);
  tft.drawString(_display, Cfg::SCREEN_W - 16, Cfg::STATUS_BAR_H + 19, 4);
  tft.setTextDatum(TL_DATUM);

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      UI::Rect kr = keyRect(row, col);
      const char* label = KEYS[row][col];
      bool isOp = (col == 3) || (row == 3 && col == 0);
      uint16_t fill = isOp ? Theme::PANEL2 : Theme::PANEL;
      uint16_t txt = (row == 3 && col == 0) ? Theme::DANGER : (isOp ? Theme::ACCENT : Theme::TEXT);
      tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 6, fill);
      tft.setTextColor(txt, fill);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  UI::Rect eq{2, (int16_t)(EQ_Y + 2), Cfg::SCREEN_W - 4, (int16_t)(KEY_H - 4)};
  tft.fillRoundRect(eq.x, eq.y, eq.w, eq.h, 6, Theme::ACCENT);
  tft.setTextColor(Theme::BG, Theme::ACCENT);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("=", eq.x + eq.w / 2, eq.y + eq.h / 2 + 1, 2);
  tft.setTextDatum(TL_DATUM);
}

void CalculatorApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  UI::Rect eq{2, (int16_t)(EQ_Y + 2), Cfg::SCREEN_W - 4, (int16_t)(KEY_H - 4)};
  if (eq.contains(x, y)) {
    pressEquals();
    _dirty = true;
    return;
  }

  for (int row = 0; row < 4; row++) {
    for (int col = 0; col < 4; col++) {
      if (!keyRect(row, col).contains(x, y)) continue;
      const char* label = KEYS[row][col];
      char c = label[0];
      if (c == 'C') clearAll();
      else if (c == '.') pressDot();
      else if (c == '+' || c == '-' || c == '*' || c == '/') pressOp(c);
      else pressDigit(c);
      _dirty = true;
      return;
    }
  }
}
