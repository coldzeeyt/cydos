#pragma once
#include "App.h"
#include "core/UI.h"

class SdCard;

// A plain SD-card file browser: navigate directories, see file sizes, and
// delete files or empty folders. Read-only beyond delete - no create/move/
// rename, since there's no keyboard-free way to name something new that's
// worth the screen space this device has.
class FileManagerApp : public App {
public:
  explicit FileManagerApp(SdCard* sd) : _sd(sd) {}

  const char* name() const override { return "Files"; }

  void onEnter(TFT_eSPI& tft) override;
  bool update() override;
  void draw(TFT_eSPI& tft) override;
  void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) override;

private:
  static constexpr uint8_t MAX_ENTRIES = 48;
  static constexpr uint8_t NAME_LEN = 28;
  static constexpr uint8_t ROWS_PER_PAGE = 6;
  static constexpr uint16_t PATH_LEN = 128;

  struct Entry {
    char name[NAME_LEN];
    bool isDir;
    uint32_t size;
  };

  enum Mode { BROWSE, CONFIRM_DELETE };

  SdCard* _sd;
  char _path[PATH_LEN] = "/";
  Entry _entries[MAX_ENTRIES];
  uint8_t _entryCount = 0;
  uint8_t _page = 0;
  int8_t _selected = -1;
  Mode _mode = BROWSE;

  char _statusMsg[40] = {0};
  uint32_t _statusUntil = 0;
  bool _dirty = true;

  UI::Button _upBtn{{6, Cfg::STATUS_BAR_H + 180, 60, 30}, "Up"};
  UI::Button _prevBtn{{72, Cfg::STATUS_BAR_H + 180, 50, 30}, "<"};
  UI::Button _nextBtn{{128, Cfg::STATUS_BAR_H + 180, 50, 30}, ">"};
  UI::Button _deleteBtn{{184, Cfg::STATUS_BAR_H + 180, 130, 30}, "Delete"};
  UI::Button _confirmYesBtn{{60, 130, 90, 34}, "Delete", Theme::DANGER};
  UI::Button _confirmNoBtn{{170, 130, 90, 34}, "Cancel"};

  static bool entryLess(const Entry& a, const Entry& b);

  void refresh();
  void goUp();
  void enterDir(const char* childName);
  uint8_t pageCount() const;
  UI::Rect rowRect(uint8_t rowInPage) const;
  int8_t rowAt(int16_t x, int16_t y) const;
  void setStatus(const char* msg);
  void drawBrowse(TFT_eSPI& tft);
  void drawConfirm(TFT_eSPI& tft);
};
