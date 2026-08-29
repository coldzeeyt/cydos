#include "PasswordGenApp.h"
#include <qrcode.h>

static const char* LOWER = "abcdefghijklmnopqrstuvwxyz";
static const char* UPPER = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char* DIGITS = "0123456789";
static const char* SYMBOLS = "!@#$%^&*-_=+";

void PasswordGenApp::onEnter(TFT_eSPI& tft) {
  _mode = SETTINGS;
  _dirty = true;
}

void PasswordGenApp::generate() {
  char pool[96];
  pool[0] = 0;
  strcat(pool, LOWER);
  if (_useUpper) strcat(pool, UPPER);
  if (_useDigits) strcat(pool, DIGITS);
  if (_useSymbols) strcat(pool, SYMBOLS);
  int poolLen = strlen(pool);

  for (int i = 0; i < _length; i++) {
    _password[i] = pool[random(poolLen)];
  }
  _password[_length] = 0;
}

void PasswordGenApp::drawSettings(TFT_eSPI& tft) {
  UI::clearContent(tft);

  char lenBuf[16];
  snprintf(lenBuf, sizeof(lenBuf), "Length: %d", _length);
  UI::centerText(tft, lenBuf, Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 22, 2, Theme::TEXT);
  _lenDown.draw(tft);
  _lenUp.draw(tft);

  _upperBtn.active = _useUpper;
  _digitBtn.active = _useDigits;
  _symBtn.active = _useSymbols;
  _upperBtn.draw(tft);
  _digitBtn.draw(tft);
  _symBtn.draw(tft);

  _genBtn.draw(tft);

  int16_t boxY = Cfg::STATUS_BAR_H + 122;
  tft.fillRoundRect(10, boxY, Cfg::SCREEN_W - 20, 34, 6, Theme::PANEL);
  tft.setTextColor(_password[0] ? Theme::ACCENT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(_password[0] ? _password : "tap Generate", Cfg::SCREEN_W / 2, boxY + 17, 2);
  tft.setTextDatum(TL_DATUM);

  if (_password[0]) _qrBtn.draw(tft);
}

void PasswordGenApp::drawQr(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _backBtn.draw(tft);

  QRCode qrcode;
  uint8_t buf[qrcode_getBufferSize(4)];
  qrcode_initText(&qrcode, buf, 4, ECC_LOW, _password);

  int16_t areaTop = Cfg::STATUS_BAR_H + 44;
  int16_t areaH = Cfg::SCREEN_H - areaTop - 6;
  int16_t areaW = Cfg::SCREEN_W - 12;
  int16_t px = min(areaW, areaH) / qrcode.size;
  if (px < 1) px = 1;
  int16_t total = px * qrcode.size;
  int16_t ox = (Cfg::SCREEN_W - total) / 2;
  int16_t oy = areaTop + (areaH - total) / 2;

  tft.fillRect(ox - 4, oy - 4, total + 8, total + 8, Theme::TEXT);
  for (uint8_t y = 0; y < qrcode.size; y++) {
    for (uint8_t x = 0; x < qrcode.size; x++) {
      if (qrcode_getModule(&qrcode, x, y)) tft.fillRect(ox + x * px, oy + y * px, px, px, Theme::BG);
    }
  }
}

void PasswordGenApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  if (_mode == SETTINGS) drawSettings(tft);
  else drawQr(tft);
}

void PasswordGenApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  if (_mode == QR) {
    if (_backBtn.hit(x, y)) { _mode = SETTINGS; _dirty = true; }
    return;
  }

  if (_lenDown.hit(x, y)) { _length = max(6, _length - 1); _dirty = true; }
  else if (_lenUp.hit(x, y)) { _length = min((int)MAX_LEN, _length + 1); _dirty = true; }
  else if (_upperBtn.hit(x, y)) { _useUpper = !_useUpper; _dirty = true; }
  else if (_digitBtn.hit(x, y)) { _useDigits = !_useDigits; _dirty = true; }
  else if (_symBtn.hit(x, y)) { _useSymbols = !_useSymbols; _dirty = true; }
  else if (_genBtn.hit(x, y)) { generate(); _dirty = true; }
  else if (_password[0] && _qrBtn.hit(x, y)) { _mode = QR; _dirty = true; }
}
