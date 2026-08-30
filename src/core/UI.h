#pragma once
#include <TFT_eSPI.h>
#include "Config.h"

// Shared look & feel + tiny widget helpers so every app in CydOs feels
// like part of the same system instead of a pile of demos.

namespace Theme {
constexpr uint16_t BG      = 0x0841;  // near-black navy
constexpr uint16_t PANEL   = 0x1A69;  // dark slate panel
constexpr uint16_t PANEL2  = 0x2A8A;  // slightly lighter panel (pressed/hover)
constexpr uint16_t ACCENT  = 0x07FF;  // cyan
constexpr uint16_t ACCENT2 = 0xFD20;  // amber
constexpr uint16_t GOOD    = 0x07E6;  // green
constexpr uint16_t DANGER  = 0xF800;  // red
constexpr uint16_t TEXT    = 0xFFFF;  // white
constexpr uint16_t MUTED   = 0x8C71;  // grey-blue
}

namespace UI {

struct Rect {
  int16_t x, y, w, h;
  bool contains(int16_t px, int16_t py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

struct Button {
  Rect r;
  const char* label;
  uint16_t color;
  uint16_t textColor;
  bool active;

  Button(Rect rect, const char* lbl, uint16_t col = Theme::PANEL, uint16_t txtColor = Theme::TEXT)
      : r(rect), label(lbl), color(col), textColor(txtColor), active(false) {}

  void draw(TFT_eSPI& tft) const {
    uint16_t fill = active ? Theme::ACCENT : color;
    uint16_t txt = active ? Theme::BG : textColor;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 6, fill);
    tft.drawRoundRect(r.x, r.y, r.w, r.h, 6, Theme::MUTED);
    tft.setTextColor(txt, fill);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2 + 1, 2);
    tft.setTextDatum(TL_DATUM);
  }

  bool hit(int16_t x, int16_t y) const { return r.contains(x, y); }
};

struct Slider {
  Rect r;
  int value;   // 0-100
  uint16_t color;

  Slider(Rect rect, int val = 50, uint16_t col = Theme::ACCENT) : r(rect), value(val), color(col) {}

  void draw(TFT_eSPI& tft) const {
    int16_t trackY = r.y + r.h / 2 - 3;
    tft.fillRoundRect(r.x, trackY, r.w, 6, 3, Theme::PANEL2);
    int16_t fillW = map(value, 0, 100, 0, r.w);
    if (fillW > 0) tft.fillRoundRect(r.x, trackY, fillW, 6, 3, color);
    int16_t knobX = r.x + fillW;
    tft.fillCircle(knobX, r.y + r.h / 2, 9, Theme::TEXT);
    tft.fillCircle(knobX, r.y + r.h / 2, 6, color);
  }

  bool hit(int16_t x, int16_t y) const {
    Rect padded{(int16_t)(r.x - 10), (int16_t)(r.y - 10), (int16_t)(r.w + 20), (int16_t)(r.h + 20)};
    return padded.contains(x, y);
  }

  // Call while touch is held over the slider.
  void updateFromTouch(int16_t x) {
    int v = map(x, r.x, r.x + r.w, 0, 100);
    value = constrain(v, 0, 100);
  }
};

// Blends fg over bg at the given opacity (0-1), all in RGB565. TFT_eSPI has
// no real alpha channel, so this just does the channel math by hand -
// used for the timer's "flash" alarm rather than a flat, jarring color.
inline uint16_t blend565(uint16_t fg, uint16_t bg, float alpha) {
  uint8_t fr = (fg >> 11) & 0x1F, fgg = (fg >> 5) & 0x3F, fb = fg & 0x1F;
  uint8_t br = (bg >> 11) & 0x1F, bgg = (bg >> 5) & 0x3F, bb = bg & 0x1F;
  uint8_t r = (uint8_t)(fr * alpha + br * (1 - alpha));
  uint8_t g = (uint8_t)(fgg * alpha + bgg * (1 - alpha));
  uint8_t b = (uint8_t)(fb * alpha + bb * (1 - alpha));
  return (uint16_t)((r << 11) | (g << 5) | b);
}

inline void clearContent(TFT_eSPI& tft) {
  tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, Theme::BG);
}

inline void centerText(TFT_eSPI& tft, const char* s, int16_t cx, int16_t cy, uint8_t font, uint16_t color) {
  tft.setTextColor(color, Theme::BG);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(s, cx, cy, font);
  tft.setTextDatum(TL_DATUM);
}

