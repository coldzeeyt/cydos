#pragma once
#include <Arduino.h>
#include "Config.h"

// Backlight brightness control via LEDC PWM, shared by the Settings
// brightness slider and the Flashlight app.
namespace Display {

inline void beginBacklight() {
  ledcSetup(Cfg::BL_PWM_CHANNEL, Cfg::BL_PWM_FREQ, Cfg::BL_PWM_RES_BITS);
  ledcAttachPin(Cfg::TFT_BL_PIN, Cfg::BL_PWM_CHANNEL);
  ledcWrite(Cfg::BL_PWM_CHANNEL, 255);
}

inline void setBrightnessPercent(uint8_t pct) {
  if (pct > 100) pct = 100;
  uint32_t duty = map(pct, 0, 100, 0, 255);
  ledcWrite(Cfg::BL_PWM_CHANNEL, duty);
}

} // namespace Display
