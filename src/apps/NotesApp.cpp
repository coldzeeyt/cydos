#include "NotesApp.h"

// Greedy word-wrap using the font's real rendered width (tft.textWidth)
// instead of a guessed fixed character count, so it wraps correctly at any
// font. Falls back to a hard break mid-word only if a single word alone is
// wider than maxWidth.
static uint8_t wrapText(TFT_eSPI& tft, const char* text, uint8_t font, int16_t maxWidth, char lines[][41], uint8_t maxLines) {
  uint8_t lineCount = 0;
  const char* p = text;
  while (*p && lineCount < maxLines) {
    char probe[41] = {0};
    size_t fit = 0;       // chars confirmed to fit on this line
    size_t lastSpace = 0; // chars to take if we break at the last space (0 = no space seen)
    while (p[fit] && fit < 40) {
      probe[fit] = p[fit];
      probe[fit + 1] = 0;
      if (fit > 0 && (int16_t)tft.textWidth(probe, font) > maxWidth) break;
      if (p[fit] == ' ') lastSpace = fit + 1;
      fit++;
    }
    size_t breakAt = fit;
    if (p[fit] != 0 && lastSpace > 0) breakAt = lastSpace;
    if (breakAt == 0) breakAt = 1; // pathological: force progress
    strncpy(lines[lineCount], p, breakAt);
    lines[lineCount][breakAt] = 0;
    lineCount++;
    p += breakAt;
    while (*p == ' ') p++;
  }
  return lineCount;
}

UI::Rect NotesApp::kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 24;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

void NotesApp::onEnter(TFT_eSPI& tft) {
  String saved = _prefs->getString("note", "");
  saved.toCharArray(_text, sizeof(_text));
  _mode = VIEW;
  _dirty = true;
}

void NotesApp::drawView(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _editBtn.draw(tft);

  if (_text[0] == 0) {
    UI::centerText(tft, "No note yet - tap Edit", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 1, Theme::MUTED);
    return;
  }

  static char lines[20][41];
  uint8_t lineCount = wrapText(tft, _text, 1, Cfg::SCREEN_W - 16, lines, 20);
  tft.setTextColor(Theme::TEXT, Theme::BG);
  tft.setTextDatum(TL_DATUM);
  int16_t y = Cfg::STATUS_BAR_H + 32;
  for (uint8_t i = 0; i < lineCount && y < Cfg::SCREEN_H - 12; i++) {
    tft.drawString(lines[i], 8, y, 1);
    y += 14;
  }
}

void NotesApp::drawEdit(TFT_eSPI& tft) {
  UI::clearContent(tft);

  int16_t fieldW = Cfg::SCREEN_W - 8;
  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, fieldW, 24, 4, Theme::PANEL);
  tft.setTextColor(_text[0] ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  size_t len = strlen(_text);
  const char* shown = len > 40 ? _text + (len - 40) : _text; // show the tail while typing
  tft.drawString(_text[0] ? shown : "type your note...", 10, Cfg::STATUS_BAR_H + 14, 1);
  tft.setTextDatum(TL_DATUM);

  for (uint8_t r = 0; r < KB_ROWS; r++) {
    uint8_t len2 = strlen(_kbRows[r]);
    for (uint8_t c = 0; c < len2; c++) {
      UI::Rect kr = kbKeyRect(r, c, len2);
      char label[2] = {_kbRows[r][c], 0};
      tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 4, Theme::PANEL);
      tft.setTextColor(Theme::TEXT, Theme::PANEL);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  int16_t ctrlY = Cfg::STATUS_BAR_H + 30 + KB_ROWS * 24 + 4;
  _kbSpaceBtn.r.y = ctrlY;
  _kbDelBtn.r.y = ctrlY;
  _kbClrBtn.r.y = ctrlY;
  _kbSpaceBtn.draw(tft);
  _kbDelBtn.draw(tft);
  _kbClrBtn.draw(tft);

  _kbDoneBtn.r.y = ctrlY + 30;
  _kbDoneBtn.draw(tft);
}

void NotesApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  if (_mode == VIEW) drawView(tft);
  else drawEdit(tft);
}

void NotesApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  if (_mode == VIEW) {
    if (_editBtn.hit(x, y)) { _mode = EDIT; _dirty = true; }
    return;
  }

  // EDIT
  size_t len = strlen(_text);
  for (uint8_t r = 0; r < KB_ROWS; r++) {
    uint8_t rowLen = strlen(_kbRows[r]);
    for (uint8_t c = 0; c < rowLen; c++) {
      if (kbKeyRect(r, c, rowLen).contains(x, y) && len < MAX_LEN) {
        _text[len++] = _kbRows[r][c];
        _text[len] = 0;
        _dirty = true;
        return;
      }
    }
  }
  if (_kbSpaceBtn.hit(x, y) && len < MAX_LEN) { _text[len++] = ' '; _text[len] = 0; _dirty = true; }
  else if (_kbDelBtn.hit(x, y) && len > 0) { _text[len - 1] = 0; _dirty = true; }
  else if (_kbClrBtn.hit(x, y)) { _text[0] = 0; _dirty = true; }
  else if (_kbDoneBtn.hit(x, y)) {
    _prefs->putString("note", _text);
    _mode = VIEW;
    _dirty = true;
  }
}
