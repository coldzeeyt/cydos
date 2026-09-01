#include "MediaPlayerApp.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include "Config.h"

void MediaPlayerApp::loadHost() {
  String h = _prefs->getString("mediahost", "");
  h.toCharArray(_host, sizeof(_host));
  _hostLen = strlen(_host);
}

// Same minimal, no-library JSON reading as ObsApp/SpotifyApp - good
// enough for the flat, fixed-shape objects the companion script returns.
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

// Pulls quoted strings out of the flat JSON array in {"tracks": [...]}-
// not a real parser, just enough for the fixed shape the companion script
// always returns. Handles \" and \\ escapes; anything else passes through.
static uint8_t parseTracksArray(const String& body, char out[][MediaPlayerApp::NAME_LEN], uint8_t maxItems) {
  int arrStart = body.indexOf('[');
  if (arrStart < 0) return 0;
  uint8_t count = 0;
  size_t i = (size_t)arrStart, n = body.length();
  while (i < n && count < maxItems) {
    if (body[i] == ']') break;
    if (body[i] != '"') { i++; continue; }
    i++; // opening quote
    size_t len = 0;
    while (i < n && body[i] != '"') {
      char c = body[i];
      if (c == '\\' && i + 1 < n) { i++; c = body[i]; }
      if (len < MediaPlayerApp::NAME_LEN - 1) out[count][len++] = c;
      i++;
    }
    out[count][len] = 0;
    if (i < n) i++; // closing quote
    count++;
  }
  return count;
}

UI::Rect MediaPlayerApp::kbKeyRect(uint8_t row, uint8_t col, uint8_t rowLen) const {
  int16_t top = Cfg::STATUS_BAR_H + 30;
  int16_t rowH = 24;
  int16_t keyW = Cfg::SCREEN_W / rowLen;
  return {(int16_t)(col * keyW + 1), (int16_t)(top + row * rowH), (int16_t)(keyW - 2), (int16_t)(rowH - 4)};
}

UI::Rect MediaPlayerApp::trackRowRect(uint8_t rowInPage) const {
  return {4, (int16_t)(Cfg::STATUS_BAR_H + 28 + rowInPage * 26), (int16_t)(Cfg::SCREEN_W - 8), 24};
}

bool MediaPlayerApp::fetchTracks() {
  if (WiFi.status() != WL_CONNECTED || _hostLen == 0) return false;
  HTTPClient http;
  http.setTimeout(4000);
  String url = String("http://") + _host + "/tracks";
  if (!http.begin(url)) return false;

  bool ok = false;
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String body = http.getString();
    _trackCount = parseTracksArray(body, _tracks, MAX_TRACKS);
    _tracksLoaded = true;
    ok = true;
  }
  http.end();
  return ok;
}

bool MediaPlayerApp::fetchStatus() {
  if (WiFi.status() != WL_CONNECTED || _hostLen == 0) return false;
  HTTPClient http;
  http.setTimeout(3000);
  String url = String("http://") + _host + "/status";
  if (!http.begin(url)) return false;

  bool ok = false;
  int code = http.GET();
  if (code == HTTP_CODE_OK) {
    String body = http.getString();
    _playing = extractJsonBool(body, "playing");
    extractJsonString(body, "track").toCharArray(_currentTrack, sizeof(_currentTrack));
    long vol = extractJsonLong(body, "volume");
    if (vol >= 0 && vol <= 100) _volume = (int)vol;
    ok = true;
  }
  http.end();
  return ok;
}

bool MediaPlayerApp::sendCommand(const char* path) {
  if (WiFi.status() != WL_CONNECTED || _hostLen == 0) return false;
  HTTPClient http;
  http.setTimeout(3000);
  String url = String("http://") + _host + path;
  if (!http.begin(url)) return false;
  int code = http.GET();
  http.end();
  return code == HTTP_CODE_OK;
}

bool MediaPlayerApp::sendPlay(const char* track) {
  if (WiFi.status() != WL_CONNECTED || _hostLen == 0) return false;
  HTTPClient http;
  http.setTimeout(4000);
  String url = String("http://") + _host + "/play?track=" + track;
  if (!http.begin(url)) return false;
  int code = http.GET();
  http.end();
  return code == HTTP_CODE_OK;
}

void MediaPlayerApp::onEnter(TFT_eSPI& tft) {
  loadHost();
  _mode = CONNECTING;
  _tracksLoaded = false;
  _trackCount = 0;
  _listPage = 0;

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
    UI::centerText(tft, "Loading tracks...", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 2, Theme::MUTED);
    fetchTracks();
    fetchStatus();
    _mode = LIST;
    _lastPollAt = millis();
  }
  _dirty = true;
}

void MediaPlayerApp::onExit() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

bool MediaPlayerApp::update() {
  if (_mode != PLAYER) return false;
  if (millis() - _lastPollAt < POLL_INTERVAL_MS) return false;
  _lastPollAt = millis();
  fetchStatus(); // blocking, but a local plain-HTTP GET - same tradeoff Spotify/OBS make
  _dirty = true;
  return true;
}

