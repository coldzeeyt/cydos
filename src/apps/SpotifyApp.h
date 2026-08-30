#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"

// Shows what's currently playing on Spotify, via the companion script in
// spotify-script/cydos_now_playing.py (a standalone script - not OBS
// scripting, Spotify has nothing to do with OBS - run with `python3
// cydos_now_playing.py`) polled over plain HTTP on your LAN:
//   GET /now-playing -> {"playing": bool, "track", "artist", "album",
//                        "progress_ms", "duration_ms"}
// WiFi is connected once on entry and held open while this app is open,
// same as the OBS app, and for the same reason: polling every few
// seconds needs to stay fast.
class SpotifyApp : public App {
public:
  explicit SpotifyApp(Preferences* prefs) : _prefs(prefs) {}

  const char* name() const override { return "Spotify"; }

  void onEnter(TFT_eSPI& tft) override;
  void onExit() override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Mode { CONNECTING, NOW_PLAYING, HOST_EDIT, HELP, FAILED };
  Mode _mode = CONNECTING;
  Mode _helpReturnTo = HOST_EDIT;

  Preferences* _prefs;

  static constexpr size_t HOST_MAX = 64;
  char _host[HOST_MAX + 1] = {0};
  size_t _hostLen = 0;
  const char* _failMsg = "";

  static constexpr size_t FIELD_MAX = 48;
  char _track[FIELD_MAX + 1] = {0};
  char _artist[FIELD_MAX + 1] = {0};
  char _album[FIELD_MAX + 1] = {0};
  bool _playing = false;
  bool _hasTrack = false;
  uint32_t _progressMs = 0, _durationMs = 0;

  static constexpr uint32_t POLL_INTERVAL_MS = 3000;
  uint32_t _lastPollAt = 0;

  UI::Button _editBtn{{Cfg::SCREEN_W - 84, Cfg::STATUS_BAR_H + 2, 48, 22}, "Edit"};
  UI::Button _helpBtn{{Cfg::SCREEN_W - 32, Cfg::STATUS_BAR_H + 2, 28, 22}, "?"};
  UI::Button _retryBtn{{Cfg::SCREEN_W / 2 - 60, Cfg::SCREEN_H / 2 + 20, 120, 34}, "Retry"};
  UI::Button _helpBackBtn{{4, Cfg::SCREEN_H - 36, 90, 30}, "Back"};

  static constexpr uint8_t KB_ROWS = 5;
  const char* _kbRows[KB_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", ".:-"};
  UI::Button _kbDelBtn{{4, 0, 100, 26}, "DEL"};
  UI::Button _kbClrBtn{{108, 0, 90, 26}, "CLR"};
  UI::Button _kbDoneBtn{{202, 0, 114, 26}, "Done"};

  bool _dirty = true;

  void loadHost();
  bool poll();
  UI::Rect kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  void drawConnecting(TFT_eSPI& tft);
  void drawFailed(TFT_eSPI& tft);
  void drawDisplay(TFT_eSPI& tft);
  void drawHostEdit(TFT_eSPI& tft);
  void drawHelp(TFT_eSPI& tft);
};
