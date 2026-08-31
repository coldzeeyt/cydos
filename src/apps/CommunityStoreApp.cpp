#include "CommunityStoreApp.h"
#include "core/AppManager.h"
#include "core/AppStore.h"
#include "core/SdCard.h"
#include "core/SdAppPool.h"
#include "Config.h"
#include <SD.h>
#include <WiFi.h>

UI::Rect CommunityStoreApp::tileRect(uint8_t i) const {
  uint8_t col = i % COLS;
  uint8_t row = i / COLS;
  int16_t top = Cfg::STATUS_BAR_H + 24;
  int16_t contentH = Cfg::SCREEN_H - top;
  int16_t tw = Cfg::SCREEN_W / COLS;
  int16_t th = contentH / ROWS;
  return {(int16_t)(col * tw), (int16_t)(top + row * th), tw, th};
}

int8_t CommunityStoreApp::tileAt(int16_t x, int16_t y) const {
  for (uint8_t i = 0; i < _tileCount; i++) {
    if (tileRect(i).contains(x, y)) return (int8_t)i;
  }
  return -1;
}

UI::Rect CommunityStoreApp::tabRect(uint8_t i) const {
  int16_t y = Cfg::STATUS_BAR_H + 2, h = 20;
  int16_t w = Cfg::SCREEN_W / 2 - 3;
  if (i == 0) return {2, y, w, h};
  return {(int16_t)(Cfg::SCREEN_W / 2 + 1), y, w, h};
}

UI::Rect CommunityStoreApp::browseRowRect(uint8_t rowInPage) const {
  return {4, (int16_t)(Cfg::STATUS_BAR_H + 28 + rowInPage * 26), (int16_t)(Cfg::SCREEN_W - 8), 24};
}

uint8_t CommunityStoreApp::browsePageCount() const {
  return _itemCount == 0 ? 0 : (_itemCount + BROWSE_ROWS_PER_PAGE - 1) / BROWSE_ROWS_PER_PAGE;
}

void CommunityStoreApp::onEnter(TFT_eSPI& tft) {
  _mode = INSTALLED;
  _dirty = true;
}

void CommunityStoreApp::onExit() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool CommunityStoreApp::connectWifi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  String ssid = _prefs->getString("wssid", Cfg::WIFI_SSID);
  String pass = _prefs->getString("wpass", Cfg::WIFI_PASSWORD);
  if (ssid.length() == 0) return false;

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) delay(50);
  return WiFi.status() == WL_CONNECTED;
}

// Blocking (network round-trips) - only ever called from onTouch(), same
// tradeoff ObsApp::sendScene() makes, never from update()/draw().
void CommunityStoreApp::loadBrowseCatalog(TFT_eSPI& tft) {
  UI::clearContent(tft);
  drawTabBar(tft);
  UI::centerText(tft, "Connecting to WiFi...", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 1, Theme::MUTED);

  if (!_sd || !_sd->available() || !connectWifi()) {
    _browseState = FAILED;
    return;
  }

  char names[MAX_BROWSE_ITEMS][NAME_LEN];
  char paths[MAX_BROWSE_ITEMS][PATH_LEN];
  _itemCount = 0;

  auto addItems = [&](const char* manifestPath, bool isApp, const char* destDir) {
    uint8_t n = AppStore::fetchManifest(manifestPath, names, paths, MAX_BROWSE_ITEMS - _itemCount);
    for (uint8_t i = 0; i < n && _itemCount < MAX_BROWSE_ITEMS; i++) {
      BrowseItem& item = _items[_itemCount];
      strncpy(item.name, names[i], NAME_LEN - 1); item.name[NAME_LEN - 1] = 0;
      strncpy(item.urlPath, paths[i], PATH_LEN - 1); item.urlPath[PATH_LEN - 1] = 0;
      item.isApp = isApp;
      const char* base = strrchr(item.urlPath, '/');
      base = base ? base + 1 : item.urlPath;
      char full[64];
      snprintf(full, sizeof(full), "%s/%s", destDir, base);
      item.installed = SD.exists(full);
      _itemCount++;
    }
    return n;
  };

  SdCard::useSdBus();
  uint8_t appCount = addItems("/ondevice_apps.txt", true, "/cydos_apps");
  uint8_t wpCount = addItems("/ondevice_wallpapers.txt", false, "/cydos_wallpapers");
  SdCard::useTouchBus();

  _browsePage = 0;
  _browseState = (appCount == 0 && wpCount == 0) ? FAILED : LOADED;
}

