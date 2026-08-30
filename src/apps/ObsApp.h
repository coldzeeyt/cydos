#pragma once
#include <Preferences.h>
#include "App.h"
#include "core/UI.h"

// A 3x3 grid of real OBS scenes. Talks to the companion OBS script in
// obs-script/cydos_scene_switcher.py (Tools > Scripts in OBS, no
// compiling) over plain HTTP on your LAN:
//   GET /scenes           -> JSON array of scene names, populates the grid
//   GET /switch?scene=X   -> switches to it; the ESP32 sends the real name
// WiFi is connected once on entry and held open the whole time you're in
// this app (unlike Browser/WiFi Radar) so switching scenes stays fast.
class ObsApp : public App {
public:
  explicit ObsApp(Preferences* prefs) : _prefs(prefs) {}

  const char* name() const override { return "OBS"; }

  static constexpr uint8_t SCENE_NAME_MAX = 22;

  void onEnter(TFT_eSPI& tft) override;
  void onExit() override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  enum Mode { CONNECTING, GRID, HOST_EDIT, HELP, FAILED };
  Mode _mode = CONNECTING;
  Mode _helpReturnTo = HOST_EDIT;

  Preferences* _prefs;

  static constexpr size_t HOST_MAX = 64;
  char _host[HOST_MAX + 1] = {0};
  size_t _hostLen = 0;
  const char* _failMsg = "";

  static constexpr uint8_t COLS = 3;
  static constexpr uint8_t ROWS = 3;
  static constexpr uint8_t NUM_SCENES = COLS * ROWS;
  static constexpr int16_t GRID_TOP_GAP = 26; // room for the host bar above the grid

  char _sceneNames[NUM_SCENES][SCENE_NAME_MAX + 1];
  uint8_t _sceneCount = 0;
  bool _scenesLoaded = false; // false => grid falls back to plain numbers

  UI::Button _refreshBtn{{Cfg::SCREEN_W - 128, Cfg::STATUS_BAR_H + 2, 44, 22}, "Sync"};
  UI::Button _editBtn{{Cfg::SCREEN_W - 80, Cfg::STATUS_BAR_H + 2, 44, 22}, "Edit"};
  UI::Button _helpBtn{{Cfg::SCREEN_W - 32, Cfg::STATUS_BAR_H + 2, 28, 22}, "?"};
  UI::Button _retryBtn{{Cfg::SCREEN_W / 2 - 60, Cfg::SCREEN_H / 2 + 20, 120, 34}, "Retry"};
  UI::Button _helpBackBtn{{4, Cfg::SCREEN_H - 36, 90, 30}, "Back"};

  int8_t _sendingIdx = -1;   // scene currently mid-request (blocking, but drawn first)
  int8_t _feedbackIdx = -1;  // scene showing a just-finished result
  bool _feedbackOk = false;
  uint32_t _feedbackUntil = 0;

  static constexpr uint8_t KB_ROWS = 5;
  const char* _kbRows[KB_ROWS] = {"1234567890", "qwertyuiop", "asdfghjkl", "zxcvbnm", ".:-"};
  UI::Button _kbDelBtn{{4, 0, 100, 26}, "DEL"};
  UI::Button _kbClrBtn{{108, 0, 90, 26}, "CLR"};
  UI::Button _kbDoneBtn{{202, 0, 114, 26}, "Done"};

  bool _dirty = true;

  void loadHost();
  bool fetchScenes();
  UI::Rect sceneRect(uint8_t i) const;
  UI::Rect kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const;
  void sendScene(TFT_eSPI& tft, uint8_t i);
  void drawConnecting(TFT_eSPI& tft);
  void drawFailed(TFT_eSPI& tft);
  void drawGrid(TFT_eSPI& tft);
  void drawHostEdit(TFT_eSPI& tft);
  void drawHelp(TFT_eSPI& tft);
};
