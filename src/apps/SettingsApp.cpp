#include "SettingsApp.h"
#include "core/AppManager.h"
#include "core/Display.h"

void SettingsApp::onEnter(TFT_eSPI& tft) {
  _mode = MAIN;
  _brightSlider.value = _mgr->brightnessPercent();
  _dirty = true;
}

void SettingsApp::drawMain(TFT_eSPI& tft) {
  UI::clearContent(tft);
  UI::centerText(tft, "Brightness", Cfg::SCREEN_W / 2, 50, 2, Theme::MUTED);
  _brightSlider.draw(tft);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", _brightSlider.value);
  UI::centerText(tft, buf, Cfg::SCREEN_W / 2, 110, 2, Theme::TEXT);

  _touchTestBtn.draw(tft);

  UI::centerText(tft, "CydOs v1.0", Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 30, 2, Theme::MUTED);
  UI::centerText(tft, "a tiny OS for the Cheap Yellow Display", Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 14, 1, Theme::MUTED);
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

void SettingsApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  if (_mode == MAIN) drawMain(tft);
  else drawTouchTest(tft);
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

  if (down && _touchTestBtn.hit(x, y)) {
    _mode = TOUCH_TEST;
    _lastTouchX = _lastTouchY = -1;
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
