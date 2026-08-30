#pragma once
#include "App.h"
#include "core/UI.h"

class Battery;
class SdCard;

// A one-stop hardware check, gated behind Settings > Dev Mode since none
// of this is something a regular user needs day to day (and a stray tap
// into a full-screen solid red test pattern is a confusing thing to land
// on by accident). Three tabs:
//  - Display: solid R/G/B/W/K fills, a checkerboard (catches a stuck
//    pixel that a same-color solid fill would hide), and gradients for
//    spotting banding - tap anywhere below the tab bar to cycle.
//  - Touch: a live crosshair under your finger, same idea as Settings'
//    Touch Test but folded in here as part of the same hardware sweep.
//  - Info: chip/memory/uptime, SD card presence+size, battery voltage.
class DiagnosticsApp : public App {
public:
  DiagnosticsApp(Battery* battery, SdCard* sd) : _battery(battery), _sd(sd) {}

  const char* name() const override { return _title; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Tab : uint8_t { TAB_DISPLAY, TAB_TOUCH, TAB_INFO, TAB_COUNT };
  static constexpr uint8_t PATTERN_COUNT = 8;
  static const char* const PATTERN_NAMES[PATTERN_COUNT];
  static const char* const TAB_NAMES[TAB_COUNT];

  Battery* _battery;
  SdCard* _sd;
  Tab _tab = TAB_DISPLAY;
  uint8_t _pattern = 0;
  int16_t _touchX = -1, _touchY = -1;
  uint32_t _lastInfoRefresh = 0;
  char _title[24] = "Diagnostics";
  bool _dirty = true;

  UI::Rect tabRect(uint8_t i) const;
  void updateTitle();
  void drawTabBar(TFT_eSPI& tft);
  void drawDisplayPage(TFT_eSPI& tft);
  void drawTouchPage(TFT_eSPI& tft);
  void drawInfoPage(TFT_eSPI& tft);
};
