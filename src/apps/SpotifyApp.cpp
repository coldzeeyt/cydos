#include "SpotifyApp.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "Config.h"

void SpotifyApp::loadHost() {
  String h = _prefs->getString("spotifyhost", "");
  h.toCharArray(_host, sizeof(_host));
  _hostLen = strlen(_host);
}

// Same minimal, no-library approach as UpdateChecker/ObsApp - good enough
// for a flat JSON object we control the shape of on the companion side.
static bool extractJsonBool(const String& body, const char* key) {
  String needle = String("\"") + key + "\":";
  int k = body.indexOf(needle);
  if (k < 0) return false;
  int v = k + needle.length();
  while (v < (int)body.length() && body[v] == ' ') v++;
  return body.startsWith("true", v);
}

static long extractJsonLong(const String& body, const char* key) {
  String needle = String("\"") + key + "\":";
  int k = body.indexOf(needle);
  if (k < 0) return 0;
  int v = k + needle.length();
  return body.substring(v).toInt();
}

static String extractJsonString(const String& body, const char* key) {
  String needle = String("\"") + key + "\":";
  int k = body.indexOf(needle);
  if (k < 0) return "";
  int q1 = body.indexOf('"', k + needle.length());
  if (q1 < 0) return "";
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return "";
  return body.substring(q1 + 1, q2);
}

UI::Rect SpotifyApp::kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 24;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

bool SpotifyApp::poll() {
  if (WiFi.status() != WL_CONNECTED || _hostLen == 0) return false;

  HTTPClient http;
  http.setTimeout(3000);
  String url = String("http://") + _host + "/now-playing";
  if (!http.begin(url)) return false;

  bool ok = false;
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String body = http.getString();
    _playing = extractJsonBool(body, "playing");
    String track = extractJsonString(body, "track");
    if (track.length() > 0) {
      track.toCharArray(_track, sizeof(_track));
      extractJsonString(body, "artist").toCharArray(_artist, sizeof(_artist));
      extractJsonString(body, "album").toCharArray(_album, sizeof(_album));
      _progressMs = (uint32_t)extractJsonLong(body, "progress_ms");
      _durationMs = (uint32_t)extractJsonLong(body, "duration_ms");
      _hasTrack = true;
    } else {
      _hasTrack = false;
    }
    ok = true;
  }
  http.end();
  return ok;
}

void SpotifyApp::onEnter(TFT_eSPI& tft) {
  loadHost();
  _mode = CONNECTING;
  _hasTrack = false;

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
    poll();
    _mode = NOW_PLAYING;
    _lastPollAt = millis();
  }
  _dirty = true;
}

void SpotifyApp::onExit() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool SpotifyApp::update() {
  if (_mode != NOW_PLAYING) return false;
  if (millis() - _lastPollAt < POLL_INTERVAL_MS) return false;
  _lastPollAt = millis();
  poll(); // blocking, but a local plain-HTTP GET - same tradeoff ObsApp makes
  _dirty = true;
  return true;
}

void SpotifyApp::drawConnecting(TFT_eSPI& tft) {
  // Painted by hand in onEnter() before the blocking WiFi connect; nothing
  // dynamic to redraw here.
}

void SpotifyApp::drawFailed(TFT_eSPI& tft) {
  UI::clearContent(tft);
  UI::centerText(tft, _failMsg, Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 16, 2, Theme::DANGER);
  _retryBtn.draw(tft);
}

void SpotifyApp::drawDisplay(TFT_eSPI& tft) {
  UI::clearContent(tft);

  tft.setTextColor(_hostLen ? Theme::MUTED : Theme::DANGER, Theme::BG);
  tft.setTextDatum(ML_DATUM);
  const char* shownHost = (_hostLen > 28) ? _host + (_hostLen - 28) : _host;
  tft.drawString(_hostLen ? shownHost : "no companion host set", 8, Cfg::STATUS_BAR_H + 13, 1);
  tft.setTextDatum(TL_DATUM);
  _editBtn.draw(tft);

  int16_t midY = Cfg::STATUS_BAR_H + 100;
  if (!_hasTrack) {
    UI::centerText(tft, "Nothing playing", Cfg::SCREEN_W / 2, midY, 2, Theme::MUTED);
    return;
  }

  UI::centerText(tft, _track, Cfg::SCREEN_W / 2, midY - 30, 2, Theme::TEXT);
  UI::centerText(tft, _artist, Cfg::SCREEN_W / 2, midY, 2, Theme::ACCENT);
  UI::centerText(tft, _album, Cfg::SCREEN_W / 2, midY + 24, 1, Theme::MUTED);

  int16_t barY = midY + 46, barX = 40, barW = Cfg::SCREEN_W - 80;
  tft.fillRoundRect(barX, barY, barW, 6, 3, Theme::PANEL2);
  if (_durationMs > 0) {
    int16_t fillW = (int32_t)((uint64_t)barW * _progressMs / _durationMs);
    if (fillW > barW) fillW = barW;
    if (fillW > 0) tft.fillRoundRect(barX, barY, fillW, 6, 3, Theme::GOOD);
  }

  UI::centerText(tft, _playing ? "Playing" : "Paused", Cfg::SCREEN_W / 2, barY + 24, 1,
                 _playing ? Theme::GOOD : Theme::MUTED);
}

void SpotifyApp::drawHostEdit(TFT_eSPI& tft) {
  UI::clearContent(tft);

  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, Cfg::SCREEN_W - 8, 24, 4, Theme::PANEL);
  tft.setTextColor(_hostLen ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  const char* shown = _hostLen > 34 ? _host + (_hostLen - 34) : _host;
  tft.drawString(_hostLen ? shown : "host:port, e.g. 192.168.1.50:8090", 10, Cfg::STATUS_BAR_H + 14, 2);
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

void SpotifyApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  switch (_mode) {
    case CONNECTING: drawConnecting(tft); break;
    case FAILED: drawFailed(tft); break;
    case NOW_PLAYING: drawDisplay(tft); break;
    case HOST_EDIT: drawHostEdit(tft); break;
  }
}

void SpotifyApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
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
      _prefs->putString("spotifyhost", _host);
      if (_hostLen > 0) { poll(); _lastPollAt = millis(); }
      _mode = NOW_PLAYING;
      _dirty = true;
    }
    return;
  }

  // NOW_PLAYING
  if (_editBtn.hit(x, y)) { _mode = HOST_EDIT; _dirty = true; return; }
}
