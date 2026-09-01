#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"

// Remote control for audio playing on your PC - the CYD has no speaker or
// audio output pin of any kind, so playback has to happen somewhere that
// does. Talks to the companion script in
// pc-companion/cydos_media_player.py (`pip install pygame`, then
// `python3 cydos_media_player.py --dir /path/to/music`) over plain HTTP
// on your LAN:
//   GET /tracks             -> {"tracks": [name, ...]}
//   GET /play?track=NAME    -> starts NAME playing
//   GET /pause /resume /stop /next /prev
//   GET /volume?level=0-100
//   GET /status              -> {"playing": bool, "track": str, "volume": int}
// WiFi is connected once on entry and held open while this app is open,
// same tradeoff OBS/Spotify make, for the same reason (fast repeated
// polling/control).
class MediaPlayerApp : public App {
public:
  explicit MediaPlayerApp(Preferences* prefs) : _prefs(prefs) {}

  const char* name() const override { return "Media Player"; }

  static constexpr uint8_t MAX_TRACKS = 60;
  static constexpr uint8_t NAME_LEN = 40;

  void onEnter(TFT_eSPI& tft) override;
  void onExit() override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;
  void onTouchUp() override { _draggingVolume = false; }

private:
  enum Mode { CONNECTING, LIST, PLAYER, HOST_EDIT, HELP, FAILED };
  Mode _mode = CONNECTING;
  Mode _helpReturnTo = HOST_EDIT;

  Preferences* _prefs;

  static constexpr size_t HOST_MAX = 64;
  char _host[HOST_MAX + 1] = {0};
  size_t _hostLen = 0;
  const char* _failMsg = "";

  char _tracks[MAX_TRACKS][NAME_LEN];
  uint8_t _trackCount = 0;
  bool _tracksLoaded = false;

  static constexpr uint8_t ROWS_PER_PAGE = 6;
  uint8_t _listPage = 0;

  char _currentTrack[NAME_LEN] = {0};
  bool _playing = false;
  int _volume = 70;

  static constexpr uint32_t POLL_INTERVAL_MS = 2000;
  uint32_t _lastPollAt = 0;

  UI::Button _editBtn{{Cfg::SCREEN_W - 84, Cfg::STATUS_BAR_H + 2, 48, 22}, "Edit"};
  UI::Button _helpBtn{{Cfg::SCREEN_W - 32, Cfg::STATUS_BAR_H + 2, 28, 22}, "?"};
  UI::Button _playerBtn{{4, Cfg::STATUS_BAR_H + 2, 90, 22}, "Now Playing"};
  UI::Button _tracksBtn{{4, Cfg::STATUS_BAR_H + 2, 70, 22}, "Tracks"};
  UI::Button _retryBtn{{Cfg::SCREEN_W / 2 - 60, Cfg::SCREEN_H / 2 + 20, 120, 34}, "Retry"};
  UI::Button _helpBackBtn{{4, Cfg::SCREEN_H - 36, 90, 30}, "Back"};

  UI::Button _prevBtn{{20, 0, 60, 30}, "Prev"};
  UI::Button _playPauseBtn{{90, 0, 140, 30}, "Play"};
  UI::Button _nextBtn{{240, 0, 60, 30}, "Next"};
  UI::Button _stopBtn{{Cfg::SCREEN_W / 2 - 40, 0, 80, 24}, "Stop"};
  UI::Slider _volumeSlider{{40, 0, Cfg::SCREEN_W - 80, 22}, 70};

  UI::Button _listPrevBtn{{40, Cfg::STATUS_BAR_H + 184, 60, 22}, "<"};
  UI::Button _listNextBtn{{(int16_t)(Cfg::SCREEN_W - 100), Cfg::STATUS_BAR_H + 184, 60, 22}, ">"};

  static constexpr uint8_t KB_ROWS = 5;
  const char* _kbRows[KB_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", ".:-"};
  UI::Button _kbDelBtn{{4, 0, 100, 26}, "DEL"};
  UI::Button _kbClrBtn{{108, 0, 90, 26}, "CLR"};
  UI::Button _kbDoneBtn{{202, 0, 114, 26}, "Done"};

  bool _draggingVolume = false;
  bool _dirty = true;

  void loadHost();
  bool fetchTracks();
  bool fetchStatus();
  bool sendCommand(const char* path);
  bool sendPlay(const char* track);
  uint8_t listPageCount() const { return _trackCount == 0 ? 0 : (_trackCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE; }
  UI::Rect kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  UI::Rect trackRowRect(uint8_t rowInPage) const;

  void drawConnecting(TFT_eSPI& tft);
  void drawFailed(TFT_eSPI& tft);
  void drawList(TFT_eSPI& tft);
  void drawPlayer(TFT_eSPI& tft);
  void drawHostEdit(TFT_eSPI& tft);
  void drawHelp(TFT_eSPI& tft);
};
