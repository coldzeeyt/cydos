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
    UI::Rect cell = tileRect(i);
    App* app = _mgr->appAt(_appIndex[i]);

    // A little breathing room between cells reads as a grid of app icons
    // rather than a spreadsheet.
    int16_t pad = 4;
    UI::Rect card{(int16_t)(cell.x + pad), (int16_t)(cell.y + pad), (int16_t)(cell.w - pad * 2), (int16_t)(cell.h - pad * 2)};
    tft.fillRoundRect(card.x, card.y, card.w, card.h, 8, Theme::PANEL);

    int16_t cx = card.x + card.w / 2;
    int16_t cy = card.y + card.h / 2 - 9;
    tft.fillCircle(cx, cy, 17, Theme::PANEL2);
    if (_icons[i]) _icons[i](tft, cx, cy, Theme::ACCENT);

    tft.setTextColor(Theme::TEXT, Theme::PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(app ? app->name() : "?", cx, card.y + card.h - 13, 1);
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
