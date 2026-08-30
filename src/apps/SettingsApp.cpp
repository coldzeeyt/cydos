#include "SettingsApp.h"
#include "core/AppManager.h"
#include "core/Display.h"
#include "Version.h"
#include <WiFi.h>

void SettingsApp::onEnter(TFT_eSPI& tft) {
  _mode = MAIN;
  _brightSlider.value = _mgr->brightnessPercent();
  _dirty = true;
}

void SettingsApp::loadWifiCreds() {
  String ssid = _prefs->getString("wssid", Cfg::WIFI_SSID);
  String pass = _prefs->getString("wpass", Cfg::WIFI_PASSWORD);
  ssid.toCharArray(_ssid, sizeof(_ssid));
  pass.toCharArray(_pass, sizeof(_pass));
}

void SettingsApp::drawMain(TFT_eSPI& tft) {
  UI::clearContent(tft);
  UI::centerText(tft, "Brightness", Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 14, 1, Theme::MUTED);
  _brightSlider.draw(tft);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", _brightSlider.value);
  UI::centerText(tft, buf, Cfg::SCREEN_W / 2, 94, 1, Theme::TEXT);

  _touchTestBtn.draw(tft);
  _setTimeBtn.draw(tft);
  _wifiBtn.draw(tft);

  _battToggleBtn.label = _mgr->batteryVisible() ? "Battery Icon: ON" : "Battery Icon: OFF";
  _battToggleBtn.draw(tft);

  _lockToggleBtn.label = _mgr->lockScreenEnabled() ? "Lock Screen: ON" : "Lock Screen: OFF";
  _lockToggleBtn.draw(tft);

  char verBuf[24];
  snprintf(verBuf, sizeof(verBuf), "CydOs v%s", CYDOS_VERSION);
  UI::centerText(tft, verBuf, Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 10, 1, Theme::MUTED);
}

void SettingsApp::drawSetTime(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _backBtn.draw(tft);

  uint32_t s = _clock->secondsSinceMidnight();
  char buf[12];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", s / 3600, (s % 3600) / 60, s % 60);
  UI::centerText(tft, buf, Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 55, 7, Theme::ACCENT);

  _timeHourUp.draw(tft);
  _timeHourDn.draw(tft);
  _timeMinUp.draw(tft);
  _timeMinDn.draw(tft);
}

void SettingsApp::drawTouchTest(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _backBtn.draw(tft);
  UI::centerText(tft, "Drag your finger around", Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 50, 2, Theme::MUTED);
  if (_lastTouchX >= 0) {
    tft.drawFastHLine(0, _lastTouchY, Cfg::SCREEN_W, Theme::PANEL2);
    tft.drawFastVLine(_lastTouchX, Cfg::STATUS_BAR_H, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, Theme::PANEL2);
    tft.fillCircle(_lastTouchX, _lastTouchY, 6, Theme::ACCENT);

    char buf[48];
    snprintf(buf, sizeof(buf), "screen: %3d,%3d", _lastTouchX, _lastTouchY);
    UI::centerText(tft, buf, Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 40, 2, Theme::TEXT);
    if (_lastRawX >= 0) {
      snprintf(buf, sizeof(buf), "raw: %4d,%4d", _lastRawX, _lastRawY);
      UI::centerText(tft, buf, Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 20, 2, Theme::MUTED);
    }
  }
}

