#include "FlashlightApp.h"
#include "core/AppManager.h"
#include "core/Display.h"

// White, Red (preserves night vision), Amber (softer than white), Cyan
// and Green (signaling / matching a colored chart) - all pulled straight
// from Theme rather than inventing new colors.
const uint16_t FlashlightApp::COLOR_VALUES[FlashlightApp::NUM_COLORS] = {
    Theme::TEXT, Theme::DANGER, Theme::ACCENT2, Theme::ACCENT, Theme::GOOD,
};
const char* const FlashlightApp::COLOR_NAMES[FlashlightApp::NUM_COLORS] = {
    "White", "Red", "Amber", "Cyan", "Green",
};
// Contrasting text/border color to draw *on* each swatch above - white is
// the only one dark text would disappear on.
static const uint16_t TEXT_ON[FlashlightApp::NUM_COLORS] = {
    Theme::BG, Theme::TEXT, Theme::BG, Theme::BG, Theme::BG,
};

UI::Rect FlashlightApp::swatchRect(uint8_t i) const {
  const int16_t w = 50, gap = 8;
  const int16_t totalW = NUM_COLORS * w + (NUM_COLORS - 1) * gap;
  const int16_t x0 = (Cfg::SCREEN_W - totalW) / 2;
  return {(int16_t)(x0 + i * (w + gap)), (int16_t)(Cfg::STATUS_BAR_H + 52), w, 40};
}

void FlashlightApp::applyBacklight() {
  Display::setBrightnessPercent(_asleep ? 0 : (uint8_t)_slider.value);
}

void FlashlightApp::onEnter(TFT_eSPI& tft) {
  _dirty = true;
  _asleep = false;
  applyBacklight();
}

void FlashlightApp::onExit() {
  Display::setBrightnessPercent(_mgr->brightnessPercent());
}

void FlashlightApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;

  if (_asleep) {
    tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, TFT_BLACK);
    return;
  }

  uint16_t fill = COLOR_VALUES[_colorIdx];
  uint16_t textOn = TEXT_ON[_colorIdx];
  tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, fill);

  _sleepBtn.draw(tft);

  for (uint8_t i = 0; i < NUM_COLORS; i++) {
    UI::Rect r = swatchRect(i);
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, COLOR_VALUES[i]);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, i == _colorIdx ? Theme::PANEL2 : TEXT_ON[i]);
    if (i == _colorIdx) tft.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 5, TEXT_ON[i]);
  }

  tft.setTextColor(textOn, fill);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(COLOR_NAMES[_colorIdx], Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 52 + 40 + 16, 2);
  tft.setTextDatum(TL_DATUM);

  _slider.draw(tft);
  tft.setTextColor(textOn, fill);
  tft.setTextDatum(MC_DATUM);
  char buf[16];
  snprintf(buf, sizeof(buf), "%d%%", _slider.value);
  tft.drawString(buf, Cfg::SCREEN_W / 2, _slider.r.y - 16, 2);
  tft.setTextDatum(TL_DATUM);
}

void FlashlightApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  // Asleep swallows the waking tap rather than also acting on whatever's
  // underneath it - the whole point is "any tap just wakes it up".
  if (_asleep) {
    _asleep = false;
    applyBacklight();
    _dirty = true;
    return;
  }

  if (_sleepBtn.hit(x, y)) {
    _asleep = true;
    applyBacklight();
    _dirty = true;
    return;
  }

  for (uint8_t i = 0; i < NUM_COLORS; i++) {
    if (swatchRect(i).contains(x, y)) {
      _colorIdx = i;
      _dirty = true;
      return;
    }
  }

  if (_slider.hit(x, y)) _draggingSlider = true;
  if (_draggingSlider) {
    _slider.updateFromTouch(x);
    applyBacklight();
    _dirty = true;
  }
}
