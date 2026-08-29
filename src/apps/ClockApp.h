#pragma once
#include "App.h"
#include "core/UI.h"

// Three tabs in one app: a manually-set clock, a stopwatch, and a
// countdown timer. All styled the same, all driven off millis().
class ClockApp : public App {
public:
  const char* name() const override { return "Clock"; }

  void onEnter(TFT_eSPI& tft) override { _dirty = true; }
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Tab { CLOCK, STOPWATCH, TIMER, TAB_COUNT };
  Tab _tab = CLOCK;

  UI::Button _tabBtns[TAB_COUNT] = {
      {{0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W / 3, 32}, "Clock"},
      {{Cfg::SCREEN_W / 3, Cfg::STATUS_BAR_H, Cfg::SCREEN_W / 3, 32}, "Stopwatch"},
      {{2 * Cfg::SCREEN_W / 3, Cfg::STATUS_BAR_H, Cfg::SCREEN_W / 3, 32}, "Timer"},
  };

  // Clock tab
  int32_t _clockOffsetSec = 12 * 3600;  // seconds since midnight, settable
  UI::Button _hourUp{{40, 170, 40, 34}, "H+"};
  UI::Button _hourDn{{90, 170, 40, 34}, "H-"};
  UI::Button _minUp{{190, 170, 40, 34}, "M+"};
  UI::Button _minDn{{240, 170, 40, 34}, "M-"};

  // Stopwatch tab
  bool _swRunning = false;
  uint32_t _swStartMs = 0;
  uint32_t _swAccumMs = 0;
  UI::Button _swStartStop{{40, 170, 110, 40}, "Start"};
  UI::Button _swReset{{170, 170, 110, 40}, "Reset"};

  // Timer tab
  int32_t _timerSetSec = 60;
  int32_t _timerRemainMs = 60000;
  bool _timerRunning = false;
  uint32_t _timerLastTick = 0;
  UI::Button _timerMinUp{{20, 130, 60, 34}, "+1m"};
  UI::Button _timerMinDn{{90, 130, 60, 34}, "-1m"};
  UI::Button _timerSecUp{{170, 130, 60, 34}, "+10s"};
  UI::Button _timerSecDn{{240, 130, 60, 34}, "-10s"};
  UI::Button _timerStartStop{{40, 175, 110, 36}, "Start"};
  UI::Button _timerReset{{170, 175, 110, 36}, "Reset"};

  bool _dirty = true;

  void drawClockTab(TFT_eSPI& tft);
  void drawStopwatchTab(TFT_eSPI& tft);
  void drawTimerTab(TFT_eSPI& tft);
  void drawTabs(TFT_eSPI& tft);
  static void formatHMS(uint32_t totalMs, char* out, size_t n, bool withMillis);
};
