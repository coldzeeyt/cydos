#include "BrowserApp.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ctype.h>
#include "Config.h"

void BrowserApp::onEnter(TFT_eSPI& tft) {
  _mode = URL_EDIT;
  _dirty = true;
}

void BrowserApp::onExit() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void BrowserApp::appendPlain(const char* s) {
  for (size_t i = 0; s[i] && _textLen < TEXT_MAX; i++) _text[_textLen++] = s[i];
}

// A tag/script/style-stripping, entity-decoding, whitespace-collapsing
// pass over one chunk of downloaded HTML. Not a parser - just enough to
// turn a typical page into readable text. Entities split across a chunk
// boundary are shown literally rather than decoded; a cosmetic edge case.
void BrowserApp::stripChunk(const char* buf, size_t n) {
  for (size_t i = 0; i < n && _textLen < TEXT_MAX; i++) {
    char c = buf[i];

    if (_stripInTag) {
      if (c == '>') _stripInTag = false;
      continue;
    }

    if (c == '<') {
      _stripInTag = true;
      bool closing = (i + 1 < n && buf[i + 1] == '/');
      size_t j = i + 1 + (closing ? 1 : 0);
      char name[8];
      uint8_t len = 0;
      while (j < n && len < 7 && isalnum((unsigned char)buf[j])) name[len++] = tolower(buf[j++]);
      name[len] = 0;
      if (closing) {
        if (!strcmp(name, "script")) _stripInScript = false;
        else if (!strcmp(name, "style")) _stripInStyle = false;
        else if (!strcmp(name, "p") || !strcmp(name, "div") || !strcmp(name, "li") ||
                 !strcmp(name, "tr") || !strcmp(name, "h1") || !strcmp(name, "h2") ||
                 !strcmp(name, "h3")) {
          if (!_stripLastSpace && _textLen < TEXT_MAX) { _text[_textLen++] = '\n'; _stripLastSpace = true; }
        }
      } else {
        if (!strcmp(name, "script")) _stripInScript = true;
        else if (!strcmp(name, "style")) _stripInStyle = true;
        else if (!strcmp(name, "br")) { if (_textLen < TEXT_MAX) { _text[_textLen++] = '\n'; _stripLastSpace = true; } }
      }
      continue;
    }

    if (_stripInScript || _stripInStyle) continue;

    if (c == '&') {
      size_t j = i + 1;
      char ent[10];
      uint8_t el = 0;
      while (j < n && buf[j] != ';' && el < 9) ent[el++] = buf[j++];
      if (j < n && buf[j] == ';') {
        ent[el] = 0;
        char decoded = 0;
        bool known = true;
        if (!strcmp(ent, "amp")) decoded = '&';
        else if (!strcmp(ent, "lt")) decoded = '<';
        else if (!strcmp(ent, "gt")) decoded = '>';
        else if (!strcmp(ent, "quot")) decoded = '"';
        else if (!strcmp(ent, "apos") || !strcmp(ent, "#39")) decoded = '\'';
        else if (!strcmp(ent, "nbsp")) decoded = ' ';
        else if (ent[0] == '#') { int code = atoi(ent + 1); if (code > 32 && code < 127) decoded = (char)code; else known = false; }
        else known = false;
        if (known) {
          if (decoded == ' ') { if (!_stripLastSpace && _textLen < TEXT_MAX) { _text[_textLen++] = ' '; _stripLastSpace = true; } }
          else { _text[_textLen++] = decoded; _stripLastSpace = false; }
          i = j;
          continue;
        }
      }
      _text[_textLen++] = '&';
      _stripLastSpace = false;
      continue;
    }

    if (c == '\n' || c == '\r' || c == '\t' || c == ' ') {
      if (!_stripLastSpace) { _text[_textLen++] = ' '; _stripLastSpace = true; }
      continue;
    }
    if ((unsigned char)c < 32 || (unsigned char)c > 126) continue; // not in the default font
    _text[_textLen++] = c;
    _stripLastSpace = false;
  }
}

