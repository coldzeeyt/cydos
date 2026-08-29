#include "HomeApp.h"
#include "core/AppManager.h"
#include "Config.h"

UI::Rect HomeApp::tileRect(uint8_t i) const {
  uint8_t col = i % COLS;
  uint8_t row = i / COLS;
  int16_t contentH = Cfg::SCREEN_H - Cfg::STATUS_BAR_H;
  int16_t rows = (_tileCount + COLS - 1) / COLS;
  int16_t tw = Cfg::SCREEN_W / COLS;
  int16_t th = contentH / rows;
  return {(int16_t)(col * tw), (int16_t)(Cfg::STATUS_BAR_H + row * th), tw, th};
}

void HomeApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  UI::clearContent(tft);

  for (uint8_t i = 0; i < _tileCount; i++) {
    UI::Rect r = tileRect(i);
    App* app = _mgr->appAt(_appIndex[i]);

    tft.drawRect(r.x, r.y, r.w, r.h, Theme::PANEL2);
    int16_t cx = r.x + r.w / 2;
    int16_t cy = r.y + r.h / 2 - 8;
    if (_icons[i]) _icons[i](tft, cx, cy, Theme::ACCENT);

    tft.setTextColor(Theme::TEXT, Theme::BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(app ? app->name() : "?", cx, r.y + r.h - 14, 2);
    tft.setTextDatum(TL_DATUM);
  }
}

void HomeApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;
  for (uint8_t i = 0; i < _tileCount; i++) {
    if (tileRect(i).contains(x, y)) {
      _mgr->openApp(_appIndex[i]);
      return;
    }
  }
}
