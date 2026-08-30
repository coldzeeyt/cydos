#pragma once
#include <SD.h>
#include "AppManager.h"
#include "UI.h"
#include "apps/SdCardApp.h"
#include "apps/HomeApp.h"

// Owns the fixed pool of SdCardApp slots and the logic that turns
// /cydos_apps/*.cydapp files into live Home tiles. Used once at boot (see
// main.cpp) and again any time the App Store's "Get More" tab downloads a
// new .cydapp over WiFi - rescan() is safe to call repeatedly, so a
// downloaded file becomes a real Home tile immediately, no reboot needed.
class SdAppPool {
public:
  static constexpr uint8_t MAX_APPS = 6;

  void begin(AppManager* mgr, HomeApp* home) {
    _mgr = mgr;
    _home = home;
  }

  void rescan() {
    File dir = SD.open("/cydos_apps");
    if (!dir || !dir.isDirectory()) {
      if (dir) dir.close();
      return;
    }

    File entry = dir.openNextFile();
    while (entry) {
      if (!entry.isDirectory()) {
        String fname = entry.name();
        String lower = fname;
        lower.toLowerCase();
        if (lower.endsWith(".cydapp")) {
          String path = fname.startsWith("/") ? fname : String("/cydos_apps/") + fname;
          entry.close();
          if (!alreadyLoaded(path.c_str())) loadIntoFreeSlot(path.c_str());
          entry = dir.openNextFile();
          continue;
        }
      }
      entry.close();
      entry = dir.openNextFile();
    }
    dir.close();
  }

private:
  bool alreadyLoaded(const char* path) const {
    for (uint8_t i = 0; i < MAX_APPS; i++) {
      if (_used[i] && strcmp(_apps[i].path(), path) == 0) return true;
    }
    return false;
  }

  void loadIntoFreeSlot(const char* path) {
    for (uint8_t i = 0; i < MAX_APPS; i++) {
      if (_used[i]) continue;
      if (_apps[i].load(path)) {
        _used[i] = true;
        uint8_t idx = _mgr->registerApp(&_apps[i]);
        _home->addTile(UI::iconPuzzle, idx);
      }
      return;
    }
  }

  AppManager* _mgr = nullptr;
  HomeApp* _home = nullptr;
  SdCardApp _apps[MAX_APPS];
  bool _used[MAX_APPS] = {false};
};
