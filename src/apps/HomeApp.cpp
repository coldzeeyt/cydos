#include "HomeApp.h"
#include "core/AppManager.h"
#include "Config.h"

UI::Rect HomeApp::tileRectInPage(uint8_t i) const {
  uint8_t col = i % COLS;
  uint8_t row = i / COLS;
  int16_t dotsH = pageCount() > 1 ? 14 : 0;
  int16_t contentH = Cfg::SCREEN_H - Cfg::STATUS_BAR_H - dotsH;
  int16_t tw = Cfg::SCREEN_W / COLS;
  int16_t th = contentH / ROWS;
  return {(int16_t)(col * tw), (int16_t)(Cfg::STATUS_BAR_H + row * th), tw, th};
}

void HomeApp::drawPage(TFT_eSPI& tft, uint8_t page, int16_t xOffset) {
  uint16_t start = page * TILES_PER_PAGE;
  uint16_t end = start + TILES_PER_PAGE;
  if (end > _tileCount) end = _tileCount;

  for (uint16_t i = start; i < end; i++) {
    UI::Rect cell = tileRectInPage(i - start);
    cell.x += xOffset;
    App* app = _mgr->appAt(_appIndex[i]);

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

int8_t HomeApp::tileAt(uint8_t page, int16_t x, int16_t y) const {
  uint16_t start = page * TILES_PER_PAGE;
  uint16_t end = start + TILES_PER_PAGE;
  if (end > _tileCount) end = _tileCount;
  for (uint16_t i = start; i < end; i++) {
    if (tileRectInPage(i - start).contains(x, y)) return (int8_t)i;
  }
  return -1;
}

void HomeApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  UI::clearContent(tft);
  drawPage(tft, _page, _dragOffsetX);

  uint8_t pages = pageCount();
  if (pages > 1) {
    int16_t dotsY = Cfg::SCREEN_H - 8;
    int16_t totalW = pages * 12;
    int16_t startX = Cfg::SCREEN_W / 2 - totalW / 2 + 4;
    for (uint8_t p = 0; p < pages; p++) {
      int16_t cx = startX + p * 12;
      if (p == _page) tft.fillCircle(cx, dotsY, 3, Theme::ACCENT);
      else tft.drawCircle(cx, dotsY, 3, Theme::MUTED);
    }
  }
}

void HomeApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (down) {
    _dragStartX = x;
    _dragStartY = y;
    _dragOffsetX = 0;
    _dragging = true;
    _isSwipe = false;
    return;
  }
  if (!_dragging) return;

  int16_t dx = x - _dragStartX;
  if (abs(dx) > SWIPE_MOVE_THRESHOLD) _isSwipe = true;
  if (_isSwipe) {
    _dragOffsetX = dx;
    _dirty = true;
  }
}

void HomeApp::onTouchUp() {
  if (!_dragging) return;
  _dragging = false;

  if (_isSwipe) {
    uint8_t pages = pageCount();
    if (_dragOffsetX <= -SWIPE_PAGE_THRESHOLD && _page + 1 < pages) _page++;
    else if (_dragOffsetX >= SWIPE_PAGE_THRESHOLD && _page > 0) _page--;
    _dragOffsetX = 0;
    _dirty = true;
  } else {
    int8_t idx = tileAt(_page, _dragStartX, _dragStartY);
    if (idx >= 0) _mgr->openApp(_appIndex[idx]);
  }
}
