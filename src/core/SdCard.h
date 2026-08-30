#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "Config.h"

// Thin wrapper around the Arduino SD library for the CYD's microSD slot.
// The slot shares its SPI bus with the touch controller (same
// CLK/MOSI/MISO, a separate CS line - see Cfg::SD_CS), so begin() MUST be
// called after Touch::begin() has already pointed the global SPI object
// at those shared pins; this just adds the SD card's own CS to the same
// bus rather than reconfiguring it.
//
// Caveat: this is the one piece of CydOs I could not test against real
// hardware while writing it (no physical board + SD card in this
// environment) - the SPI bus sharing follows the documented, commonly
// used pattern for this board, but if cards aren't detected, start by
// checking Cfg::SD_CS matches your board revision and that FAT32
// formatting is used.
class SdCard {
public:
  bool begin() {
    _available = SD.begin(Cfg::SD_CS, SPI);
    return _available;
  }

  bool available() const { return _available; }

private:
  bool _available = false;
};
