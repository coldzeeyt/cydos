#pragma once
#include <Arduino.h>

// =====================================================================
// CydOs hardware configuration.
// Pin numbers below match the common ESP32-2432S028R "Cheap Yellow
// Display" board. If your board revision differs, this is the only
// file you should need to touch.
// =====================================================================

namespace Cfg {

// ---- Screen ----
constexpr uint8_t SCREEN_ROTATION = 1;     // 1 = landscape, USB on the left. Try 3 if upside down.
constexpr int16_t SCREEN_W = 320;
constexpr int16_t SCREEN_H = 240;
constexpr int16_t STATUS_BAR_H = 26;

// ---- Touch (XPT2046, on its own SPI bus - standard CYD wiring) ----
constexpr int8_t TOUCH_CLK = 25;
constexpr int8_t TOUCH_MOSI = 32;
constexpr int8_t TOUCH_MISO = 39;
constexpr int8_t TOUCH_CS = 33;
constexpr int8_t TOUCH_IRQ = 36;

// Raw ADC range coming back from the touch controller. Cheap resistive
// panels vary a bit unit to unit - if taps feel off, use Settings > Touch
// Test to see raw behaviour and tweak these four numbers.
constexpr int16_t TOUCH_RAW_X_MIN = 300;
constexpr int16_t TOUCH_RAW_X_MAX = 3800;
constexpr int16_t TOUCH_RAW_Y_MIN = 300;
constexpr int16_t TOUCH_RAW_Y_MAX = 3800;
constexpr bool TOUCH_SWAP_XY = true;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = true;

// ---- Backlight (PWM brightness control) ----
constexpr int8_t TFT_BL_PIN = 21;
constexpr uint8_t BL_PWM_CHANNEL = 0;
constexpr uint32_t BL_PWM_FREQ = 5000;
constexpr uint8_t BL_PWM_RES_BITS = 8;

// ---- Battery monitor (user-added) ----
// Optional: wire a 100k/100k divider from BAT+ to this ADC pin so the
// max ~4.2V LiPo voltage is halved to a safe ~2.1V for the ESP32 ADC.
// Set BATTERY_MONITOR_ENABLED to false if you haven't wired one up.
constexpr bool BATTERY_MONITOR_ENABLED = true;
constexpr int8_t BATTERY_ADC_PIN = 35;
constexpr float BATTERY_DIVIDER_RATIO = 2.0f;   // (R1+R2)/R2
constexpr float BATTERY_ADC_VREF = 3.3f;
constexpr float BATTERY_EMPTY_V = 3.3f;
constexpr float BATTERY_FULL_V = 4.2f;

} // namespace Cfg
