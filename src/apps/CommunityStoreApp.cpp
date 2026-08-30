#include "CommunityStoreApp.h"
#include "core/AppManager.h"

UI::Rect CommunityStoreApp::tileRect(uint8_t i) const {
  uint8_t col = i % COLS;
  uint8_t row = i / COLS;
  int16_t contentH = Cfg::SCREEN_H - Cfg::STATUS_BAR_H;
  int16_t tw = Cfg::SCREEN_W / COLS;
  int16_t th = contentH / ROWS;
  return {(int16_t)(col * tw), (int16_t)(Cfg::STATUS_BAR_H + row * th), tw, th};
}

int8_t CommunityStoreApp::tileAt(int16_t x, int16_t y) const {
  for (uint8_t i = 0; i < _tileCount; i++) {
    if (tileRect(i).contains(x, y)) return (int8_t)i;
  }
  return -1;
}

void CommunityStoreApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  UI::clearContent(tft);

  if (_tileCount == 0) {
    UI::centerText(tft, "No community apps installed", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 10, 1, Theme::MUTED);
    UI::centerText(tft, "coldzeeyt.github.io/cydos/store.html", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 10, 1, Theme::MUTED);
    return;
  }

  for (uint8_t i = 0; i < _tileCount; i++) {
    UI::Rect cell = tileRect(i);
    App* app = _mgr->appAt(_appIndex[i]);
    int16_t pad = 5;
    UI::Rect card{(int16_t)(cell.x + pad), (int16_t)(cell.y + pad), (int16_t)(cell.w - pad * 2), (int16_t)(cell.h - pad * 2)};
    tft.fillRoundRect(card.x, card.y, card.w, card.h, 8, Theme::PANEL);
    tft.drawRoundRect(card.x, card.y, card.w, card.h, 8, Theme::PANEL2);
    int16_t cx = card.x + card.w / 2;
    int16_t cy = card.y + card.h / 2 - 8;
    if (_icons[i]) _icons[i](tft, cx, cy, Theme::ACCENT);
    tft.setTextColor(Theme::TEXT, Theme::PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(app ? app->name() : "?", cx, card.y + card.h - 13, 1);
    tft.setTextDatum(TL_DATUM);
  }
}

void CommunityStoreApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;
  int8_t idx = tileAt(x, y);
  if (idx >= 0) _mgr->openApp(_appIndex[idx]);
}
