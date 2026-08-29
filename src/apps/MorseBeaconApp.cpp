#include "MorseBeaconApp.h"
#include <ctype.h>

static const char* LETTER_CODES[26] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
    "..-", "...-", ".--", "-..-", "-.--", "--..",
};
static const char* DIGIT_CODES[10] = {
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----.",
};
static const uint16_t UNIT_MS = 200;

static const char* morseFor(char c) {
  if (c >= 'a' && c <= 'z') return LETTER_CODES[c - 'a'];
  if (c >= '0' && c <= '9') return DIGIT_CODES[c - '0'];
  return nullptr;
}

void MorseBeaconApp::onEnter(TFT_eSPI& tft) {
  _mode = EDIT;
  _dirty = true;
}

UI::Rect MorseBeaconApp::keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 26;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

void MorseBeaconApp::buildSchedule(const char* msg) {
  _stepCount = 0;
  auto push = [&](int16_t v) { if (_stepCount < MAX_STEPS) _steps[_stepCount++] = v; };

  for (const char* p = msg; *p && _stepCount < MAX_STEPS - 8; p++) {
    char c = tolower(*p);
    if (c == ' ') {
      push(-(int16_t)(UNIT_MS * 4)); // top up the previous letter-gap to a 7-unit word gap
      continue;
    }
    const char* code = morseFor(c);
    if (!code) continue;
    for (const char* s = code; *s; s++) {
      push(*s == '.' ? UNIT_MS : UNIT_MS * 3);
      push(-(int16_t)UNIT_MS);
    }
    push(-(int16_t)(UNIT_MS * 2)); // top up to a 3-unit letter gap
  }
}

void MorseBeaconApp::startSend(const char* msg) {
  buildSchedule(msg);
  if (_stepCount == 0) return;
  _mode = SENDING;
  _stepIndex = 0;
  _stepStart = millis();
  _isOn = _steps[0] > 0;
  _dirty = true;
}

bool MorseBeaconApp::update() {
  if (_mode != SENDING) return false;
  uint32_t elapsed = millis() - _stepStart;
  if (elapsed >= (uint32_t)abs(_steps[_stepIndex])) {
    _stepIndex++;
    if (_stepIndex >= _stepCount) {
      _mode = EDIT;
      _dirty = true;
      return true;
    }
    _stepStart = millis();
    _isOn = _steps[_stepIndex] > 0;
    _dirty = true;
  }
  return _dirty;
}

void MorseBeaconApp::drawEdit(TFT_eSPI& tft) {
  UI::clearContent(tft);

  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, Cfg::SCREEN_W - 8, 24, 4, Theme::PANEL);
  const char* shown = _text;
  if (_len > 34) shown = _text + (_len - 34);
  tft.setTextColor(_len ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_len ? shown : "type a message to flash...", 10, Cfg::STATUS_BAR_H + 14, 2);
  tft.setTextDatum(TL_DATUM);

  for (uint8_t r = 0; r < NUM_ROWS; r++) {
    uint8_t len = strlen(_rows[r]);
    for (uint8_t c = 0; c < len; c++) {
      UI::Rect kr = keyRect(r, c, len);
      char label[2] = {_rows[r][c], 0};
      tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 4, Theme::PANEL);
      tft.setTextColor(Theme::TEXT, Theme::PANEL);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  int16_t ctrlY = Cfg::STATUS_BAR_H + 30 + NUM_ROWS * 26 + 4;
  _spaceBtn.r.y = ctrlY;
  _delBtn.r.y = ctrlY;
  _clrBtn.r.y = ctrlY;
  _sosBtn.r.y = ctrlY;
  _spaceBtn.draw(tft);
  _delBtn.draw(tft);
  _clrBtn.draw(tft);
  _sosBtn.draw(tft);

  _sendBtn.r.y = ctrlY + 30;
  _sendBtn.color = _len ? Theme::PANEL : Theme::PANEL2;
  _sendBtn.textColor = _len ? Theme::ACCENT : Theme::MUTED;
  _sendBtn.draw(tft);
}

void MorseBeaconApp::drawSending(TFT_eSPI& tft) {
  uint16_t fill = _isOn ? Theme::TEXT : Theme::BG;
  tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, fill);
  UI::centerText(tft, _text, Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 30, 2, _isOn ? Theme::BG : Theme::MUTED);
  _stopBtn.draw(tft);
}

void MorseBeaconApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  if (_mode == EDIT) drawEdit(tft);
  else drawSending(tft);
}

void MorseBeaconApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  if (_mode == SENDING) {
    if (_stopBtn.hit(x, y)) { _mode = EDIT; _dirty = true; }
    return;
  }

  for (uint8_t r = 0; r < NUM_ROWS; r++) {
    uint8_t len = strlen(_rows[r]);
    for (uint8_t c = 0; c < len; c++) {
      if (keyRect(r, c, len).contains(x, y) && _len < MAX_LEN) {
        _text[_len++] = _rows[r][c];
        _text[_len] = 0;
        _dirty = true;
        return;
      }
    }
  }

  if (_spaceBtn.hit(x, y) && _len < MAX_LEN) { _text[_len++] = ' '; _text[_len] = 0; _dirty = true; }
  else if (_delBtn.hit(x, y) && _len > 0) { _text[--_len] = 0; _dirty = true; }
  else if (_clrBtn.hit(x, y)) { _len = 0; _text[0] = 0; _dirty = true; }
  else if (_sosBtn.hit(x, y)) { startSend("sos"); }
  else if (_sendBtn.hit(x, y) && _len > 0) { startSend(_text); }
}
