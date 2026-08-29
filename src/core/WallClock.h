#pragma once
#include <Arduino.h>

// A manually-set clock with no RTC and no NTP - just an offset from
// millis(). Shared between the Clock app (which displays it and offers
// its own +/- buttons) and Settings > Set Time, so either place can set
// it and both stay in sync. Resets to noon on every power-up, same as
// before - there's no battery-backed RTC on the CYD to remember it.
class WallClock {
public:
  void addHours(int32_t h) { _offsetSec += h * 3600; }
  void addMinutes(int32_t m) { _offsetSec += m * 60; }

  uint32_t secondsSinceMidnight() const {
    int64_t t = (int64_t)_offsetSec + (int64_t)(millis() / 1000);
    t %= 86400;
    if (t < 0) t += 86400;
    return (uint32_t)t;
  }

private:
  int32_t _offsetSec = 12 * 3600; // default to noon
};
