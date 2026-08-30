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

// Reverse of morseFor(): a dot/dash code back to the letter or digit it
// represents, '?' if it doesn't match anything (e.g. a mis-keyed code).
static char decodeSymbols(const char* code) {
  for (uint8_t i = 0; i < 26; i++) {
    if (!strcmp(code, LETTER_CODES[i])) return 'a' + i;
  }
  for (uint8_t i = 0; i < 10; i++) {
    if (!strcmp(code, DIGIT_CODES[i])) return '0' + i;
  }
  return '?';
}

// Renders the dot/dash code for a message - letters separated by a space,
// words by " / " - so you can read the pattern being flashed, not just the
// plain-text message. `out` should be sized generously: a message full of
// digits (5 symbols each) is the worst case.
static void buildMorseText(const char* msg, char* out, size_t outCap) {
  size_t pos = 0;
  bool needLetterSpace = false;
  for (const char* p = msg; *p; p++) {
    char c = tolower(*p);
    if (c == ' ') {
      if (pos + 3 < outCap) { out[pos++] = ' '; out[pos++] = '/'; out[pos++] = ' '; }
      needLetterSpace = false;
      continue;
    }
    const char* code = morseFor(c);
    if (!code) continue;
    if (needLetterSpace && pos + 1 < outCap) out[pos++] = ' ';
    for (const char* s = code; *s && pos + 1 < outCap; s++) out[pos++] = *s;
    needLetterSpace = true;
  }
  out[pos < outCap ? pos : outCap - 1] = 0;
}

// Word-wraps `text` (breaking on spaces where possible) into up to
// maxLines centered lines, maxCharsPerLine wide, starting at yStart and
// stepping by lineH. Anything past maxLines is simply not shown - fine
// for a nice-to-have readout, the actual flash timing doesn't depend on it.
static void drawWrappedCenter(TFT_eSPI& tft, const char* text, int16_t cx, int16_t yStart,
                               int16_t lineH, uint8_t font, uint16_t color,
                               uint8_t maxLines, uint8_t maxCharsPerLine) {
  size_t len = strlen(text);
  size_t pos = 0;
  for (uint8_t line = 0; line < maxLines && pos < len; line++) {
    size_t remaining = len - pos;
    size_t take = remaining < maxCharsPerLine ? remaining : maxCharsPerLine;
    size_t breakAt = take;
    if (pos + take < len) {
      size_t i = take;
      while (i > 0 && text[pos + i - 1] != ' ') i--;
      if (i > 0) breakAt = i;
    }
    char buf[64];
    size_t n = breakAt < sizeof(buf) - 1 ? breakAt : sizeof(buf) - 1;
    memcpy(buf, text + pos, n);
    buf[n] = 0;
    UI::centerText(tft, buf, cx, yStart + line * lineH, font, color);
    pos += breakAt;
    while (pos < len && text[pos] == ' ') pos++;
  }
}

void MorseBeaconApp::onEnter(TFT_eSPI& tft) {
  _mode = EDIT;
  _dirty = true;
}

UI::Rect MorseBeaconApp::keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = CONTENT_TOP + 40; // room for the text field + dot/dash preview line above
  int16_t rowH = 24;
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
  if (_tab != TAB_SEND || _mode != SENDING) return false;
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

void MorseBeaconApp::drawTabs(TFT_eSPI& tft) {
  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    bool active = (i == _tab);
    _tabBtns[i].color = active ? Theme::ACCENT : Theme::PANEL;
    _tabBtns[i].textColor = active ? Theme::BG : Theme::MUTED;
    _tabBtns[i].draw(tft);
  }
}

void MorseBeaconApp::drawEdit(TFT_eSPI& tft) {
  tft.fillRoundRect(4, CONTENT_TOP + 2, Cfg::SCREEN_W - 8, 22, 4, Theme::PANEL);
  const char* shown = _text;
  if (_len > 34) shown = _text + (_len - 34);
  tft.setTextColor(_len ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_len ? shown : "type a message to flash...", 10, CONTENT_TOP + 13, 2);
  tft.setTextDatum(TL_DATUM);

  // Live dot/dash preview of what Send will actually flash.
  char morse[256];
  buildMorseText(_text, morse, sizeof(morse));
  size_t morseLen = strlen(morse);
  const char* morseShown = morseLen > 50 ? morse + (morseLen - 50) : morse;
  tft.setTextColor(_len ? Theme::ACCENT : Theme::MUTED, Theme::BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_len ? morseShown : "morse code appears here...", 8, CONTENT_TOP + 32, 1);
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

  int16_t ctrlY = CONTENT_TOP + 40 + NUM_ROWS * 24 + 2;
  _spaceBtn.r.y = ctrlY;
  _delBtn.r.y = ctrlY;
  _clrBtn.r.y = ctrlY;
  _sosBtn.r.y = ctrlY;
  _spaceBtn.draw(tft);
  _delBtn.draw(tft);
  _clrBtn.draw(tft);
  _sosBtn.draw(tft);

  _sendBtn.r.y = ctrlY + 26;
  _sendBtn.color = _len ? Theme::PANEL : Theme::PANEL2;
  _sendBtn.textColor = _len ? Theme::ACCENT : Theme::MUTED;
  _sendBtn.draw(tft);
}

