#pragma once
#include <Arduino.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include "Config.h"
#include "SdCard.h"

// Draws /cydos_wallpaper.bmp from the SD card as Home's background, if
// present - a plain 24-bit uncompressed BMP, exactly
// Cfg::SCREEN_W x (Cfg::SCREEN_H - Cfg::STATUS_BAR_H) pixels (320x214),
// since it only needs to cover the content area below the status bar.
// (The App Store's Wallpapers tab generates this exact format from any
// image client-side, so nobody needs to hand-craft it.)
//
// Re-reads and re-decodes the file from SD every time draw() is called
// rather than caching a decoded copy in RAM - a full-screen RGB565
// framebuffer would be ~135KB, too much of this board's ~320KB RAM to
// spend on a cache - so opening or swiping Home has a brief pause while
// a wallpaper is active. Same "written but not verified on real
// hardware" caveat as SdCard.h/SdCardApp.h.
namespace Wallpaper {

inline uint16_t readLE16(File& f) {
  uint8_t b0 = f.read(), b1 = f.read();
  return (uint16_t)b0 | ((uint16_t)b1 << 8);
}
inline uint32_t readLE32(File& f) {
  uint8_t b0 = f.read(), b1 = f.read(), b2 = f.read(), b3 = f.read();
  return (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
}

// Which file Home (and Settings > Wallpapers' preview) actually draws.
// Defaults to the single file every existing doc/tool already tells people
// to copy to the SD card root; Settings > Wallpapers can point this at one
// of several files under /cydos_wallpapers/ instead - see SettingsApp.
// A function-local static (not a namespace-scope one) so this stays a
// single shared instance across every .cpp that includes this header
// without needing C++17 inline variables.
inline char* activePathBuf() {
  static char path[48] = "/cydos_wallpaper.bmp";
  return path;
}

inline void setActivePath(const char* path) {
  char* buf = activePathBuf();
  strncpy(buf, path, 47);
  buf[47] = 0;
}
inline const char* activePath() { return activePathBuf(); }

// Returns true if it actually drew a wallpaper - callers should fall back
// to their normal solid background otherwise (missing file, wrong
// dimensions/format, anything unexpected all just return false, never a
// crash or a half-drawn screen).
inline bool drawFrom(TFT_eSPI& tft, const char* path) {
  // The tft.pushImage() calls below stay safe to interleave with the SD
  // reads in this same loop - the display has its own dedicated SPI
  // peripheral (USE_HSPI_PORT), so it's never affected by which pins the
  // one SD/touch share (see SdCard.h) currently points at.
  SdCard::useSdBus();
  File f = SD.open(path);
  if (!f) { SdCard::useTouchBus(); return false; }

  const int16_t wantW = Cfg::SCREEN_W;
  const int16_t wantH = Cfg::SCREEN_H - Cfg::STATUS_BAR_H;
  bool ok = false;

  if (f.read() == 'B' && f.read() == 'M') {
    f.seek(10); uint32_t dataOffset = readLE32(f);
    f.seek(18); int32_t w = (int32_t)readLE32(f);
    int32_t h = (int32_t)readLE32(f);
    f.seek(28); uint16_t bpp = readLE16(f);
    f.seek(30); uint32_t compression = readLE32(f);

    if (bpp == 24 && compression == 0 && w == wantW && abs(h) == wantH) {
      uint32_t rowBytes = ((uint32_t)w * 3 + 3) & ~3u; // BMP rows pad to a 4-byte boundary
      bool bottomUp = h > 0;
      static uint16_t lineBuf[Cfg::SCREEN_W];

      ok = true;
      for (int16_t row = 0; row < wantH; row++) {
        if (!f.seek(dataOffset + (uint32_t)row * rowBytes)) { ok = false; break; }
        for (int16_t x = 0; x < w; x++) {
          uint8_t b = f.read(), g = f.read(), r = f.read();
          lineBuf[x] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
        }
        int16_t destRow = bottomUp ? (wantH - 1 - row) : row;
        tft.pushImage(0, Cfg::STATUS_BAR_H + destRow, w, 1, lineBuf);
      }
    }
  }
  f.close();
  SdCard::useTouchBus();
  return ok;
}

// Draws whichever file setActivePath() last pointed at (or the default).
inline bool draw(TFT_eSPI& tft) { return drawFrom(tft, activePathBuf()); }

} // namespace Wallpaper
