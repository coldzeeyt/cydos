#include "DiceApp.h"

static const char* MODE_NAMES[] = {"Mode: D6", "Mode: D20", "Mode: Coin"};
static const uint32_t ROLL_MS = 700;
static const uint32_t FLICKER_MS = 60;

void DiceApp::startRoll() {
  _rolling = true;
  _rollStart = millis();
  _lastFlicker = 0;
  _dirty = true;
}

bool DiceApp::update() {
  if (!_rolling) return false;
  uint32_t now = millis();
  uint32_t elapsed = now - _rollStart;

  if (now - _lastFlicker > FLICKER_MS) {
    _lastFlicker = now;
    if (_mode == D6) _result = random(1, 7);
    else if (_mode == D20) _result = random(1, 21);
    else _result = random(0, 2);
    _dirty = true;
  }

  if (elapsed > ROLL_MS) {
    _rolling = false;
    _dirty = true;
  }
  return _dirty;
}

void DiceApp::drawPips(TFT_eSPI& tft, int16_t cx, int16_t cy, int16_t size, int value) {
  tft.fillRoundRect(cx - size, cy - size, size * 2, size * 2, 12, Theme::TEXT);
  tft.drawRoundRect(cx - size, cy - size, size * 2, size * 2, 12, Theme::MUTED);
  int16_t off = size / 2;
  int16_t r = 5;
  auto pip = [&](int dx, int dy) { tft.fillCircle(cx + dx * off, cy + dy * off, r, Theme::BG); };
  switch (value) {
    case 1: pip(0, 0); break;
    case 2: pip(-1, -1); pip(1, 1); break;
    case 3: pip(-1, -1); pip(0, 0); pip(1, 1); break;
    case 4: pip(-1, -1); pip(1, -1); pip(-1, 1); pip(1, 1); break;
    case 5: pip(-1, -1); pip(1, -1); pip(0, 0); pip(-1, 1); pip(1, 1); break;
    case 6: pip(-1, -1); pip(1, -1); pip(-1, 0); pip(1, 0); pip(-1, 1); pip(1, 1); break;
  }
}

void DiceApp::drawResult(TFT_eSPI& tft) {
  int16_t cx = Cfg::SCREEN_W / 2;
  int16_t cy = Cfg::STATUS_BAR_H + 130;
  tft.fillRect(0, Cfg::STATUS_BAR_H + 55, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H - 55, Theme::BG);

  if (_mode == D6) {
    drawPips(tft, cx, cy, 55, _result);
  } else if (_mode == D20) {
    tft.fillCircle(cx, cy, 60, Theme::ACCENT);
    char buf[4];
    snprintf(buf, sizeof(buf), "%d", _result);
    tft.setTextColor(Theme::BG, Theme::ACCENT);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buf, cx, cy + 2, 7);
    tft.setTextDatum(TL_DATUM);
  } else {
    bool heads = _result == 0;
    tft.fillCircle(cx, cy, 60, heads ? Theme::ACCENT2 : Theme::MUTED);
    tft.setTextColor(Theme::BG, heads ? Theme::ACCENT2 : Theme::MUTED);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(heads ? "HEADS" : "TAILS", cx, cy + 2, 4);
    tft.setTextDatum(TL_DATUM);
  }
}

void DiceApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  _modeBtn.label = MODE_NAMES[_mode];
  _rollBtn.label = _rolling ? "..." : "ROLL";
  _modeBtn.draw(tft);
  _rollBtn.draw(tft);
  drawResult(tft);
}

void DiceApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down || _rolling) return;
  if (_modeBtn.hit(x, y)) {
    _mode = (Mode)((_mode + 1) % MODE_COUNT);
    _dirty = true;
  } else if (_rollBtn.hit(x, y)) {
    startRoll();
  }
}
