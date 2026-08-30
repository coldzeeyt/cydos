#include "ObsApp.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ctype.h>
#include "Config.h"

void ObsApp::loadHost() {
  String h = _prefs->getString("obshost", "");
  h.toCharArray(_host, sizeof(_host));
  _hostLen = strlen(_host);
}

// Percent-encodes everything except unreserved characters - scene names
// often have spaces, parens, etc. (e.g. "Gameplay (Main)").
static String urlEncode(const char* s) {
  String out;
  for (const char* p = s; *p; p++) {
    unsigned char c = (unsigned char)*p;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      char buf[4];
      snprintf(buf, sizeof(buf), "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

// Pulls quoted strings out of a flat JSON array like ["A","B (C)"] - not a
// real parser, just enough for the fixed shape cydos_scene_switcher.py
// always returns. Handles \" and \\ escapes; anything else passes through.
static uint8_t parseJsonStringArray(const String& body, char out[][ObsApp::SCENE_NAME_MAX + 1], uint8_t maxItems) {
  uint8_t count = 0;
  size_t i = 0, n = body.length();
  while (i < n && count < maxItems) {
    if (body[i] != '"') { i++; continue; }
    i++; // opening quote
    size_t len = 0;
    while (i < n && body[i] != '"') {
      char c = body[i];
      if (c == '\\' && i + 1 < n) { i++; c = body[i]; }
      if (len < ObsApp::SCENE_NAME_MAX) out[count][len++] = c;
      i++;
    }
    out[count][len] = 0;
    if (i < n) i++; // closing quote
    count++;
  }
  return count;
}

bool ObsApp::fetchScenes() {
  _sceneCount = 0;
  _scenesLoaded = false;
  if (WiFi.status() != WL_CONNECTED || _hostLen == 0) return false;

  HTTPClient http;
  http.setTimeout(3000);
  String url = String("http://") + _host + "/scenes";
  if (!http.begin(url)) return false;

  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String body = http.getString();
    _sceneCount = parseJsonStringArray(body, _sceneNames, NUM_SCENES);
    _scenesLoaded = _sceneCount > 0;
  }
  http.end();
  return _scenesLoaded;
}

void ObsApp::onEnter(TFT_eSPI& tft) {
  loadHost();
  _mode = CONNECTING;
  _sendingIdx = -1;
  _feedbackIdx = -1;
  _sceneCount = 0;
  _scenesLoaded = false;

  UI::clearContent(tft);
  UI::centerText(tft, "Connecting to WiFi...", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 2, Theme::MUTED);

  String ssid = _prefs->getString("wssid", Cfg::WIFI_SSID);
  String pass = _prefs->getString("wpass", Cfg::WIFI_PASSWORD);
  if (ssid.length() == 0) {
    _mode = FAILED;
    _failMsg = "No WiFi configured";
    _dirty = true;
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) delay(50);
  if (WiFi.status() != WL_CONNECTED) {
    _mode = FAILED;
    _failMsg = "WiFi connect failed";
    WiFi.mode(WIFI_OFF);
    _dirty = true;
    return;
  }

  if (_hostLen == 0) {
    _mode = HOST_EDIT;
  } else {
    UI::centerText(tft, "Loading scenes...", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 2, Theme::MUTED);
    fetchScenes(); // best-effort; grid falls back to plain numbers if this fails
    _mode = GRID;
  }
  _dirty = true;
}

void ObsApp::onExit() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool ObsApp::update() {
  if (_feedbackIdx >= 0 && millis() > _feedbackUntil) {
    _feedbackIdx = -1;
    _dirty = true;
  }
  return _dirty;
}

UI::Rect ObsApp::sceneRect(uint8_t i) const {
  uint8_t col = i % COLS;
  uint8_t row = i / COLS;
  int16_t top = Cfg::STATUS_BAR_H + GRID_TOP_GAP;
  int16_t contentH = Cfg::SCREEN_H - top;
  int16_t tw = Cfg::SCREEN_W / COLS;
  int16_t th = contentH / ROWS;
  return {(int16_t)(col * tw), (int16_t)(top + row * th), tw, th};
}

UI::Rect ObsApp::kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 24;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

void ObsApp::sendScene(TFT_eSPI& tft, uint8_t i) {
  if (_hostLen == 0) {
    _mode = HOST_EDIT;
    _dirty = true;
    return;
  }

  _sendingIdx = i;
  _feedbackIdx = -1;
  drawGrid(tft); // paint the "sending" highlight before the blocking request below

  const char* ident = (_scenesLoaded && i < _sceneCount) ? _sceneNames[i] : nullptr;
  char numBuf[4];
  if (!ident) { snprintf(numBuf, sizeof(numBuf), "%d", i + 1); ident = numBuf; }

  bool ok = false;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.setTimeout(3000);
    String url = String("http://") + _host + "/switch?scene=" + urlEncode(ident);
    if (http.begin(url)) {
      int code = http.GET();
      ok = (code >= 200 && code < 300);
      http.end();
    }
  }

  _sendingIdx = -1;
  _feedbackIdx = i;
  _feedbackOk = ok;
  _feedbackUntil = millis() + 600;
  _dirty = true;
}

void ObsApp::drawConnecting(TFT_eSPI& tft) {
  // onEnter already painted this by hand before the blocking WiFi connect
  // and scene fetch; nothing dynamic to redraw here.
}

void ObsApp::drawFailed(TFT_eSPI& tft) {
  UI::clearContent(tft);
  UI::centerText(tft, _failMsg, Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 16, 2, Theme::DANGER);
  _retryBtn.draw(tft);
}

void ObsApp::drawGrid(TFT_eSPI& tft) {
  UI::clearContent(tft);

  tft.setTextColor(_hostLen ? Theme::MUTED : Theme::DANGER, Theme::BG);
  tft.setTextDatum(ML_DATUM);
  tft.drawString(_hostLen ? _host : "no companion host set", 8, Cfg::STATUS_BAR_H + 13, 1);
  tft.setTextDatum(TL_DATUM);
  _refreshBtn.draw(tft);
  _editBtn.draw(tft);

  for (uint8_t i = 0; i < NUM_SCENES; i++) {
    UI::Rect cell = sceneRect(i);
    int16_t pad = 5;
    UI::Rect card{(int16_t)(cell.x + pad), (int16_t)(cell.y + pad), (int16_t)(cell.w - pad * 2), (int16_t)(cell.h - pad * 2)};

    uint16_t border = Theme::PANEL2;
    if (i == _sendingIdx) border = Theme::ACCENT2;
    else if (i == _feedbackIdx) border = _feedbackOk ? Theme::GOOD : Theme::DANGER;

    bool hasName = _scenesLoaded && i < _sceneCount;
    tft.fillRoundRect(card.x, card.y, card.w, card.h, 8, Theme::PANEL);
    tft.drawRoundRect(card.x, card.y, card.w, card.h, 8, border);

    tft.setTextColor(hasName ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
    tft.setTextDatum(MC_DATUM);
    if (hasName) {
      tft.drawString(_sceneNames[i], card.x + card.w / 2, card.y + card.h / 2 + 1, 1);
    } else {
      char label[4];
      snprintf(label, sizeof(label), "%d", i + 1);
      tft.drawString(label, card.x + card.w / 2, card.y + card.h / 2 + 1, 4);
    }
    tft.setTextDatum(TL_DATUM);
  }
}

void ObsApp::drawHostEdit(TFT_eSPI& tft) {
  UI::clearContent(tft);

  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, Cfg::SCREEN_W - 8, 24, 4, Theme::PANEL);
  tft.setTextColor(_hostLen ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  const char* shown = _hostLen > 34 ? _host + (_hostLen - 34) : _host;
  tft.drawString(_hostLen ? shown : "host:port, e.g. 192.168.1.50:8088", 10, Cfg::STATUS_BAR_H + 14, 2);
  tft.setTextDatum(TL_DATUM);

  for (uint8_t r = 0; r < KB_ROWS; r++) {
    uint8_t len = strlen(_kbRows[r]);
    for (uint8_t c = 0; c < len; c++) {
      UI::Rect kr = kbKeyRect(r, c, len);
      char label[2] = {_kbRows[r][c], 0};
      tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 4, Theme::PANEL);
      tft.setTextColor(Theme::TEXT, Theme::PANEL);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  int16_t ctrlY = Cfg::STATUS_BAR_H + 30 + KB_ROWS * 24 + 4;
  _kbDelBtn.r.y = ctrlY;
  _kbClrBtn.r.y = ctrlY;
  _kbDoneBtn.r.y = ctrlY;
  _kbDelBtn.draw(tft);
  _kbClrBtn.draw(tft);
  _kbDoneBtn.draw(tft);
}

void ObsApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  switch (_mode) {
    case CONNECTING: drawConnecting(tft); break;
    case FAILED: drawFailed(tft); break;
    case GRID: drawGrid(tft); break;
    case HOST_EDIT: drawHostEdit(tft); break;
  }
}

void ObsApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;

  if (_mode == FAILED) {
    if (_retryBtn.hit(x, y)) onEnter(tft);
    return;
  }

  if (_mode == HOST_EDIT) {
    for (uint8_t r = 0; r < KB_ROWS; r++) {
      uint8_t len = strlen(_kbRows[r]);
      for (uint8_t c = 0; c < len; c++) {
        if (kbKeyRect(r, c, len).contains(x, y) && _hostLen < HOST_MAX) {
          _host[_hostLen++] = _kbRows[r][c];
          _host[_hostLen] = 0;
          _dirty = true;
          return;
        }
      }
    }
    if (_kbDelBtn.hit(x, y) && _hostLen > 0) { _host[--_hostLen] = 0; _dirty = true; }
    else if (_kbClrBtn.hit(x, y)) { _hostLen = 0; _host[0] = 0; _dirty = true; }
    else if (_kbDoneBtn.hit(x, y)) {
      _prefs->putString("obshost", _host);
      if (_hostLen > 0) { fetchScenes(); }
      _mode = GRID;
      _dirty = true;
    }
    return;
  }

  // GRID
  if (_editBtn.hit(x, y)) { _mode = HOST_EDIT; _dirty = true; return; }
  if (_refreshBtn.hit(x, y)) { fetchScenes(); _dirty = true; return; }
  for (uint8_t i = 0; i < NUM_SCENES; i++) {
    if (sceneRect(i).contains(x, y)) { sendScene(tft, i); return; }
  }
}
