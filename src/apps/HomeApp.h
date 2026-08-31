#pragma once
#include "App.h"
#include "core/UI.h"

class AppManager;

// The CydOs launcher: a 3x3 grid of tiles per page, one tile per
// registered app (skipping itself at index 0). Tap a tile to open it;
// swipe left/right to move between pages when there's more than nine.
class HomeApp : public App {
public:
  using IconFn = void (*)(TFT_eSPI&, int16_t, int16_t, uint16_t);

  explicit HomeApp(AppManager* mgr) : _mgr(mgr) {}

  const char* name() const override { return "CydOs"; }

  void addTile(IconFn icon, uint8_t appIndex) {
    if (_tileCount < MAX_TILES) {
      _icons[_tileCount] = icon;
      _appIndex[_tileCount] = appIndex;
      _tileCount++;
    }
  }

  void onEnter(TFT_eSPI& tft) override { _dirty = true; }
  bool update() override { return false; }

  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;
  void onTouchUp() override;

private:
  static constexpr uint8_t MAX_TILES = 27; // three pages' worth, headroom to grow
  static constexpr uint8_t COLS = 3;
  static constexpr uint8_t ROWS = 3;
  static constexpr uint8_t TILES_PER_PAGE = COLS * ROWS;
  static constexpr int16_t SWIPE_MOVE_THRESHOLD = 10;  // px of finger movement before it counts as a drag, not a tap
  static constexpr int16_t SWIPE_PAGE_THRESHOLD = 50;  // px of drag needed to actually flip the page

  AppManager* _mgr;
  IconFn _icons[MAX_TILES];
  uint8_t _appIndex[MAX_TILES];
  uint8_t _tileCount = 0;
  bool _dirty = true;

  uint8_t _page = 0;
  int16_t _dragStartX = 0, _dragStartY = 0;
  int16_t _dragOffsetX = 0;
  bool _dragging = false;
  bool _isSwipe = false;
  uint32_t _lastDragRedraw = 0;

  uint8_t pageCount() const { return (_tileCount + TILES_PER_PAGE - 1) / TILES_PER_PAGE; }
  UI::Rect tileRectInPage(uint8_t indexInPage) const;
  void drawPage(TFT_eSPI& tft, uint8_t page, int16_t xOffset);
  int8_t tileAt(uint8_t page, int16_t x, int16_t y) const; // -1 if none

  // Explicit prev/next page buttons - some touch panels track a drag
  // badly enough that swiping never reliably crosses SWIPE_PAGE_THRESHOLD,
  // so this doesn't depend on a working swipe gesture at all. Only shown
  // (and only hit-tested) when there's more than one page, same condition
  // the page dots already use.
  UI::Rect prevPageBtnRect() const { return {0, (int16_t)(Cfg::SCREEN_H - 18), 34, 18}; }
  UI::Rect nextPageBtnRect() const { return {(int16_t)(Cfg::SCREEN_W - 34), (int16_t)(Cfg::SCREEN_H - 18), 34, 18}; }
};
