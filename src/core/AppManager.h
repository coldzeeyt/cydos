#pragma once
#include <TFT_eSPI.h>
#include "Config.h"
#include "UI.h"
#include "Battery.h"
#include "UpdateChecker.h"
#include "WallClock.h"
#include "Wallpaper.h"
#include "apps/App.h"

// Owns the status bar (title, battery, back-to-home) and switches the
// active App. Index 0 is always the Home launcher.
class AppManager {
public:
  // 15 built-in apps + up to 8 Community Edition apps (see
  // scripts/generate_community.py, which enforces its own smaller cap so a
  // bad community build fails loudly in CI well before this limit matters)
  // + up to 6 SD Card Apps - rounded up with headroom rather than the
  // exact worst-case sum, same as MAX_TILES in HomeApp.h.
  static constexpr uint8_t MAX_APPS = 32;

  void begin(TFT_eSPI& tft, Battery* battery, UpdateChecker* updates = nullptr) {
    _tft = &tft;
    _battery = battery;
    _updates = updates;
  }

  // Returns 0xFF (and drops the app) if MAX_APPS is already full, rather
  // than writing past the end of _apps - matters now that the app list
  // isn't a fixed, known-at-a-glance count (community builds add to it).
  uint8_t registerApp(App* app) {
    if (_count >= MAX_APPS) return 0xFF;
    _apps[_count] = app;
    return _count++;
  }

  uint8_t appCount() const { return _count; }
  App* appAt(uint8_t i) const { return _apps[i]; }

  void openApp(uint8_t index) {
    if (index >= _count) return;
    if (_current) _current->onExit();
    _currentIndex = index;
    _current = _apps[index];
    UI::clearContent(*_tft);
    _current->onEnter(*_tft);
    drawStatusBar();
    _current->draw(*_tft);
  }

  void goHome() { openApp(0); }
  bool atHome() const { return _currentIndex == 0; }
  App* currentApp() const { return _current; }

  void setBrightnessPercent(uint8_t pct) { _brightnessPct = pct; }
  uint8_t brightnessPercent() const { return _brightnessPct; }

  void setBatteryVisible(bool visible) { _showBattery = visible; }
  bool batteryVisible() const { return _showBattery; }

  // Whether Lock Screen is turned on in Settings - persisted by the
  // caller (Preferences), just read/stored here. Changing this takes
  // effect on the next boot (beginLocked() is what actually locks),
  // not mid-session, so toggling it in Settings can't lock you out of
  // Settings itself.
  void setLockScreenEnabled(bool on) { _lockEnabled = on; }
  bool lockScreenEnabled() const { return _lockEnabled; }
  bool isLocked() const { return _locked; }

  // Call once from setup(), after all registerApp()/addTile() calls,
  // instead of openApp(0), when Settings > Lock Screen is on. `clock` is
  // optional - pass nullptr to show the lock screen without a time.
  void beginLocked(WallClock* clock = nullptr) {
    _lockClock = clock;
    _locked = true;
    _lastTapAt = 0;
    drawLockScreen();
  }

  void loop(int16_t touchX, int16_t touchY, bool touchDown, bool touchHeld) {
    if (_locked) { loopLocked(touchDown); return; }

    bool needsRedraw = false;
    if (_current) needsRedraw = _current->update();

    // "New Update!" badge - lives entirely inside the status bar, so
    // dismissing it never has to touch (or fight with) whatever the
    // current app has drawn in the content area below.
    if (touchDown && _updates && _updates->shouldShowBanner() && updateBadgeRect().contains(touchX, touchY)) {
      _updates->dismiss();
      drawStatusBar();
      return;
    }

    // Status bar back button (only shown away from Home).
    if (touchDown && !atHome() && backButtonRect().contains(touchX, touchY)) {
      goHome();
      return;
    }

    if ((touchDown || touchHeld) && _current) {
      _current->onTouch(*_tft, touchX, touchY, touchDown);
      needsRedraw = true;
    }
    if (!touchHeld && !touchDown && _lastHeld && _current) {
      _current->onTouchUp();
    }
    _lastHeld = touchHeld;

    if (needsRedraw && _current) {
      _current->draw(*_tft);
    }

    // Refresh the battery pill on a timer, independent of whether the
    // current app redrew - otherwise it freezes at whatever it read when
    // the app opened for as long as you leave an idle screen (e.g. Home)
    // on screen without touching it.
    uint32_t now = millis();
    if (now - _lastBarRefresh > 1000) {
      drawStatusBar();
      _lastBarRefresh = now;
    }
  }

  UI::Rect backButtonRect() const { return {2, 2, 22, Cfg::STATUS_BAR_H - 4}; }
  UI::Rect updateBadgeRect() const {
    int16_t w = 74, h = 18;
    int16_t x = Cfg::SCREEN_W - 30 - 10 - 6 - w; // left of the battery pill
    return {x, (int16_t)((Cfg::STATUS_BAR_H - h) / 2), w, h};
  }

