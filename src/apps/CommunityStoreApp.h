#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"

class AppManager;
class SdCard;
class SdAppPool;

// The on-device App Store, two tabs:
//  - Installed: every Community Edition app baked into this firmware
//    build, one shared 3x3 grid instead of each claiming its own
//    top-level Home tile (see scripts/generate_community.py).
//  - Get More: fetches the tiny catalogs
//    scripts/generate_ondevice_catalog.py publishes over WiFi and lets
//    you download an SD Card App or a wallpaper straight onto the SD
//    card - no reflash, and a downloaded app becomes a real Home tile
//    immediately (see SdAppPool::rescan()). Community Edition apps can't
//    work this way - they're compiled into the firmware image itself,
//    and this hardware has no way to load compiled code at runtime.
class CommunityStoreApp : public App {
public:
  using IconFn = void (*)(TFT_eSPI&, int16_t, int16_t, uint16_t);

  CommunityStoreApp(AppManager* mgr, Preferences* prefs, SdCard* sd, SdAppPool* sdAppPool)
      : _mgr(mgr), _prefs(prefs), _sd(sd), _sdAppPool(sdAppPool) {}

  const char* name() const override { return "App Store"; }

  void addTile(IconFn icon, uint8_t appIndex) {
    if (_tileCount < MAX_TILES) {
      _icons[_tileCount] = icon;
      _appIndex[_tileCount] = appIndex;
      _tileCount++;
    }
  }

  void onEnter(TFT_eSPI& tft) override;
  void onExit() override;
  bool update() override { return false; }
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  static constexpr uint8_t MAX_TILES = 8; // matches MAX_COMMUNITY_APPS in scripts/generate_community.py
  static constexpr uint8_t COLS = 3;
  static constexpr uint8_t ROWS = 3;

  enum Mode { INSTALLED, BROWSE };
  enum BrowseState { NOT_LOADED, FAILED, LOADED };

  AppManager* _mgr;
  Preferences* _prefs;
  SdCard* _sd;
  SdAppPool* _sdAppPool;

  IconFn _icons[MAX_TILES];
  uint8_t _appIndex[MAX_TILES];
  uint8_t _tileCount = 0;

  Mode _mode = INSTALLED;
  BrowseState _browseState = NOT_LOADED;
  bool _dirty = true;

  // Was 12 - too easy to blow through once apps and wallpapers add up
  // (they share this one cap), silently dropping later submissions from
  // Get More with no sign anything was cut. 60 costs under 5KB of RAM
  // (trivial against 327KB total) and leaves real headroom to grow.
  static constexpr uint8_t MAX_BROWSE_ITEMS = 60;
  static constexpr uint8_t NAME_LEN = 32;
  static constexpr uint8_t PATH_LEN = 48;
  struct BrowseItem {
    char name[NAME_LEN];
    char urlPath[PATH_LEN];
    bool isApp;
    bool installed;
  };
  BrowseItem _items[MAX_BROWSE_ITEMS];
  uint8_t _itemCount = 0;

  // The rows themselves only ever fit ~7 on screen at once - paginated
  // the same way Files browses SD card directories (see
  // FileManagerApp::ROWS_PER_PAGE) instead of just silently dropping
  // whatever didn't fit below the fold.
  static constexpr uint8_t BROWSE_ROWS_PER_PAGE = 6;
  uint8_t _browsePage = 0;
  UI::Button _browsePrevBtn{{40, Cfg::STATUS_BAR_H + 184, 60, 22}, "<"};
  UI::Button _browseNextBtn{{(int16_t)(Cfg::SCREEN_W - 100), Cfg::STATUS_BAR_H + 184, 60, 22}, ">"};

  UI::Rect tileRect(uint8_t i) const;
  int8_t tileAt(int16_t x, int16_t y) const;
  UI::Rect tabRect(uint8_t i) const;
  UI::Rect browseRowRect(uint8_t rowInPage) const;
  uint8_t browsePageCount() const;

  bool connectWifi();
  void loadBrowseCatalog(TFT_eSPI& tft);
  void downloadItem(TFT_eSPI& tft, uint8_t idx);

  void drawTabBar(TFT_eSPI& tft);
  void drawInstalled(TFT_eSPI& tft);
  void drawBrowse(TFT_eSPI& tft);
};