void MorseBeaconApp::drawSending(TFT_eSPI& tft) {
  uint16_t fill = _isOn ? Theme::TEXT : Theme::BG;
  tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, fill);
  uint16_t fg = _isOn ? Theme::BG : Theme::MUTED;
  UI::centerText(tft, _text, Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 26, 2, fg);

  char morse[256];
  buildMorseText(_text, morse, sizeof(morse));
  drawWrappedCenter(tft, morse, Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 54, 18, 2, fg, 5, 20);

  _stopBtn.draw(tft);
}

void MorseBeaconApp::appendRaw(char c) {
  if (_rawLen < RAW_MAX) { _rawMorse[_rawLen++] = c; _rawMorse[_rawLen] = 0; }
}

void MorseBeaconApp::finalizeLetter() {
  if (_curSymsLen == 0) return;
  char c = decodeSymbols(_curSymbols);
  if (_decodedLen < DECODE_MAX_LEN) { _decodedText[_decodedLen++] = c; _decodedText[_decodedLen] = 0; }
  _curSymsLen = 0;
  _curSymbols[0] = 0;
}

void MorseBeaconApp::drawDecode(TFT_eSPI& tft) {
  tft.fillRoundRect(4, CONTENT_TOP + 2, Cfg::SCREEN_W - 8, 22, 4, Theme::PANEL);
  const char* shownText = _decodedText;
  if (_decodedLen > 34) shownText = _decodedText + (_decodedLen - 34);
  tft.setTextColor(_decodedLen ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_decodedLen ? shownText : "decoded text appears here...", 10, CONTENT_TOP + 13, 2);
  tft.setTextDatum(TL_DATUM);

  const char* shownRaw = _rawLen > 50 ? _rawMorse + (_rawLen - 50) : _rawMorse;
  tft.setTextColor(_rawLen ? Theme::ACCENT : Theme::MUTED, Theme::BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_rawLen ? shownRaw : "key in . and - below...", 8, CONTENT_TOP + 32, 1);
  tft.setTextDatum(TL_DATUM);

  int16_t bigY = CONTENT_TOP + 40;
  _dotBtn.r.y = bigY;
  _dashBtn.r.y = bigY;
  _dotBtn.draw(tft);
  _dashBtn.draw(tft);

  int16_t ctrlY = bigY + 76 + 4;
  _gapBtn.r.y = ctrlY;
  _wordBtn.r.y = ctrlY;
  _decDelBtn.r.y = ctrlY;
  _decClrBtn.r.y = ctrlY;
  _gapBtn.draw(tft);
  _wordBtn.draw(tft);
  _decDelBtn.draw(tft);
  _decClrBtn.draw(tft);
}

void MorseBeaconApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;

  if (_tab == TAB_SEND && _mode == SENDING) {
    drawSending(tft);
    return;
  }

  UI::clearContent(tft);
  drawTabs(tft);
  if (_tab == TAB_SEND) drawEdit(tft);
  else drawDecode(tft);
}

void MorseBeaconApp::onTouchSend(int16_t x, int16_t y) {
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

void MorseBeaconApp::onTouchDecode(int16_t x, int16_t y) {
  if (_dotBtn.hit(x, y)) {
    if (_curSymsLen < CUR_SYMS_MAX) { _curSymbols[_curSymsLen++] = '.'; _curSymbols[_curSymsLen] = 0; appendRaw('.'); }
    _dirty = true;
  } else if (_dashBtn.hit(x, y)) {
    if (_curSymsLen < CUR_SYMS_MAX) { _curSymbols[_curSymsLen++] = '-'; _curSymbols[_curSymsLen] = 0; appendRaw('-'); }
    _dirty = true;
  } else if (_gapBtn.hit(x, y)) {
    if (_curSymsLen > 0) { finalizeLetter(); appendRaw(' '); _dirty = true; }
  } else if (_wordBtn.hit(x, y)) {
    bool hadPending = _curSymsLen > 0;
    finalizeLetter();
    if (hadPending) appendRaw(' ');
    appendRaw('/');
    appendRaw(' ');
    if (_decodedLen < DECODE_MAX_LEN) { _decodedText[_decodedLen++] = ' '; _decodedText[_decodedLen] = 0; }
    _dirty = true;
  } else if (_decDelBtn.hit(x, y)) {
    // Only undoes the symbol currently being keyed in - once a letter is
    // finalized (Gap/Word), Clear is the way to fix a mistake. Keeps the
    // raw/decoded views always exactly in sync with no reverse-parsing.
    if (_curSymsLen > 0) {
      _curSymsLen--;
      _curSymbols[_curSymsLen] = 0;
      if (_rawLen > 0) { _rawLen--; _rawMorse[_rawLen] = 0; }
      _dirty = true;
    }
  } else if (_decClrBtn.hit(x, y)) {
    _curSymsLen = 0; _curSymbols[0] = 0;
    _decodedLen = 0; _decodedText[0] = 0;
    _rawLen = 0; _rawMorse[0] = 0;
    _dirty = true;
  }
}

void MorseBeaconApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  (void)tft;
  if (!down) return;

  if (_tab == TAB_SEND && _mode == SENDING) {
    onTouchSend(x, y);
    return;
  }

  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    if (_tabBtns[i].hit(x, y)) {
      _tab = (Tab)i;
      _dirty = true;
      return;
    }
  }

  if (_tab == TAB_SEND) onTouchSend(x, y);
  else onTouchDecode(x, y);
}