void MediaPlayerApp::drawConnecting(TFT_eSPI& tft) {
  // Painted by hand in onEnter() before the blocking WiFi connect + track
  // fetch; nothing dynamic to redraw here.
}

void MediaPlayerApp::drawFailed(TFT_eSPI& tft) {
  UI::clearContent(tft);
  UI::centerText(tft, _failMsg, Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 16, 2, Theme::DANGER);
  _retryBtn.draw(tft);
}

void MediaPlayerApp::drawList(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _playerBtn.draw(tft);
  _editBtn.draw(tft);
  _helpBtn.draw(tft);

  if (!_tracksLoaded) {
    UI::centerText(tft, "Couldn't reach the companion script", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 - 10, 1, Theme::DANGER);
    UI::centerText(tft, "Check it's running and Edit has the right host", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2 + 10, 1, Theme::MUTED);
    return;
  }
  if (_trackCount == 0) {
    UI::centerText(tft, "No .mp3/.wav/.ogg files found", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 1, Theme::MUTED);
    return;
  }

  uint8_t base = _listPage * ROWS_PER_PAGE;
  for (uint8_t row = 0; row < ROWS_PER_PAGE; row++) {
    uint8_t i = base + row;
    if (i >= _trackCount) break;
    UI::Rect r = trackRowRect(row);
    bool isCurrent = strcmp(_tracks[i], _currentTrack) == 0 && _currentTrack[0] != 0;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, isCurrent ? Theme::PANEL2 : Theme::PANEL);
    tft.setTextColor(isCurrent ? Theme::ACCENT : Theme::TEXT, isCurrent ? Theme::PANEL2 : Theme::PANEL);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(_tracks[i], r.x + 8, r.y + r.h / 2 + 1, 1);
    tft.setTextDatum(TL_DATUM);
  }

  uint8_t pc = listPageCount();
  if (pc > 1) {
    _listPrevBtn.textColor = _listPage > 0 ? Theme::TEXT : Theme::MUTED;
    _listNextBtn.textColor = (_listPage + 1 < pc) ? Theme::TEXT : Theme::MUTED;
    _listPrevBtn.draw(tft);
    _listNextBtn.draw(tft);
    char pageBuf[12];
    snprintf(pageBuf, sizeof(pageBuf), "%d/%d", _listPage + 1, pc);
    UI::centerText(tft, pageBuf, Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 195, 1, Theme::MUTED);
  }
}

void MediaPlayerApp::drawPlayer(TFT_eSPI& tft) {
  UI::clearContent(tft);
  _tracksBtn.draw(tft);
  _helpBtn.draw(tft);

  int16_t nameY = Cfg::STATUS_BAR_H + 50;
  UI::centerText(tft, _currentTrack[0] ? _currentTrack : "Nothing playing", Cfg::SCREEN_W / 2, nameY, 2,
                 _currentTrack[0] ? Theme::TEXT : Theme::MUTED);
  UI::centerText(tft, _currentTrack[0] ? (_playing ? "Playing" : "Paused") : "", Cfg::SCREEN_W / 2, nameY + 22, 1,
                 _playing ? Theme::GOOD : Theme::MUTED);

  int16_t ctrlY = Cfg::STATUS_BAR_H + 92;
  _prevBtn.r.y = ctrlY;
  _playPauseBtn.r.y = ctrlY;
  _nextBtn.r.y = ctrlY;
  _playPauseBtn.label = _playing ? "Pause" : "Play";
  _prevBtn.draw(tft);
  _playPauseBtn.draw(tft);
  _nextBtn.draw(tft);

  _stopBtn.r.y = ctrlY + 34;
  _stopBtn.draw(tft);

  int16_t volY = ctrlY + 66;
  UI::centerText(tft, "Volume", Cfg::SCREEN_W / 2, volY, 1, Theme::MUTED);
  _volumeSlider.r.y = volY + 10;
  _volumeSlider.value = _volume;
  _volumeSlider.draw(tft);
}