  void drawStatusBar() {
    TFT_eSPI& tft = *_tft;
    tft.fillRect(0, 0, Cfg::SCREEN_W, Cfg::STATUS_BAR_H, Theme::PANEL);
    tft.drawFastHLine(0, Cfg::STATUS_BAR_H - 1, Cfg::SCREEN_W, Theme::MUTED);

    int16_t titleX = 8;
    if (!atHome()) {
      UI::Rect b = backButtonRect();
      tft.fillRoundRect(b.x, b.y, b.w, b.h, 4, Theme::PANEL2);
      tft.setTextColor(Theme::ACCENT, Theme::PANEL2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("<", b.x + b.w / 2, b.y + b.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
      titleX = b.x + b.w + 6;
    }

    tft.setTextColor(Theme::TEXT, Theme::PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(_current ? _current->name() : "CydOs", titleX, Cfg::STATUS_BAR_H / 2 + 1, 2);
    tft.setTextDatum(TL_DATUM);

    // "New Update!" badge, if the update checker found a newer version.
    if (_updates && _updates->shouldShowBanner()) {
      UI::Rect b = updateBadgeRect();
      tft.fillRoundRect(b.x, b.y, b.w, b.h, 4, Theme::ACCENT2);
      char label[20];
      snprintf(label, sizeof(label), "NEW: v%s", _updates->latestVersion());
      tft.setTextColor(Theme::BG, Theme::ACCENT2);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, b.x + b.w / 2, b.y + b.h / 2 + 1, 1);
      tft.setTextDatum(TL_DATUM);
    }

    // Battery pill, top-right.
    if (_battery && _battery->available() && _showBattery) {
      uint8_t pct = _battery->percent();
      int16_t bw = 30, bh = 14;
      int16_t bx = Cfg::SCREEN_W - bw - 10, by = (Cfg::STATUS_BAR_H - bh) / 2;
      uint16_t col = pct < 20 ? Theme::DANGER : (pct < 50 ? Theme::ACCENT2 : Theme::GOOD);
      tft.drawRoundRect(bx, by, bw, bh, 2, Theme::MUTED);
      tft.fillRect(bx + bw, by + 3, 2, bh - 6, Theme::MUTED);
      int16_t fillW = map(pct, 0, 100, 0, bw - 4);
      tft.fillRect(bx + 2, by + 2, bw - 4, bh - 4, Theme::PANEL);
      if (fillW > 0) tft.fillRect(bx + 2, by + 2, fillW, bh - 4, col);
    }
  }

private:
  // Two taps within this window (of each other, not of any fixed clock)
  // unlock; a lone tap just resets the window, same as a real lock screen.
  static constexpr uint32_t DOUBLE_TAP_WINDOW_MS = 600;

  void loopLocked(bool touchDown) {
    if (touchDown) {
      uint32_t now = millis();
      if (_lastTapAt != 0 && now - _lastTapAt < DOUBLE_TAP_WINDOW_MS) {
        _locked = false;
        goHome();
        return;
      }
      _lastTapAt = now;
    }
    uint32_t now = millis();
    if (now - _lastBarRefresh > 1000) {
      drawLockScreen(); // keeps the optional clock ticking while idle
      _lastBarRefresh = now;
    }
  }

  void drawLockScreen() {
    TFT_eSPI& tft = *_tft;
    tft.fillScreen(Theme::BG);
    // Wallpaper::draw() only paints y >= STATUS_BAR_H (it's sized for the
    // content area below Home's status bar) - the top strip stays flat
    // Theme::BG, which reads fine as a thin top margin on a lock screen
    // that has no status bar of its own.
    Wallpaper::draw(tft);

    // Flat panels behind each text block instead of UI::centerText's
    // usual trick (opaque text drawn straight onto a hardcoded Theme::BG
    // background) - that trick punches a Theme::BG-colored box behind
    // every glyph, which looks broken once there's a real photo behind it
    // instead of a flat color.
    if (_lockClock) {
      uint32_t s = _lockClock->secondsSinceMidnight();
      char buf[6];
      snprintf(buf, sizeof(buf), "%02u:%02u", (unsigned)((s / 3600) % 24), (unsigned)((s / 60) % 60));
      tft.fillRoundRect(Cfg::SCREEN_W / 2 - 70, Cfg::SCREEN_H / 2 - 46, 140, 60, 10, Theme::PANEL);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(Theme::ACCENT);
      tft.drawString(buf, Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 20, 7);
      tft.setTextColor(Theme::MUTED);
      tft.drawString("CydOs", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 6, 2);
      tft.setTextDatum(TL_DATUM);
    } else {
      tft.fillRoundRect(Cfg::SCREEN_W / 2 - 50, Cfg::SCREEN_H / 2 - 16, 100, 32, 8, Theme::PANEL);
      tft.setTextDatum(MC_DATUM);
      tft.setTextColor(Theme::MUTED);
      tft.drawString("CydOs", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 2);
      tft.setTextDatum(TL_DATUM);
    }

    tft.fillRoundRect(Cfg::SCREEN_W / 2 - 90, Cfg::SCREEN_H - 46, 180, 26, 8, Theme::PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(Theme::ACCENT);
    tft.drawString("Double-tap to unlock", Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 33, 2);
    tft.setTextDatum(TL_DATUM);
  }

  TFT_eSPI* _tft = nullptr;
  Battery* _battery = nullptr;
  UpdateChecker* _updates = nullptr;
  App* _apps[MAX_APPS] = {nullptr};
  uint8_t _count = 0;
  uint8_t _currentIndex = 0;
  App* _current = nullptr;
  uint8_t _brightnessPct = 80;
  bool _showBattery = true;
  uint32_t _lastBarRefresh = 0;
  bool _lastHeld = false;

  bool _lockEnabled = false;
  bool _locked = false;
  uint32_t _lastTapAt = 0;
  WallClock* _lockClock = nullptr;
};
