#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>
#include "Config.h"

// Wraps the XPT2046 resistive touch controller and turns its raw ADC
// readings into calibrated screen coordinates for Cfg::SCREEN_ROTATION.
class Touch {
public:
  void begin() {
    // XPT2046_Touchscreen drives the Arduino global SPI object. TFT_eSPI
    // manages its own SPI peripheral internally on the TFT_* pins, so
    // repointing the global SPI bus at the touch controller's pins here
    // does not disturb the display - this is the standard CYD wiring.
    SPI.begin(Cfg::TOUCH_CLK, Cfg::TOUCH_MISO, Cfg::TOUCH_MOSI, Cfg::TOUCH_CS);
    _ts.begin();
    // Deliberately not using ts.setRotation() - all raw->screen mapping is
    // done by hand below so behaviour is predictable and easy to tune via
    // the Cfg::TOUCH_* constants (see Settings > Touch Test).
  }

  // Returns true and fills x/y (screen space) if the panel is currently pressed.
  bool read(int16_t &x, int16_t &y) {
    if (!_ts.touched()) return false;
    TS_Point p = _ts.getPoint();

    int16_t rx = p.x, ry = p.y;
    if (Cfg::TOUCH_SWAP_XY) { int16_t t = rx; rx = ry; ry = t; }

    int16_t sx = map(rx, Cfg::TOUCH_RAW_X_MIN, Cfg::TOUCH_RAW_X_MAX, 0, Cfg::SCREEN_W - 1);
    int16_t sy = map(ry, Cfg::TOUCH_RAW_Y_MIN, Cfg::TOUCH_RAW_Y_MAX, 0, Cfg::SCREEN_H - 1);
    if (Cfg::TOUCH_INVERT_X) sx = Cfg::SCREEN_W - 1 - sx;
    if (Cfg::TOUCH_INVERT_Y) sy = Cfg::SCREEN_H - 1 - sy;

    x = constrain(sx, 0, Cfg::SCREEN_W - 1);
    y = constrain(sy, 0, Cfg::SCREEN_H - 1);
    return true;
  }

  // Raw, uncalibrated point - only used by the Settings > Touch Test screen.
  bool readRaw(int16_t &rx, int16_t &ry) {
    if (!_ts.touched()) return false;
    TS_Point p = _ts.getPoint();
    rx = p.x;
    ry = p.y;
    return true;
  }

private:
  XPT2046_Touchscreen _ts{Cfg::TOUCH_CS, Cfg::TOUCH_IRQ};
};