void SettingsApp::drawWifi(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _backBtn.draw(tft);
  UI::centerText(tft, "WiFi Setup (for update checks)", Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 20, 1, Theme::MUTED);

  UI::Rect ssidBox = wifiSsidBox();
  tft.fillRoundRect(ssidBox.x, ssidBox.y, ssidBox.w, ssidBox.h, 6, Theme::PANEL);
  tft.drawRoundRect(ssidBox.x, ssidBox.y, ssidBox.w, ssidBox.h, 6, Theme::MUTED);
  tft.setTextColor(_ssid[0] ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_ssid[0] ? _ssid : "SSID - tap to type", ssidBox.x + 10, ssidBox.y + ssidBox.h / 2 + 1, 2);
  tft.setTextDatum(TL_DATUM);

  UI::Rect passBox = wifiPassBox();
  tft.fillRoundRect(passBox.x, passBox.y, passBox.w, passBox.h, 6, Theme::PANEL);
  tft.drawRoundRect(passBox.x, passBox.y, passBox.w, passBox.h, 6, Theme::MUTED);
  size_t plen = strlen(_pass);
  char masked[PASS_MAX + 1];
  for (size_t i = 0; i < plen; i++) masked[i] = '*';
  masked[plen] = 0;
  tft.setTextColor(plen ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(plen ? masked : "Password - tap to type", passBox.x + 10, passBox.y + passBox.h / 2 + 1, 2);
  tft.setTextDatum(TL_DATUM);

  _wifiSaveBtn.draw(tft);
  _wifiTestBtn.draw(tft);

  char status[56];
  snprintf(status, sizeof(status), "link: %s   last check: %s",
           WiFi.status() == WL_CONNECTED ? "connected" : "idle",
           _updates ? _updates->lastResult() : "n/a");
  UI::centerText(tft, status, Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 16, 1, Theme::MUTED);
}

UI::Rect SettingsApp::kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 24;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

void SettingsApp::drawWifiKeyboard(TFT_eSPI& tft) {
  UI::clearContent(tft);

  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, Cfg::SCREEN_W - 8, 24, 4, Theme::PANEL);
  char* buf = activeBuf();
  size_t n = strlen(buf);
  char shown[36];
  if (_activeField == FIELD_PASS) {
    size_t m = n < sizeof(shown) - 1 ? n : sizeof(shown) - 1;
    for (size_t i = 0; i < m; i++) shown[i] = '*';
    shown[m] = 0;
  } else {
    strncpy(shown, buf, sizeof(shown) - 1);
    shown[sizeof(shown) - 1] = 0;
  }
  tft.setTextColor(n ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(n ? shown : (_activeField == FIELD_SSID ? "type your network's SSID" : "type your network's password"),
                  10, Cfg::STATUS_BAR_H + 14, 2);
  tft.setTextDatum(TL_DATUM);

  for (uint8_t r = 0; r < KB_ROWS; r++) {
    uint8_t len = strlen(_kbRows[r]);
    for (uint8_t c = 0; c < len; c++) {
      UI::Rect kr = kbKeyRect(r, c, len);
      char label[2] = {_kbRows[r][c], 0};
      tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 4, Theme::PANEL);
      tft.setTextColor(Theme::TEXT, Theme::PANEL);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  uint8_t slen = strlen(_kbSymbols);
  for (uint8_t c = 0; c < slen; c++) {
    UI::Rect kr = kbKeyRect(KB_ROWS, c, slen);
    char label[2] = {_kbSymbols[c], 0};
    tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 4, Theme::PANEL2);
    tft.setTextColor(Theme::ACCENT, Theme::PANEL2);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
    tft.setTextDatum(TL_DATUM);
  }

  int16_t ctrlY = Cfg::STATUS_BAR_H + 30 + (KB_ROWS + 1) * 24 + 4;
  _kbSpaceBtn.r.y = ctrlY;
  _kbDelBtn.r.y = ctrlY;
  _kbClrBtn.r.y = ctrlY;
  _kbSpaceBtn.draw(tft);
  _kbDelBtn.draw(tft);
  _kbClrBtn.draw(tft);

  _kbDoneBtn.r.y = ctrlY + 30;
  _kbDoneBtn.draw(tft);
}

void SettingsApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  switch (_mode) {
    case MAIN: drawMain(tft); break;
    case TOUCH_TEST: drawTouchTest(tft); break;
    case WIFI: drawWifi(tft); break;
    case WIFI_KEYBOARD: drawWifiKeyboard(tft); break;
    case SET_TIME: drawSetTime(tft); break;
  }
}

void SettingsApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (_mode == TOUCH_TEST) {
    if (down && _backBtn.hit(x, y)) {
      _mode = MAIN;
      _dirty = true;
      return;
    }
    _lastTouchX = x;
    _lastTouchY = y;
    int16_t rx, ry;
    if (_touch->readRaw(rx, ry)) { _lastRawX = rx; _lastRawY = ry; }
    _dirty = true;
    return;
  }

  if (_mode == SET_TIME) {
    if (!down) return;
    if (_backBtn.hit(x, y)) { _mode = MAIN; _dirty = true; return; }
    if (_timeHourUp.hit(x, y)) _clock->addHours(1);
    else if (_timeHourDn.hit(x, y)) _clock->addHours(-1);
    else if (_timeMinUp.hit(x, y)) _clock->addMinutes(1);
    else if (_timeMinDn.hit(x, y)) _clock->addMinutes(-1);
    else return;
    _dirty = true;
    return;
  }

  if (_mode == WIFI) {
    if (!down) return;
    if (_backBtn.hit(x, y)) { _mode = MAIN; _dirty = true; return; }
    if (wifiSsidBox().contains(x, y)) { _activeField = FIELD_SSID; _mode = WIFI_KEYBOARD; _dirty = true; return; }
    if (wifiPassBox().contains(x, y)) { _activeField = FIELD_PASS; _mode = WIFI_KEYBOARD; _dirty = true; return; }
    if (_wifiSaveBtn.hit(x, y)) {
      _prefs->putString("wssid", _ssid);
      _prefs->putString("wpass", _pass);
      _dirty = true;
      return;
    }
    if (_wifiTestBtn.hit(x, y)) {
      if (_updates) _updates->checkNow();
      _dirty = true;
      return;
    }
    return;
  }

  if (_mode == WIFI_KEYBOARD) {
    if (!down) return;
    char* buf = activeBuf();
    size_t max = activeMax();
    size_t len = strlen(buf);

    for (uint8_t r = 0; r < KB_ROWS; r++) {
      uint8_t rlen = strlen(_kbRows[r]);
      for (uint8_t c = 0; c < rlen; c++) {
        if (kbKeyRect(r, c, rlen).contains(x, y) && len < max) {
          buf[len] = _kbRows[r][c];
          buf[len + 1] = 0;
          _dirty = true;
          return;
        }
      }
    }
    uint8_t slen = strlen(_kbSymbols);
    for (uint8_t c = 0; c < slen; c++) {
      if (kbKeyRect(KB_ROWS, c, slen).contains(x, y) && len < max) {
        buf[len] = _kbSymbols[c];
        buf[len + 1] = 0;
        _dirty = true;
        return;
      }
    }

    if (_kbSpaceBtn.hit(x, y) && len < max) { buf[len] = ' '; buf[len + 1] = 0; _dirty = true; }
    else if (_kbDelBtn.hit(x, y) && len > 0) { buf[len - 1] = 0; _dirty = true; }
    else if (_kbClrBtn.hit(x, y)) { buf[0] = 0; _dirty = true; }
    else if (_kbDoneBtn.hit(x, y)) { _mode = WIFI; _dirty = true; }
    return;
  }

  // MAIN
  if (down && _touchTestBtn.hit(x, y)) {
    _mode = TOUCH_TEST;
    _lastTouchX = _lastTouchY = -1;
    _dirty = true;
    return;
  }
  if (down && _setTimeBtn.hit(x, y)) {
    _mode = SET_TIME;
    _dirty = true;
    return;
  }
  if (down && _wifiBtn.hit(x, y)) {
    loadWifiCreds();
    _mode = WIFI;
    _dirty = true;
    return;
  }
  if (down && _battToggleBtn.hit(x, y)) {
    bool visible = !_mgr->batteryVisible();
    _mgr->setBatteryVisible(visible);
    _prefs->putBool("battshow", visible);
    _dirty = true;
    return;
  }
  if (down && _lockToggleBtn.hit(x, y)) {
    // Takes effect on the next boot (beginLocked() in main.cpp reads this
    // pref at startup) - flipping it here can't lock you out of Settings
    // mid-session.
    bool enabled = !_mgr->lockScreenEnabled();
    _mgr->setLockScreenEnabled(enabled);
    _prefs->putBool("lockscreen", enabled);
    _dirty = true;
    return;
  }
  if (down && _brightSlider.hit(x, y)) _draggingSlider = true;
  if (_draggingSlider) {
    _brightSlider.updateFromTouch(x);
    _mgr->setBrightnessPercent((uint8_t)_brightSlider.value);
    Display::setBrightnessPercent((uint8_t)_brightSlider.value);
    _dirty = true;
  }
}
