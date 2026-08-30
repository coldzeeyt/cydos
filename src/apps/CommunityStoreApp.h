#pragma once
#include "App.h"
#include "core/UI.h"

class AppManager;

// Groups every Community Edition app under a single Home tile instead of
// each one claiming its own top-level slot - a simple 3x3 grid (no
// pagination: scripts/generate_community.py caps community apps at 8,
// which fits on one page). Exists and shows an empty state in every
// build; only the Community Edition build actually has anything to put
// in it (see community_registration.inc, included from main.cpp).
class CommunityStoreApp : public App {
public:
  using IconFn = void (*)(TFT_eSPI&, int16_t, int16_t, uint16_t);

  explicit CommunityStoreApp(AppManager* mgr) : _mgr(mgr) {}

  const char* name() const override { return "App Store"; }

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
  static constexpr uint8_t MAX_TILES = 8; // matches MAX_COMMUNITY_APPS in scripts/generate_community.py
  static constexpr uint8_t COLS = 3;
  static constexpr uint8_t ROWS = 3;

  AppManager* _mgr;
  IconFn _icons[MAX_TILES];
  uint8_t _appIndex[MAX_TILES];
  uint8_t _tileCount = 0;
  bool _dirty = true;

  UI::Rect tileRect(uint8_t i) const;
  int8_t tileAt(int16_t x, int16_t y) const;
};
