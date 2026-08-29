#pragma once
#include <TFT_eSPI.h>
#include "Config.h"
#include "UI.h"
#include "Battery.h"
#include "UpdateChecker.h"
#include "apps/App.h"

// Owns the status bar (title, battery, back-to-home) and switches the
// active App. Index 0 is always the Home launcher.
class AppManager {
public:
  static constexpr uint8_t MAX_APPS = 12;

  void begin(TFT_eSPI& tft, Battery* battery, UpdateChecker* updates = nullptr) {
    _tft = &tft;
    _battery = battery;
    _updates = updates;
  }

  uint8_t registerApp(App* app) {
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

  void loop(int16_t touchX, int16_t touchY, bool touchDown, bool touchHeld) {
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
};