// ---- Simple vector icons used by the Home launcher tiles ----
inline void iconWifi(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  for (int i = 0; i < 3; i++) {
    tft.drawArc(cx, cy + 6, 6 + i * 7, 4 + i * 7, 210, 330, c, Theme::PANEL, true);
  }
  tft.fillCircle(cx, cy + 6, 2, c);
}
inline void iconFlash(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.fillCircle(cx, cy, 10, c);
  for (int a = 0; a < 360; a += 45) {
    float r1 = 13, r2 = 18;
    float rad = a * DEG_TO_RAD;
    tft.drawLine(cx + cos(rad) * r1, cy + sin(rad) * r1, cx + cos(rad) * r2, cy + sin(rad) * r2, c);
  }
}
inline void iconClock(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawCircle(cx, cy, 14, c);
  tft.drawLine(cx, cy, cx, cy - 9, c);
  tft.drawLine(cx, cy, cx + 7, cy + 2, c);
}
inline void iconQR(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawRect(cx - 14, cy - 14, 12, 12, c);
  tft.drawRect(cx + 2, cy - 14, 12, 12, c);
  tft.drawRect(cx - 14, cy + 2, 12, 12, c);
  tft.fillRect(cx + 4, cy + 4, 4, 4, c);
  tft.fillRect(cx + 10, cy + 10, 4, 4, c);
}
inline void iconDice(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawRoundRect(cx - 14, cy - 14, 28, 28, 5, c);
  tft.fillCircle(cx - 6, cy - 6, 2, c);
  tft.fillCircle(cx + 6, cy + 6, 2, c);
  tft.fillCircle(cx, cy, 2, c);
}
inline void iconGear(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawCircle(cx, cy, 8, c);
  tft.drawCircle(cx, cy, 3, c);
  for (int a = 0; a < 360; a += 45) {
    float rad = a * DEG_TO_RAD;
    tft.drawLine(cx + cos(rad) * 10, cy + sin(rad) * 10, cx + cos(rad) * 14, cy + sin(rad) * 14, c);
  }
}
inline void iconCalc(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawRoundRect(cx - 12, cy - 15, 24, 30, 3, c);
  tft.drawFastHLine(cx - 8, cy - 9, 16, c);
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 3; col++) {
      tft.fillRect(cx - 8 + col * 7, cy - 1 + row * 6, 4, 3, c);
    }
  }
}
inline void iconKey(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawCircle(cx - 7, cy - 7, 6, c);
  tft.drawLine(cx - 3, cy - 3, cx + 12, cy + 12, c);
  tft.drawLine(cx + 8, cy + 8, cx + 12, cy + 4, c);
  tft.drawLine(cx + 12, cy + 12, cx + 16, cy + 8, c);
}
inline void iconMorse(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.fillCircle(cx - 12, cy, 3, c);
  tft.fillRoundRect(cx - 5, cy - 3, 12, 6, 2, c);
  tft.fillCircle(cx + 12, cy, 3, c);
}
inline void iconGlobe(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawCircle(cx, cy, 14, c);
  tft.drawEllipse(cx, cy, 6, 14, c);
  tft.drawFastHLine(cx - 14, cy, 28, c);
}
inline void iconBroadcast(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawRoundRect(cx - 15, cy - 9, 30, 22, 3, c);
  tft.fillTriangle(cx - 5, cy - 6, cx - 5, cy + 7, cx + 6, cy + 1, c);
  tft.drawLine(cx - 8, cy - 9, cx - 14, cy - 16, c);
  tft.drawLine(cx + 8, cy - 9, cx + 14, cy - 16, c);
}
inline void iconMusic(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.fillCircle(cx - 9, cy + 10, 5, c);
  tft.fillCircle(cx + 9, cy + 7, 5, c);
  tft.drawLine(cx - 4, cy + 10, cx - 4, cy - 14, c);
  tft.drawLine(cx + 14, cy + 7, cx + 14, cy - 17, c);
  tft.drawLine(cx - 4, cy - 14, cx + 14, cy - 17, c);
  tft.drawLine(cx - 4, cy - 9, cx + 14, cy - 12, c);
}

// Generic tile icon for community-submitted apps - a puzzle piece, since a
// submitted app brings its own screen but not its own icon-drawing code.
inline void iconPuzzle(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawRoundRect(cx - 13, cy - 13, 26, 26, 3, c);
  tft.drawCircle(cx + 6, cy - 13, 4, c);
  tft.drawCircle(cx - 13, cy + 6, 4, c);
}
inline void iconFolder(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.fillRoundRect(cx - 15, cy - 8, 12, 6, 2, c);
  tft.drawRoundRect(cx - 15, cy - 4, 30, 18, 2, c);
}
inline void iconStore(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawLine(cx - 14, cy - 6, cx - 10, cy - 16, c);
  tft.drawLine(cx + 14, cy - 6, cx + 10, cy - 16, c);
  tft.drawLine(cx - 10, cy - 16, cx + 10, cy - 16, c);
  tft.drawRoundRect(cx - 14, cy - 6, 28, 20, 3, c);
  tft.fillRect(cx - 4, cy + 4, 8, 10, c);
}
inline void iconDiag(TFT_eSPI& tft, int16_t cx, int16_t cy, uint16_t c) {
  tft.drawRoundRect(cx - 15, cy - 12, 30, 20, 2, c);
  tft.fillRect(cx - 11, cy - 8, 8, 8, c);
  tft.drawRect(cx + 1, cy - 8, 8, 8, c);
  tft.drawFastVLine(cx, cy + 8, 4, c);
  tft.drawFastHLine(cx - 6, cy + 12, 12, c);
}

} // namespace UI
