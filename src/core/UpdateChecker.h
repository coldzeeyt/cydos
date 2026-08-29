#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <string.h>
#include "Config.h"

// Periodically (and once shortly after boot) checks Cfg::UPDATE_CHECK_URL
// for a newer firmware version and raises a dismissible "New Update!"
// banner (drawn by AppManager) when one is found. Only ever runs when WiFi
// credentials are available - a device with none configured, or one that
// can't reach the network, simply never checks in, which is exactly what
// "only alert the ones that are online" means here: there's no server
// pushing to devices, each device pulls in only when it can.
//
// Credentials: whatever's saved on-device (Settings > WiFi Setup) wins;
// otherwise falls back to Cfg::WIFI_SSID / Cfg::WIFI_PASSWORD.
class UpdateChecker {
public:
  void begin(Preferences* prefs) {
    _prefs = prefs;
    _nextCheckAt = millis() + Cfg::UPDATE_CHECK_BOOT_DELAY_MS;
  }

  // Call every loop() tick. Non-blocking except for the brief (bounded)
  // WiFi connect + HTTPS GET that happens only when a check is actually due.
  void update();

  // Force a check on the very next update() call - used by Settings' "Test
  // Now" button so setting up WiFi gives immediate feedback.
  void checkNow() { _nextCheckAt = millis(); }

  bool shouldShowBanner() const { return _hasUpdate && !_dismissed; }
  const char* latestVersion() const { return _latestVersion; }
  void dismiss() { _dismissed = true; }

  // Human-readable status for the Settings > WiFi screen.
  const char* lastResult() const { return _lastResult; }

private:
  Preferences* _prefs = nullptr;
  uint32_t _nextCheckAt = 0;
  bool _hasUpdate = false;
  bool _dismissed = false;
  char _latestVersion[16] = {0};
  char _lastResult[24] = "not checked yet";

  void setResult(const char* msg) {
    strncpy(_lastResult, msg, sizeof(_lastResult) - 1);
    _lastResult[sizeof(_lastResult) - 1] = '\0';
  }
  void performCheck();
};
