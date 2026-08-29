#include "ClockApp.h"

void ClockApp::formatHMS(uint32_t totalMs, char* out, size_t n, bool withMillis) {
  uint32_t totalSec = totalMs / 1000;
  uint32_t h = totalSec / 3600;
  uint32_t m = (totalSec % 3600) / 60;
  uint32_t s = totalSec % 60;
  if (withMillis) {
    uint32_t ms = (totalMs % 1000) / 10;
    snprintf(out, n, "%02u:%02u.%02u", m, s, ms);
  } else if (h > 0) {
    snprintf(out, n, "%02u:%02u:%02u", h, m, s);
  } else {
    snprintf(out, n, "%02u:%02u", m, s);
  }
}

bool ClockApp::update() {
  bool changed = false;
  if (_tab == CLOCK) changed = true;  // seconds tick, needs redraw ~1/s (cheap to just always mark dirty via draw's own timing)
  if (_tab == STOPWATCH && _swRunning) changed = true;
  if (_tab == TIMER && _timerRunning) {
    uint32_t now = millis();
    uint32_t delta = now - _timerLastTick;
    _timerLastTick = now;
    _timerRemainMs -= delta;
    if (_timerRemainMs <= 0) {
      _timerRemainMs = 0;
      _timerRunning = false;
    }
    changed = true;
  }
  if (changed) _dirty = true;
  return _dirty;
}

void ClockApp::drawTabs(TFT_eSPI& tft) {
  for (int i = 0; i < TAB_COUNT; i++) {
    _tabBtns[i].active = (_tab == i);
    _tabBtns[i].draw(tft);
  }
}

void ClockApp::drawClockTab(TFT_eSPI& tft) {
  tft.fillRect(0, Cfg::STATUS_BAR_H + 32, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H - 32, Theme::BG);
  uint32_t secSinceMidnight = (_clockOffsetSec + millis() / 1000) % 86400;
  uint32_t h = secSinceMidnight / 3600;
  uint32_t m = (secSinceMidnight % 3600) / 60;
  uint32_t s = secSinceMidnight % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02u:%02u:%02u", h, m, s);
  UI::centerText(tft, buf, Cfg::SCREEN_W / 2, 110, 7, Theme::ACCENT);
  UI::centerText(tft, "no RTC - set manually below", Cfg::SCREEN_W / 2, 145, 2, Theme::MUTED);
  _hourUp.draw(tft);
  _hourDn.draw(tft);
  _minUp.draw(tft);
  _minDn.draw(tft);
}

void ClockApp::drawStopwatchTab(TFT_eSPI& tft) {
  tft.fillRect(0, Cfg::STATUS_BAR_H + 32, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H - 32, Theme::BG);
  uint32_t elapsed = _swAccumMs + (_swRunning ? millis() - _swStartMs : 0);
  char buf[16];
  formatHMS(elapsed, buf, sizeof(buf), true);
  UI::centerText(tft, buf, Cfg::SCREEN_W / 2, 110, 7, Theme::ACCENT);
  _swStartStop.label = _swRunning ? "Stop" : "Start";
  _swStartStop.draw(tft);
  _swReset.draw(tft);
}

void ClockApp::drawTimerTab(TFT_eSPI& tft) {
  tft.fillRect(0, Cfg::STATUS_BAR_H + 32, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H - 32, Theme::BG);
  char buf[16];
  formatHMS(_timerRemainMs > 0 ? _timerRemainMs : 0, buf, sizeof(buf), false);
  uint16_t color = (_timerRemainMs <= 0) ? Theme::DANGER : Theme::ACCENT;
  UI::centerText(tft, buf, Cfg::SCREEN_W / 2, 90, 7, color);

  _timerMinUp.draw(tft);
  _timerMinDn.draw(tft);
  _timerSecUp.draw(tft);
  _timerSecDn.draw(tft);
  _timerStartStop.label = _timerRunning ? "Pause" : "Start";
  _timerStartStop.draw(tft);
  _timerReset.draw(tft);
}

void ClockApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  drawTabs(tft);
  if (_tab == CLOCK) drawClockTab(tft);
  else if (_tab == STOPWATCH) drawStopwatchTab(tft);
  else drawTimerTab(tft);
}

void ClockApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  for (int i = 0; i < TAB_COUNT; i++) {
    if (_tabBtns[i].hit(x, y)) {
      _tab = (Tab)i;
      _dirty = true;
      return;
    }
  }

  if (_tab == CLOCK) {
    if (_hourUp.hit(x, y)) _clockOffsetSec += 3600;
    else if (_hourDn.hit(x, y)) _clockOffsetSec -= 3600;
    else if (_minUp.hit(x, y)) _clockOffsetSec += 60;
    else if (_minDn.hit(x, y)) _clockOffsetSec -= 60;
    else return;
    _clockOffsetSec = ((_clockOffsetSec % 86400) + 86400) % 86400;
    _dirty = true;
  } else if (_tab == STOPWATCH) {
    if (_swStartStop.hit(x, y)) {
      if (_swRunning) {
        _swAccumMs += millis() - _swStartMs;
        _swRunning = false;
      } else {
        _swStartMs = millis();
        _swRunning = true;
      }
      _dirty = true;
    } else if (_swReset.hit(x, y)) {
      _swRunning = false;
      _swAccumMs = 0;
      _dirty = true;
    }
  } else if (_tab == TIMER) {
    if (_timerRunning) return;  // pause first to edit
    if (_timerMinUp.hit(x, y)) _timerSetSec += 60;
    else if (_timerMinDn.hit(x, y)) _timerSetSec = max((int32_t)10, _timerSetSec - 60);
    else if (_timerSecUp.hit(x, y)) _timerSetSec += 10;
    else if (_timerSecDn.hit(x, y)) _timerSetSec = max((int32_t)10, _timerSetSec - 10);
    else if (_timerStartStop.hit(x, y)) {
      _timerRemainMs = _timerSetSec * 1000;
      _timerRunning = true;
      _timerLastTick = millis();
    } else if (_timerReset.hit(x, y)) {
      _timerRemainMs = _timerSetSec * 1000;
    } else {
      return;
    }
    _timerRemainMs = _timerSetSec * 1000;
    _dirty = true;
  }
}
