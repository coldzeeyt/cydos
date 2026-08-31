#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Config.h"

// Thin wrapper around the Arduino SD library for the CYD's microSD slot.
//
// The SD slot turned out to be wired to its own dedicated CLK/MOSI/MISO
// lines (Cfg::SD_CLK/MOSI/MISO), not sharing the touch controller's like
// originally assumed - see the comment above those constants in Config.h.
// ESP32 only has two usable hardware SPI peripherals; the display has its
// own (USE_HSPI_PORT in platformio.ini), which leaves touch and SD to
// share the other one (the global `SPI` object/VSPI). Since they're on
// different pins, "sharing" here means taking turns: SPI.begin() is a
// real hardware reconfiguration (routes the peripheral's GPIO matrix to
// specific pins), not just a settings tweak, so every block of SD access
// has to point the bus at SD's pins first and hand it back to touch's
// afterward - see useSdBus()/useTouchBus() below, and grep for their call
// sites (every real `SD.xxx()` call in this codebase sits between a pair
// of them). Forgetting the "hand it back" half would leave touch reading
// garbage on whatever pins SD last used.
//
// Caveat: this bus-switching design is inferred from what actually fixed
// (and didn't fix) issues on one real device - no physical board + SD
// card was available while first writing this file, and the original
// shared-bus assumption is what several CYD wiring references describe.
// If cards still aren't detected, double check Cfg::SD_CLK/MOSI/MISO/CS
// against your specific board revision and that FAT32 formatting is used.
class SdCard {
public:
  bool begin() {
    useSdBus();
    _available = SD.begin(Cfg::SD_CS, SPI, 4000000);
    useTouchBus();
    return _available;
  }

  bool available() const { return _available; }

  // Point the shared SPI bus at the SD card's pins. Call once before a
  // block of SD.xxx()/File calls, and pair with useTouchBus() after.
  static void useSdBus() {
    SPI.begin(Cfg::SD_CLK, Cfg::SD_MISO, Cfg::SD_MOSI, Cfg::SD_CS);
  }

  // Hand the shared SPI bus back to the touch controller. Every SD access
  // site must call this when it's done, or touch reads garbage until
  // something else happens to repoint the bus.
  static void useTouchBus() {
    SPI.begin(Cfg::TOUCH_CLK, Cfg::TOUCH_MISO, Cfg::TOUCH_MOSI, Cfg::TOUCH_CS);
  }

private:
  bool _available = false;
};
