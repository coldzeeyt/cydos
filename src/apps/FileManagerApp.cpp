#include "FileManagerApp.h"
#include "core/SdCard.h"
#include <SD.h>
#include <strings.h>

static void joinPath(char* out, size_t outSize, const char* dir, const char* name) {
  if (strcmp(dir, "/") == 0) snprintf(out, outSize, "/%s", name);
  else snprintf(out, outSize, "%s/%s", dir, name);
}

bool FileManagerApp::entryLess(const Entry& a, const Entry& b) {
  if (a.isDir != b.isDir) return a.isDir;
  return strcasecmp(a.name, b.name) < 0;
}

void FileManagerApp::onEnter(TFT_eSPI& tft) {
  _mode = BROWSE;
  refresh();
}

bool FileManagerApp::update() {
  if (_statusMsg[0] && (int32_t)(millis() - _statusUntil) >= 0) {
    _statusMsg[0] = 0;
    return true;
  }
  return false;
}

void FileManagerApp::refresh() {
  _entryCount = 0;
  _page = 0;
  _selected = -1;
  _dirty = true;
  if (!_sd || !_sd->available()) return;

  SdCard::useSdBus();
  File dir = SD.open(_path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    SdCard::useTouchBus();
    return;
  }

  File f = dir.openNextFile();
  while (f && _entryCount < MAX_ENTRIES) {
    String fname = f.name();
    int slash = fname.lastIndexOf('/');
    String base = slash >= 0 ? fname.substring(slash + 1) : fname;
    if (base.length() > 0) {
      base.toCharArray(_entries[_entryCount].name, NAME_LEN);
      _entries[_entryCount].isDir = f.isDirectory();
      _entries[_entryCount].size = f.isDirectory() ? 0 : f.size();
      _entryCount++;
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  SdCard::useTouchBus();

  // Insertion sort: directories first, then alphabetical - MAX_ENTRIES
  // caps this well below where an O(n^2) sort would matter.
  for (uint8_t i = 1; i < _entryCount; i++) {
    Entry key = _entries[i];
    int8_t j = (int8_t)i - 1;
    while (j >= 0 && entryLess(key, _entries[j])) {
      _entries[j + 1] = _entries[j];
      j--;
    }
    _entries[j + 1] = key;
  }
}

void FileManagerApp::goUp() {
  if (strcmp(_path, "/") == 0) return;
  char* slash = strrchr(_path, '/');
  if (slash == _path) _path[1] = 0;
  else if (slash) *slash = 0;
  refresh();
}

void FileManagerApp::enterDir(const char* childName) {
  char newPath[PATH_LEN];
  joinPath(newPath, sizeof(newPath), _path, childName);
  strncpy(_path, newPath, sizeof(_path) - 1);
  _path[sizeof(_path) - 1] = 0;
  refresh();
}

uint8_t FileManagerApp::pageCount() const {
  return _entryCount == 0 ? 0 : (_entryCount + ROWS_PER_PAGE - 1) / ROWS_PER_PAGE;
}

UI::Rect FileManagerApp::rowRect(uint8_t rowInPage) const {
  int16_t y = Cfg::STATUS_BAR_H + 24 + rowInPage * 26;
  return {4, y, (int16_t)(Cfg::SCREEN_W - 8), 24};
}

int8_t FileManagerApp::rowAt(int16_t x, int16_t y) const {
  uint8_t base = _page * ROWS_PER_PAGE;
  for (uint8_t i = 0; i < ROWS_PER_PAGE; i++) {
    uint8_t idx = base + i;
    if (idx >= _entryCount) break;
    if (rowRect(i).contains(x, y)) return (int8_t)idx;
  }
  return -1;
}

void FileManagerApp::setStatus(const char* msg) {
  strncpy(_statusMsg, msg, sizeof(_statusMsg) - 1);
  _statusMsg[sizeof(_statusMsg) - 1] = 0;
  _statusUntil = millis() + 2000;
  _dirty = true;
}

void FileManagerApp::drawBrowse(TFT_eSPI& tft) {
  UI::clearContent(tft);

  if (!_sd || !_sd->available()) {
    UI::centerText(tft, "No SD Card", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 10, 2, Theme::MUTED);
    UI::centerText(tft, "Insert one and restart CydOs", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 14, 1, Theme::MUTED);
    return;
  }

  char shownPath[36];
  size_t plen = strlen(_path);
  if (plen > 34) snprintf(shownPath, sizeof(shownPath), "...%s", _path + (plen - 31));
  else { strncpy(shownPath, _path, sizeof(shownPath) - 1); shownPath[sizeof(shownPath) - 1] = 0; }

  tft.setTextColor(Theme::MUTED, Theme::BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_statusMsg[0] ? _statusMsg : shownPath, 8, Cfg::STATUS_BAR_H + 10, 1);
  tft.setTextDatum(TL_DATUM);

  uint8_t pc = pageCount();
  char pageBuf[12];
  snprintf(pageBuf, sizeof(pageBuf), "%d/%d", pc ? _page + 1 : 0, pc);
  tft.setTextColor(Theme::MUTED, Theme::BG);
  tft.setTextDatum(TR_DATUM);
  tft.drawString(pageBuf, Cfg::SCREEN_W - 6, Cfg::STATUS_BAR_H + 10, 1);
  tft.setTextDatum(TL_DATUM);

  if (_entryCount == 0) {
    UI::centerText(tft, "(empty)", Cfg::SCREEN_W / 2, 120, 1, Theme::MUTED);
  }

  uint8_t base = _page * ROWS_PER_PAGE;
  for (uint8_t i = 0; i < ROWS_PER_PAGE; i++) {
    uint8_t idx = base + i;
    if (idx >= _entryCount) break;
    UI::Rect r = rowRect(i);
    Entry& e = _entries[idx];
    bool sel = (_selected == (int8_t)idx);
    uint16_t bg = sel ? Theme::ACCENT : Theme::PANEL;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
    uint16_t txtCol = sel ? Theme::BG : (e.isDir ? Theme::ACCENT2 : Theme::TEXT);
    tft.setTextColor(txtCol, bg);
    tft.setTextDatum(ML_DATUM);
    char label[NAME_LEN + 4];
    snprintf(label, sizeof(label), "%s%s", e.isDir ? "> " : "  ", e.name);
    tft.drawString(label, r.x + 8, r.y + r.h / 2 + 1, 2);
    if (!e.isDir) {
      char sizeBuf[12];
      if (e.size < 1024UL) snprintf(sizeBuf, sizeof(sizeBuf), "%luB", (unsigned long)e.size);
      else if (e.size < 1024UL * 1024UL) snprintf(sizeBuf, sizeof(sizeBuf), "%.1fKB", e.size / 1024.0f);
      else snprintf(sizeBuf, sizeof(sizeBuf), "%.1fMB", e.size / (1024.0f * 1024.0f));
      tft.setTextDatum(MR_DATUM);
      tft.drawString(sizeBuf, r.x + r.w - 8, r.y + r.h / 2 + 1, 1);
    }
    tft.setTextDatum(TL_DATUM);
  }

  bool atRoot = strcmp(_path, "/") == 0;
  _upBtn.textColor = atRoot ? Theme::MUTED : Theme::TEXT;
  _upBtn.draw(tft);
  _prevBtn.draw(tft);
  _nextBtn.draw(tft);
  _deleteBtn.active = _selected >= 0;
  _deleteBtn.draw(tft);
}

void FileManagerApp::drawConfirm(TFT_eSPI& tft) {
  UI::clearContent(tft);
  Entry& e = _entries[_selected];
  char msg[48];
  snprintf(msg, sizeof(msg), "Delete \"%s\"?", e.name);
  UI::centerText(tft, msg, Cfg::SCREEN_W / 2, 100, 2, Theme::TEXT);
  UI::centerText(tft, e.isDir ? "(must be empty)" : "This can't be undone.", Cfg::SCREEN_W / 2, 122, 1, Theme::MUTED);
  _confirmYesBtn.draw(tft);
  _confirmNoBtn.draw(tft);
}

void FileManagerApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  if (_mode == CONFIRM_DELETE) drawConfirm(tft);
  else drawBrowse(tft);
}

void FileManagerApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;
  if (!_sd || !_sd->available()) return;

  if (_mode == CONFIRM_DELETE) {
    if (_confirmYesBtn.hit(x, y)) {
      char full[PATH_LEN];
      joinPath(full, sizeof(full), _path, _entries[_selected].name);
      bool isDir = _entries[_selected].isDir;
      SdCard::useSdBus();
      bool ok = isDir ? SD.rmdir(full) : SD.remove(full);
      SdCard::useTouchBus();
      _mode = BROWSE;
      refresh(); // re-points the bus at SD itself, see SdCard.h
      setStatus(ok ? "Deleted." : (isDir ? "Folder not empty." : "Delete failed."));
      return;
    }
    if (_confirmNoBtn.hit(x, y)) { _mode = BROWSE; _dirty = true; }
    return;
  }

  // BROWSE
  if (_upBtn.hit(x, y)) { goUp(); return; }
  if (_prevBtn.hit(x, y)) { if (_page > 0) { _page--; _selected = -1; _dirty = true; } return; }
  if (_nextBtn.hit(x, y)) { if (_page + 1 < pageCount()) { _page++; _selected = -1; _dirty = true; } return; }
  if (_deleteBtn.hit(x, y)) { if (_selected >= 0) { _mode = CONFIRM_DELETE; _dirty = true; } return; }

  int8_t row = rowAt(x, y);
  if (row >= 0) {
    if (_entries[row].isDir) enterDir(_entries[row].name);
    else { _selected = (_selected == row) ? (int8_t)-1 : row; _dirty = true; }
  }
}
