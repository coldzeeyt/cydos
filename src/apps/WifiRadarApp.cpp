#include "WifiRadarApp.h"

static const uint32_t RESCAN_INTERVAL_MS = 4000;
static const uint32_t FRAME_MS = 40;

static uint16_t hashAngle(const String& s) {
  uint32_t h = 2166136261u;
  for (size_t i = 0; i < s.length(); i++) {
    h ^= (uint8_t)s[i];
    h *= 16777619u;
  }
  return h % 360;
}

void WifiRadarApp::onEnter(TFT_eSPI& tft) {
  _cx = Cfg::SCREEN_W / 2;
  _cy = Cfg::STATUS_BAR_H + (Cfg::SCREEN_H - Cfg::STATUS_BAR_H) / 2 + 4;
  _radius = 92;
  _selected = -1;
  _scanning = false;
  _neverScanned = true;
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  // Switching into STA mode takes the radio a moment to settle - starting
  // a scan in the same tick as WiFi.mode()/disconnect() is a common way to
  // get WIFI_SCAN_FAILED back on real hardware, so give it a beat first.
  _nextScanAt = millis() + 300;
  _dirty = true;
}

void WifiRadarApp::onExit() {
  WiFi.scanDelete();
  WiFi.mode(WIFI_OFF);
}

void WifiRadarApp::startScan() {
  WiFi.scanNetworks(true /*async*/, false /*hidden*/);
  _scanning = true;
}

void WifiRadarApp::collectResults() {
  int n = WiFi.scanComplete();
  if (n == WIFI_SCAN_RUNNING) return; // still going - check again next tick

  _scanning = false;
  if (n < 0) {
    // WIFI_SCAN_FAILED (or anything else unexpected) - don't get stuck
    // showing "scanning..." forever, just try again shortly.
    _nextScanAt = millis() + 1500;
    return;
  }

  _netCount = 0;
  for (int i = 0; i < n && _netCount < MAX_NETS; i++) {
    Net& net = _nets[_netCount];
    net.ssid = WiFi.SSID(i);
    if (net.ssid.length() == 0) net.ssid = "(hidden)";
    net.rssi = WiFi.RSSI(i);
    net.channel = WiFi.channel(i);
    net.enc = WiFi.encryptionType(i);
    net.angleDeg = hashAngle(net.ssid + String(net.channel));
    _netCount++;
  }
  WiFi.scanDelete();
  _neverScanned = false;
  _nextScanAt = millis() + RESCAN_INTERVAL_MS;
  _dirty = true;
}

bool WifiRadarApp::update() {
  if (_scanning) collectResults();
  else if (millis() >= _nextScanAt) startScan();

  if (millis() - _lastFrame >= FRAME_MS) {
    _lastFrame = millis();
    _sweepDeg = fmod(_sweepDeg + 4.0f, 360.0f);
    _dirty = true;
  }
  return _dirty;
}

void WifiRadarApp::drawRadarFace(TFT_eSPI& tft) {
  tft.fillRect(0, Cfg::STATUS_BAR_H, Cfg::SCREEN_W, Cfg::SCREEN_H - Cfg::STATUS_BAR_H, Theme::BG);
  for (int i = 1; i <= 3; i++) {
    tft.drawCircle(_cx, _cy, _radius * i / 3, Theme::PANEL2);
  }
  tft.drawFastHLine(_cx - _radius, _cy, _radius * 2, Theme::PANEL2);
  tft.drawFastVLine(_cx, _cy - _radius, _radius * 2, Theme::PANEL2);

  // Sweep line + soft trailing wedge.
  float rad = _sweepDeg * DEG_TO_RAD;
  int16_t ex = _cx + cos(rad) * _radius;
  int16_t ey = _cy + sin(rad) * _radius;
  tft.drawLine(_cx, _cy, ex, ey, Theme::GOOD);
  for (int t = 1; t <= 5; t++) {
    float trad = (_sweepDeg - t * 5) * DEG_TO_RAD;
    tft.drawLine(_cx, _cy, _cx + cos(trad) * _radius, _cy + sin(trad) * _radius, Theme::PANEL);
  }

  const char* status = _scanning ? "scanning..." : (_neverScanned ? "starting WiFi..." : "");
  UI::centerText(tft, status, _cx, Cfg::STATUS_BAR_H + 10, 2, Theme::MUTED);
}

void WifiRadarApp::drawBlips(TFT_eSPI& tft) {
  for (uint8_t i = 0; i < _netCount; i++) {
    const Net& net = _nets[i];
    int32_t rssi = constrain(net.rssi, -90, -30);
    float dist = map(rssi, -90, -30, _radius - 4, 10);
    float rad = net.angleDeg * DEG_TO_RAD;
    int16_t bx = _cx + cos(rad) * dist;
    int16_t by = _cy + sin(rad) * dist;

    uint16_t color = rssi > -55 ? Theme::GOOD : (rssi > -75 ? Theme::ACCENT2 : Theme::DANGER);
    uint8_t sz = (i == _selected) ? 7 : 5;
    tft.fillCircle(bx, by, sz, color);
    tft.drawCircle(bx, by, sz + 2, Theme::TEXT);
  }
}

UI::Rect WifiRadarApp::blipHit(uint8_t i) const {
  const Net& net = _nets[i];
  int32_t rssi = constrain(net.rssi, -90, -30);
  float dist = map(rssi, -90, -30, _radius - 4, 10);
  float rad = net.angleDeg * DEG_TO_RAD;
  int16_t bx = _cx + cos(rad) * dist;
  int16_t by = _cy + sin(rad) * dist;
  return {(int16_t)(bx - 12), (int16_t)(by - 12), 24, 24};
}

void WifiRadarApp::drawDetail(TFT_eSPI& tft) {
  if (_selected < 0 || _selected >= _netCount) return;
  const Net& net = _nets[_selected];
  int16_t h = 56;
  int16_t y = Cfg::SCREEN_H - h;
  tft.fillRect(0, y, Cfg::SCREEN_W, h, Theme::PANEL);
  tft.drawFastHLine(0, y, Cfg::SCREEN_W, Theme::MUTED);

  tft.setTextColor(Theme::TEXT, Theme::PANEL);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(net.ssid, 8, y + 6, 2);

  char line[48];
  const char* lock = (net.enc == WIFI_AUTH_OPEN) ? "open" : "locked";
  snprintf(line, sizeof(line), "%d dBm  ch %d  %s", (int)net.rssi, (int)net.channel, lock);
  tft.setTextColor(Theme::MUTED, Theme::PANEL);
  tft.drawString(line, 8, y + 28, 2);
}

void WifiRadarApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  drawRadarFace(tft);
  drawBlips(tft);
  drawDetail(tft);

  char countBuf[24];
  snprintf(countBuf, sizeof(countBuf), "%d networks", _netCount);
  UI::centerText(tft, countBuf, _cx, Cfg::SCREEN_H - (_selected >= 0 ? 62 : 8), 2, Theme::MUTED);
}

void WifiRadarApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (!down) return;
  for (uint8_t i = 0; i < _netCount; i++) {
    if (blipHit(i).contains(x, y)) {
      _selected = (_selected == (int8_t)i) ? -1 : (int8_t)i;
      _dirty = true;
      return;
    }
  }
  if (_selected >= 0) {
    _selected = -1;
    _dirty = true;
  }
}
