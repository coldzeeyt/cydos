#pragma once
#include <Arduino.h>
#include "Config.h"

// Best-effort LiPo battery gauge for a user-wired voltage divider.
// If Cfg::BATTERY_MONITOR_ENABLED is false this just reports "unknown"
// so the status bar can hide the battery icon instead of showing junk.
class Battery {
public:
  void begin() {
    if (Cfg::BATTERY_MONITOR_ENABLED) {
      pinMode(Cfg::BATTERY_ADC_PIN, INPUT);
      analogReadResolution(12);
    }
  }

  bool available() const { return Cfg::BATTERY_MONITOR_ENABLED; }

  float voltage() const {
    if (!Cfg::BATTERY_MONITOR_ENABLED) return 0.0f;
    uint32_t sum = 0;
    const int samples = 8;
    for (int i = 0; i < samples; i++) sum += analogRead(Cfg::BATTERY_ADC_PIN);
    float raw = sum / (float)samples;
    float vAtPin = (raw / 4095.0f) * Cfg::BATTERY_ADC_VREF;
    return vAtPin * Cfg::BATTERY_DIVIDER_RATIO;
  }

  uint8_t percent() const {
    if (!Cfg::BATTERY_MONITOR_ENABLED) return 0;
    float v = voltage();
    float pct = (v - Cfg::BATTERY_EMPTY_V) / (Cfg::BATTERY_FULL_V - Cfg::BATTERY_EMPTY_V) * 100.0f;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)pct;
  }
};
