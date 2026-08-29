#pragma once
#include "App.h"
#include "core/UI.h"

class AppManager;

// The CydOs launcher: a grid of tiles, one per registered app (skipping
// itself at index 0). Tap a tile to open that app.
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

private:
  static constexpr uint8_t MAX_TILES = 8;
  static constexpr uint8_t COLS = 4;

  AppManager* _mgr;
  IconFn _icons[MAX_TILES];
  uint8_t _appIndex[MAX_TILES];
  uint8_t _tileCount = 0;
  bool _dirty = true;

  UI::Rect tileRect(uint8_t i) const;
};