void MediaPlayerApp::drawHostEdit(TFT_eSPI& tft) {
  UI::clearContent(tft);

  int16_t fieldW = Cfg::SCREEN_W - 8 - 32; // leave room for the "?" help button
  tft.fillRoundRect(4, Cfg::STATUS_BAR_H + 2, fieldW, 24, 4, Theme::PANEL);
  tft.setTextColor(_hostLen ? Theme::TEXT : Theme::MUTED, Theme::PANEL);
  tft.setTextDatum(ML_DATUM);
  const char* shown = _hostLen > 26 ? _host + (_hostLen - 26) : _host;
  tft.drawString(_hostLen ? shown : "host:port, e.g. 192.168.1.50:8095", 10, Cfg::STATUS_BAR_H + 14, 2);
  tft.setTextDatum(TL_DATUM);
  _helpBtn.draw(tft);

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

void MediaPlayerApp::drawHelp(TFT_eSPI& tft) {
  UI::clearContent(tft);

  UI::centerText(tft, "Media Player Setup", Cfg::SCREEN_W / 2, Cfg::STATUS_BAR_H + 12, 2, Theme::ACCENT);

  static const char* LINES[] = {
      "The CYD has no speaker - audio",
      "plays on your PC, this just",
      "remote-controls it.",
      "1. On your PC: pip install pygame",
      "2. python3 cydos_media_player.py",
      "   --dir \"C:\\path\\to\\music\"",
      "   (in pc-companion/, mp3/wav/ogg)",
      "3. Get this PC's LAN IP (ipconfig",
      "   or ifconfig)",
      "4. Enter IP:8095 above",
  };
  int16_t y = Cfg::STATUS_BAR_H + 26;
  tft.setTextColor(Theme::TEXT, Theme::BG);
  tft.setTextDatum(TL_DATUM);
  for (const char* line : LINES) {
    tft.drawString(line, 8, y, 1);
    y += 13;
  }

  _helpBackBtn.draw(tft);
}

void MediaPlayerApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  switch (_mode) {
    case CONNECTING: drawConnecting(tft); break;
    case FAILED: drawFailed(tft); break;
    case LIST: drawList(tft); break;
    case PLAYER: drawPlayer(tft); break;
    case HOST_EDIT: drawHostEdit(tft); break;
    case HELP: drawHelp(tft); break;
  }
}

void MediaPlayerApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  // Mirrors SettingsApp's brightness slider exactly: onTouch fires every
  // frame the finger is down or held, but `down` is only true on the very
  // first of those frames - a real release is onTouchUp(), not `!down`
  // (that's just "not the first frame"), so the drag has to keep tracking
  // across calls where down is false, not stop on them.
  if (_mode == PLAYER) {
    if (down && _volumeSlider.hit(x, y)) _draggingVolume = true;
    if (_draggingVolume) {
      _volumeSlider.updateFromTouch(x);
      _volume = _volumeSlider.value;
      sendCommand((String("/volume?level=") + _volume).c_str());
      _dirty = true;
      return;
    }
  }

  if (!down) return;

  if (_mode == FAILED) {
    if (_retryBtn.hit(x, y)) onEnter(tft);
    return;
  }

  if (_mode == HELP) {
    if (_helpBackBtn.hit(x, y)) { _mode = _helpReturnTo; _dirty = true; }
    return;
  }

  if (_mode == HOST_EDIT) {
    if (_helpBtn.hit(x, y)) { _helpReturnTo = HOST_EDIT; _mode = HELP; _dirty = true; return; }
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
      _prefs->putString("mediahost", _host);
      if (_hostLen > 0) { fetchTracks(); fetchStatus(); _lastPollAt = millis(); }
      _mode = LIST;
      _dirty = true;
    }
    return;
  }

  if (_mode == LIST) {
    if (_playerBtn.hit(x, y)) { _mode = PLAYER; fetchStatus(); _lastPollAt = millis(); _dirty = true; return; }
    if (_editBtn.hit(x, y)) { _mode = HOST_EDIT; _dirty = true; return; }
    if (_helpBtn.hit(x, y)) { _helpReturnTo = LIST; _mode = HELP; _dirty = true; return; }

    uint8_t pc = listPageCount();
    if (pc > 1) {
      if (_listPrevBtn.hit(x, y)) { if (_listPage > 0) { _listPage--; _dirty = true; } return; }
      if (_listNextBtn.hit(x, y)) { if (_listPage + 1 < pc) { _listPage++; _dirty = true; } return; }
    }

    uint8_t base = _listPage * ROWS_PER_PAGE;
    for (uint8_t row = 0; row < ROWS_PER_PAGE; row++) {
      uint8_t i = base + row;
      if (i >= _trackCount) break;
      if (trackRowRect(row).contains(x, y)) {
        sendPlay(_tracks[i]);
        strncpy(_currentTrack, _tracks[i], sizeof(_currentTrack) - 1);
        _currentTrack[sizeof(_currentTrack) - 1] = 0;
        _playing = true;
        _mode = PLAYER;
        _lastPollAt = millis();
        _dirty = true;
        return;
      }
    }
    return;
  }

  // PLAYER
  if (_tracksBtn.hit(x, y)) { _mode = LIST; _dirty = true; return; }
  if (_helpBtn.hit(x, y)) { _helpReturnTo = PLAYER; _mode = HELP; _dirty = true; return; }
  if (_prevBtn.hit(x, y)) { sendCommand("/prev"); fetchStatus(); _dirty = true; return; }
  if (_nextBtn.hit(x, y)) { sendCommand("/next"); fetchStatus(); _dirty = true; return; }
  if (_stopBtn.hit(x, y)) { sendCommand("/stop"); fetchStatus(); _dirty = true; return; }
  if (_playPauseBtn.hit(x, y)) {
    sendCommand(_playing ? "/pause" : "/resume");
    fetchStatus();
    _dirty = true;
  }
}