void CommunityStoreApp::downloadItem(TFT_eSPI& tft, uint8_t idx) {
  BrowseItem& item = _items[idx];
  const char* base = strrchr(item.urlPath, '/');
  base = base ? base + 1 : item.urlPath;
  const char* destDir = item.isApp ? "/cydos_apps" : "/cydos_wallpapers";
  SdCard::useSdBus();
  if (!SD.exists(destDir)) SD.mkdir(destDir);
  SdCard::useTouchBus();
  char destPath[64];
  snprintf(destPath, sizeof(destPath), "%s/%s", destDir, base);

  UI::clearContent(tft);
  drawTabBar(tft);
  char label[48];
  snprintf(label, sizeof(label), "Downloading %s...", item.name);
  bool ok = AppStore::downloadToSd(tft, item.urlPath, destPath, label); // re-points the bus itself, see SdCard.h

  if (ok) {
    item.installed = true;
    if (item.isApp && _sdAppPool) _sdAppPool->rescan();
  } else {
    UI::centerText(tft, "Download failed - check WiFi", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 40, 1, Theme::DANGER);
    delay(1200);
  }
  _dirty = true;
}

void CommunityStoreApp::drawTabBar(TFT_eSPI& tft) {
  static const char* LABELS[2] = {"Installed", "Get More"};
  for (uint8_t i = 0; i < 2; i++) {
    UI::Rect r = tabRect(i);
    bool sel = ((uint8_t)_mode == i);
    uint16_t bg = sel ? Theme::ACCENT : Theme::PANEL;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
    tft.setTextColor(sel ? Theme::BG : Theme::TEXT, bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(LABELS[i], r.x + r.w / 2, r.y + r.h / 2 + 1, 1);
    tft.setTextDatum(TL_DATUM);
  }
}

void CommunityStoreApp::drawInstalled(TFT_eSPI& tft) {
  if (_tileCount == 0) {
    UI::centerText(tft, "No community apps installed", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 10, 1, Theme::MUTED);
    UI::centerText(tft, "Try Get More, or flash a Community Edition build", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 10, 1, Theme::MUTED);
    return;
  }

  for (uint8_t i = 0; i < _tileCount; i++) {
    UI::Rect cell = tileRect(i);
    App* app = _mgr->appAt(_appIndex[i]);
    int16_t pad = 5;
    UI::Rect card{(int16_t)(cell.x + pad), (int16_t)(cell.y + pad), (int16_t)(cell.w - pad * 2), (int16_t)(cell.h - pad * 2)};
    tft.fillRoundRect(card.x, card.y, card.w, card.h, 8, Theme::PANEL);
    tft.drawRoundRect(card.x, card.y, card.w, card.h, 8, Theme::PANEL2);
    int16_t cx = card.x + card.w / 2;
    int16_t cy = card.y + card.h / 2 - 8;
    if (_icons[i]) _icons[i](tft, cx, cy, Theme::ACCENT);
    tft.setTextColor(Theme::TEXT, Theme::PANEL);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(app ? app->name() : "?", cx, card.y + card.h - 13, 1);
    tft.setTextDatum(TL_DATUM);
  }
}

void CommunityStoreApp::drawBrowse(TFT_eSPI& tft) {
  if (_browseState == FAILED) {
    UI::centerText(tft, "Couldn't reach the App Store", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 10, 1, Theme::DANGER);
    UI::centerText(tft, "Check WiFi in Settings", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 10, 1, Theme::MUTED);
    return;
  }
  if (_browseState == NOT_LOADED) return; // loadBrowseCatalog() runs from onTouch(), not here

  if (_itemCount == 0) {
    UI::centerText(tft, "Nothing to download yet", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 1, Theme::MUTED);
    return;
  }

  uint8_t base = _browsePage * BROWSE_ROWS_PER_PAGE;
  for (uint8_t row = 0; row < BROWSE_ROWS_PER_PAGE; row++) {
    uint8_t i = base + row;
    if (i >= _itemCount) break;
    UI::Rect r = browseRowRect(row);
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, Theme::PANEL);
    tft.setTextColor(Theme::TEXT, Theme::PANEL);
    tft.setTextDatum(ML_DATUM);
    char label[40];
    snprintf(label, sizeof(label), "%s %s", _items[i].isApp ? "[App]" : "[Wall]", _items[i].name);
    tft.drawString(label, r.x + 8, r.y + r.h / 2 + 1, 1);
    tft.setTextDatum(TL_DATUM);

    UI::Rect btn{(int16_t)(r.x + r.w - 66), r.y, 62, r.h};
    uint16_t btnBg = _items[i].installed ? Theme::GOOD : Theme::ACCENT;
    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 4, btnBg);
    tft.setTextColor(Theme::BG, btnBg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(_items[i].installed ? "Done" : "Get", btn.x + btn.w / 2, btn.y + btn.h / 2 + 1, 1);
    tft.setTextDatum(TL_DATUM);
  }

  uint8_t pc = browsePageCount();
  if (pc > 1) {
    _browsePrevBtn.textColor = _browsePage > 0 ? Theme::TEXT : Theme::MUTED;
    _browseNextBtn.textColor = (_browsePage + 1 < pc) ? Theme::TEXT : Theme::MUTED;
    _browsePrevBtn.draw(tft);
    _browseNextBtn.draw(tft);
    char pageBuf[12];
    snprintf(pageBuf, sizeof(pageBuf), "%d/%d", _browsePage + 1, pc);
    UI::centerText(tft, pageBuf, Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 195, 1, Theme::MUTED);
  }
}

void CommunityStoreApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  UI::clearContent(tft);
  drawTabBar(tft);
  if (_mode == INSTALLED) drawInstalled(tft);
  else drawBrowse(tft);
}

void CommunityStoreApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  for (uint8_t i = 0; i < 2; i++) {
    if (tabRect(i).contains(x, y)) {
      _mode = (Mode)i;
      // Retries on every tap until it actually succeeds, not just the
      // first one - lets you fix WiFi in Settings and come straight back
      // without leaving and re-entering the whole app.
      if (_mode == BROWSE && _browseState != LOADED) loadBrowseCatalog(tft);
      _dirty = true;
      return;
    }
  }

  if (_mode == INSTALLED) {
    int8_t idx = tileAt(x, y);
    if (idx >= 0) _mgr->openApp(_appIndex[idx]);
    return;
  }

  // BROWSE
  if (_browseState != LOADED) return;

  uint8_t pc = browsePageCount();
  if (pc > 1) {
    if (_browsePrevBtn.hit(x, y)) {
      if (_browsePage > 0) { _browsePage--; _dirty = true; }
      return;
    }
    if (_browseNextBtn.hit(x, y)) {
      if (_browsePage + 1 < pc) { _browsePage++; _dirty = true; }
      return;
    }
  }

  uint8_t base = _browsePage * BROWSE_ROWS_PER_PAGE;
  for (uint8_t row = 0; row < BROWSE_ROWS_PER_PAGE; row++) {
    uint8_t i = base + row;
    if (i >= _itemCount) break;
    UI::Rect r = browseRowRect(row);
    UI::Rect btn{(int16_t)(r.x + r.w - 66), r.y, 62, r.h};
    if (btn.contains(x, y) && !_items[i].installed) {
      downloadItem(tft, i);
      return;
    }
  }
}
