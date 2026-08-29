#include "QrBeamerApp.h"
#include <qrcode.h>

static const int QR_VERSION_CAPACITY[] = {17, 32, 53, 78, 106, 134, 154, 192, 230, 271};
static const uint8_t QR_MAX_VERSION = 10;

void QrBeamerApp::onEnter(TFT_eSPI& tft) {
  _mode = EDIT;
  _dirty = true;
}

UI::Rect QrBeamerApp::keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 26;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

void QrBeamerApp::drawKeyboard(TFT_eSPI& tft) {
  UI::clearContent(tft);

  // Text preview line.
  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, Cfg::SCREEN_W - 8, 24, 4, Theme::PANEL);
  const char* shown = _text;
  if (_len > 34) shown = _text + (_len - 34);  // show the tail as it fills up
  tft.setTextColor(_len ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_len ? shown : "type a message...", 10, Cfg::STATUS_BAR_H + 14, 2);
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
  _spaceBtn.draw(tft);
  _delBtn.draw(tft);
  _clrBtn.draw(tft);

  _genBtn.r.y = ctrlY + 30;
  _genBtn.color = _len ? Theme::PANEL : Theme::PANEL2;
  _genBtn.textColor = _len ? Theme::ACCENT : Theme::MUTED;
  _genBtn.draw(tft);
}

void QrBeamerApp::drawQr(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _backBtn.draw(tft);

  uint8_t version = QR_MAX_VERSION;
  for (uint8_t v = 1; v <= QR_MAX_VERSION; v++) {
    if ((int)_len <= QR_VERSION_CAPACITY[v - 1]) { version = v; break; }
  }

  QRCode qrcode;
  uint8_t buf[qrcode_getBufferSize(QR_MAX_VERSION)];
  qrcode_initText(&qrcode, buf, version, ECC_LOW, _text);

  int16_t areaTop = Cfg::STATUS_BAR_H + 44;
  int16_t areaH = Cfg::SCREEN_H - areaTop - 6;
  int16_t areaW = Cfg::SCREEN_W - 12;
  int16_t px = min(areaW, areaH) / qrcode.size;
  if (px < 1) px = 1;
  int16_t totalPx = px * qrcode.size;
  int16_t ox = (Cfg::SCREEN_W - totalPx) / 2;
  int16_t oy = areaTop + (areaH - totalPx) / 2;

  tft.fillRect(ox - 4, oy - 4, totalPx + 8, totalPx + 8, Theme::TEXT);
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) {
        tft.fillRect(ox + x * px, oy + y * px, px, px, Theme::BG);
      }
    }
  }
}

void QrBeamerApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  if (_mode == EDIT) drawKeyboard(tft);
  else drawQr(tft);
}

void QrBeamerApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  if (_mode == SHOW) {
    if (_backBtn.hit(x, y)) { _mode = EDIT; _dirty = true; }
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

  if (_spaceBtn.hit(x, y) && _len < MAX_LEN) {
    _text[_len++] = ' ';
    _text[_len] = 0;
    _dirty = true;
  } else if (_delBtn.hit(x, y) && _len > 0) {
    _text[--_len] = 0;
    _dirty = true;
  } else if (_clrBtn.hit(x, y)) {
    _len = 0;
    _text[0] = 0;
    _dirty = true;
  } else if (_genBtn.hit(x, y) && _len > 0) {
    _mode = SHOW;
    _dirty = true;
  }
}