void BrowserApp::wrapText() {
  _lineCount = 0;
  size_t i = 0;
  while (i <= _textLen && _lineCount < MAX_LINES) {
    size_t lineStart = i;
    size_t lastSpace = (size_t)-1;
    size_t j = i;
    while (j < _textLen && _text[j] != '\n' && (j - lineStart) < CHARS_PER_LINE) {
      if (_text[j] == ' ') lastSpace = j;
      j++;
    }
    if (j >= _textLen) {
      if (j > lineStart) _lines[_lineCount++] = {(uint16_t)lineStart, (uint16_t)(j - lineStart)};
      i = j + 1;
    } else if (_text[j] == '\n') {
      _lines[_lineCount++] = {(uint16_t)lineStart, (uint16_t)(j - lineStart)};
      i = j + 1;
    } else {
      size_t breakAt = (lastSpace != (size_t)-1 && lastSpace > lineStart) ? lastSpace : j;
      _lines[_lineCount++] = {(uint16_t)lineStart, (uint16_t)(breakAt - lineStart)};
      i = (breakAt == j) ? j : breakAt + 1;
    }
  }
  _scrollLine = 0;
}

void BrowserApp::doFetch(TFT_eSPI& tft) {
  _mode = PAGE;
  _textLen = 0;
  _stripInTag = _stripInScript = _stripInStyle = false;
  _stripLastSpace = true;

  // The actual fetch below blocks for real, so paint a "loading" frame
  // by hand before starting it - otherwise the screen just freezes on
  // whatever was showing until the whole thing finishes.
  UI::clearContent(tft);
  UI::centerText(tft, "Loading...", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 6, 2, Theme::MUTED);
  UI::centerText(tft, _url, Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 14, 1, Theme::MUTED);

  String ssid = _prefs->getString("wssid", Cfg::WIFI_SSID);
  String pass = _prefs->getString("wpass", Cfg::WIFI_PASSWORD);
  if (ssid.length() == 0) {
    appendPlain("No WiFi configured. Set one up in Settings > WiFi Setup first, then come back here.");
    wrapText();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) delay(50);
  if (WiFi.status() != WL_CONNECTED) {
    appendPlain("Couldn't connect to WiFi. Check the network/password in Settings.");
    wrapText();
    WiFi.mode(WIFI_OFF);
    return;
  }

  String url = _url;
  if (url.indexOf("://") < 0) url = "http://" + url;
  bool https = url.startsWith("https://");

  WiFiClientSecure secureClient;
  secureClient.setInsecure();
  WiFiClient plainClient;
  HTTPClient http;
  http.setTimeout(8000);
  bool began = https ? http.begin(secureClient, url) : http.begin(plainClient, url);
  if (!began) {
    appendPlain("Couldn't open that address.");
    wrapText();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Server responded with error %d.", code);
    appendPlain(buf);
    wrapText();
    http.end();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  WiFiClient* stream = http.getStreamPtr();
  char chunk[512];
  const size_t RAW_CAP = 40000; // stop reading raw HTML after this regardless of how much stripped text we've kept
  size_t rawRead = 0;
  uint32_t lastByte = millis();
  while (http.connected() && rawRead < RAW_CAP && _textLen < TEXT_MAX && millis() - lastByte < 5000) {
    int avail = stream->available();
    if (avail <= 0) { delay(10); continue; }
    size_t toRead = (size_t)avail < sizeof(chunk) ? (size_t)avail : sizeof(chunk);
    size_t got = stream->readBytes(chunk, toRead);
    if (got == 0) break;
    stripChunk(chunk, got);
    rawRead += got;
    lastByte = millis();
  }
  http.end();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  if (_textLen == 0) appendPlain("(no readable text on this page)");
  wrapText();
}

UI::Rect BrowserApp::keyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 24;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

void BrowserApp::drawUrlEdit(TFT_eSPI& tft) {
  UI::clearContent(tft);

  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, Cfg::SCREEN_W - 8, 24, 4, Theme::PANEL);
  tft.setTextColor(Theme::TEXT, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  const char* shown = _urlLen > 34 ? _url + (_urlLen - 34) : _url;
  tft.drawString(shown, 10, Cfg::STATUS_BAR_H + 14, 2);
  tft.setTextDatum(TL_DATUM);

  for (uint8_t r = 0; r < NUM_ROWS; r++) {
    uint8_t len = strlen(_rows[r]);
    for (uint8_t c = 0; c < len; c++) {
      UI::Rect kr = keyRect(r, c, len);
      char label[2] = {_rows[r][c], 0};
      tft.fillRoundRect(kr.x, kr.y, kr.w, kr.h, 4, Theme::PANEL);
      tft.setTextColor(Theme::TEXT, Theme::PANEL);
      tft.setTextDatum(MC_DATUM);
      tft.drawString(label, kr.x + kr.w / 2, kr.y + kr.h / 2 + 1, 2);
      tft.setTextDatum(TL_DATUM);
    }
  }

  int16_t ctrlY = Cfg::STATUS_BAR_H + 30 + NUM_ROWS * 24 + 4;
  _delBtn.r.y = ctrlY;
  _clrBtn.r.y = ctrlY;
  _goBtn.r.y = ctrlY;
  _delBtn.draw(tft);
  _clrBtn.draw(tft);
  _goBtn.draw(tft);
}

void BrowserApp::drawPage(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _backToUrlBtn.draw(tft);

  int16_t top = Cfg::STATUS_BAR_H + 30;
  _linesVisible = (Cfg::SCREEN_H - top) / LINE_H;

  tft.setTextColor(Theme::TEXT, Theme::BG);
  tft.setTextDatum(TL_DATUM);
  for (int i = 0; i < _linesVisible; i++) {
    int li = _scrollLine + i;
    if (li < 0 || li >= (int)_lineCount) break;
    char buf[CHARS_PER_LINE + 4];
    uint16_t len = _lines[li].len;
    if (len > CHARS_PER_LINE + 3) len = CHARS_PER_LINE + 3;
    memcpy(buf, _text + _lines[li].start, len);
    buf[len] = 0;
    tft.drawString(buf, 8, top + i * LINE_H, 1);
  }

  if ((int)_lineCount > _linesVisible) {
    int16_t trackH = Cfg::SCREEN_H - top;
    int16_t thumbH = trackH * _linesVisible / _lineCount;
    if (thumbH < 10) thumbH = 10;
    int16_t maxScroll = _lineCount - _linesVisible;
    int16_t thumbY = top + (int32_t)(trackH - thumbH) * _scrollLine / (maxScroll > 0 ? maxScroll : 1);
    tft.fillRect(Cfg::SCREEN_W - 4, thumbY, 3, thumbH, Theme::PANEL2);
  }
}

void BrowserApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  if (_mode == URL_EDIT) drawUrlEdit(tft);
  else drawPage(tft);
}

void BrowserApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (_mode == PAGE) {
    if (down) {
      if (_backToUrlBtn.hit(x, y)) { _mode = URL_EDIT; _dirty = true; return; }
      _dragging = true;
      _dragStartY = y;
      _dragStartScroll = _scrollLine;
      return;
    }
    if (_dragging) {
      int16_t deltaPx = y - _dragStartY;
      int deltaLines = -deltaPx / LINE_H;
      int maxScroll = (int)_lineCount - _linesVisible;
      if (maxScroll < 0) maxScroll = 0;
      int wanted = _dragStartScroll + deltaLines;
      _scrollLine = wanted < 0 ? 0 : (wanted > maxScroll ? maxScroll : wanted);
      _dirty = true;
    }
    return;
  }

  // URL_EDIT
  if (!down) return;
  for (uint8_t r = 0; r < NUM_ROWS; r++) {
    uint8_t len = strlen(_rows[r]);
    for (uint8_t c = 0; c < len; c++) {
      if (keyRect(r, c, len).contains(x, y) && _urlLen < URL_MAX) {
        _url[_urlLen++] = _rows[r][c];
        _url[_urlLen] = 0;
        _dirty = true;
        return;
      }
    }
  }
  if (_delBtn.hit(x, y) && _urlLen > 0) { _url[--_urlLen] = 0; _dirty = true; }
  else if (_clrBtn.hit(x, y)) { _urlLen = 0; _url[0] = 0; _dirty = true; }
  else if (_goBtn.hit(x, y) && _urlLen > 0) { doFetch(tft); _dirty = true; }
}
