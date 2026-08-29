#include "FlashlightApp.h"
#include "core/AppManager.h"
#include "core/Display.h"

void FlashlightApp::applyBacklight() {
  Display::setBrightnessPercent((uint8_t)_slider.value);
}

void FlashlightApp::onEnter(TFT_eSPI& tft) {
  _dirty = true;
  applyBacklight();
}

void FlashlightApp::onExit() {
  Display::setBrightnessPercent(_mgr->brightnessPercent());
}

void FlashlightApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;

  uint16_t fill = _red ? Theme::DANGER : Theme::TEXT;
  tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, fill);

  _toggle.label = _red ? "Switch: WHITE" : "Switch: RED";
  _toggle.draw(tft);
  _slider.draw(tft);

  tft.setTextColor(_red ? Theme::TEXT : Theme::BG, fill);
  tft.setTextDatum(MC_DATUM);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", _slider.value);
  tft.drawString(buf, Cfg::SCREEN_W / 2, _slider.r.y - 16, 2);
  tft.setTextDatum(TL_DATUM);
}

void FlashlightApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (down && _toggle.hit(x, y)) {
    _red = !_red;
    _dirty = true;
    return;
  }
  if (down && _slider.hit(x, y)) _draggingSlider = true;
  if (_draggingSlider) {
    _slider.updateFromTouch(x);
    applyBacklight();
    _dirty = true;
  }
}
