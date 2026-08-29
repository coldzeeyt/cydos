#pragma once
#include <WiFi.h>
#include "App.h"
#include "core/UI.h"

// Sweeping radar view of nearby WiFi networks. Each SSID gets a stable
// angle (hashed from its name) so blips don't jump around between scans;
// its distance from center tracks signal strength, so walking toward a
// stronger signal visibly pulls the blip inward - hot/cold hunting.
class WifiRadarApp : public App {
public:
  const char* name() const override { return "WiFi Radar"; }

  void onEnter(TFT_eSPI& tft) override;
  void onExit() override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  static constexpr uint8_t MAX_NETS = 24;

  struct Net {
    String ssid;
    int32_t rssi;
    int32_t channel;
    wifi_auth_mode_t enc;
    uint16_t angleDeg;
  };

  Net _nets[MAX_NETS];
  uint8_t _netCount = 0;
  bool _scanning = false;
  uint32_t _lastScanStart = 0;
  float _sweepDeg = 0;
  uint32_t _lastFrame = 0;
  int8_t _selected = -1;
  bool _dirty = true;

  int16_t _cx, _cy, _radius;

  void startScan();
  void collectResults();
  UI::Rect blipHit(uint8_t i) const;
  void drawRadarFace(TFT_eSPI& tft);
  void drawBlips(TFT_eSPI& tft);
  void drawDetail(TFT_eSPI& tft);
};
