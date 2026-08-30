#include "DiagnosticsApp.h"
#include "core/Battery.h"
#include "core/SdCard.h"
#include <Arduino.h>
#include <SD.h>

const char* const DiagnosticsApp::TAB_NAMES[DiagnosticsApp::TAB_COUNT] = {"Display", "Touch", "Info"};
const char* const DiagnosticsApp::PATTERN_NAMES[DiagnosticsApp::PATTERN_COUNT] = {
    "Red", "Green", "Blue", "White", "Black", "Checkerboard", "Gray Gradient", "RGB Gradient"};

UI::Rect DiagnosticsApp::tabRect(uint8_t i) const {
  static constexpr int16_t Y = Cfg::STATUS_BAR_H + 2;
  static constexpr int16_t H = 20;
  if (i == 0) return {2, Y, 102, H};
  if (i == 1) return {108, Y, 102, H};
  return {214, Y, 104, H};
}

void DiagnosticsApp::updateTitle() {
  if (_tab == TAB_DISPLAY) {
    snprintf(_title, sizeof(_title), "%s (%d/%d)", PATTERN_NAMES[_pattern], _pattern + 1, PATTERN_COUNT);
  } else {
    strncpy(_title, "Diagnostics", sizeof(_title) - 1);
    _title[sizeof(_title) - 1] = 0;
  }
}

void DiagnosticsApp::onEnter(TFT_eSPI& tft) {
  _tab = TAB_DISPLAY;
  _pattern = 0;
  _touchX = _touchY = -1;
  updateTitle();
  _dirty = true;
}

bool DiagnosticsApp::update() {
  if (_tab == TAB_INFO && millis() - _lastInfoRefresh > 1000) {
    _lastInfoRefresh = millis();
    return true;
  }
  return false;
}

void DiagnosticsApp::drawTabBar(TFT_eSPI& tft) {
  for (uint8_t i = 0; i < TAB_COUNT; i++) {
    UI::Rect r = tabRect(i);
    bool sel = (_tab == i);
    uint16_t bg = sel ? Theme::ACCENT : Theme::PANEL;
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 4, bg);
    tft.setTextColor(sel ? Theme::BG : Theme::TEXT, bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(TAB_NAMES[i], r.x + r.w / 2, r.y + r.h / 2 + 1, 1);
    tft.setTextDatum(TL_DATUM);
  }
}

void DiagnosticsApp::drawDisplayPage(TFT_eSPI& tft) {
  int16_t y0 = Cfg::STATUS_BAR_H + 24;
  int16_t h = Cfg::SCREEN_H - y0;
  switch (_pattern) {
    case 0: tft.fillRect(0, y0, Cfg::SCREEN_W, h, tft.color565(255, 0, 0)); break;
    case 1: tft.fillRect(0, y0, Cfg::SCREEN_W, h, tft.color565(0, 255, 0)); break;
    case 2: tft.fillRect(0, y0, Cfg::SCREEN_W, h, tft.color565(0, 0, 255)); break;
    case 3: tft.fillRect(0, y0, Cfg::SCREEN_W, h, tft.color565(255, 255, 255)); break;
    case 4: tft.fillRect(0, y0, Cfg::SCREEN_W, h, tft.color565(0, 0, 0)); break;
    case 5: {
      const int16_t cell = 8;
      for (int16_t y = 0; y < h; y += cell) {
        int16_t ch = min((int16_t)cell, (int16_t)(h - y));
        for (int16_t x = 0; x < Cfg::SCREEN_W; x += cell) {
          bool on = ((x / cell) + (y / cell)) % 2 == 0;
          tft.fillRect(x, y0 + y, cell, ch, on ? tft.color565(255, 255, 255) : tft.color565(0, 0, 0));
        }
      }
      break;
    }
    case 6:
      for (int16_t x = 0; x < Cfg::SCREEN_W; x++) {
        uint8_t v = (uint8_t)((x * 255L) / (Cfg::SCREEN_W - 1));
        tft.drawFastVLine(x, y0, h, tft.color565(v, v, v));
      }
      break;
    case 7: {
      int16_t third = Cfg::SCREEN_W / 3;
      for (int16_t x = 0; x < Cfg::SCREEN_W; x++) {
        int16_t xInThird = x % third;
        uint8_t v = (uint8_t)(((int32_t)xInThird * 255L) / (third - 1));
        uint16_t c = x < third ? tft.color565(v, 0, 0) : (x < 2 * third ? tft.color565(0, v, 0) : tft.color565(0, 0, v));
        tft.drawFastVLine(x, y0, h, c);
      }
      break;
    }
  }
}

void DiagnosticsApp::drawTouchPage(TFT_eSPI& tft) {
  int16_t y0 = Cfg::STATUS_BAR_H + 24;
  UI::centerText(tft, "Drag your finger around", Cfg::SCREEN_W / 2, y0 + 16, 1, Theme::MUTED);
  if (_touchX >= 0) {
    tft.drawFastHLine(0, _touchY, Cfg::SCREEN_W, Theme::PANEL2);
    tft.drawFastVLine(_touchX, y0, Cfg::SCREEN_H - y0, Theme::PANEL2);
    tft.fillCircle(_touchX, _touchY, 6, Theme::ACCENT);
    char buf[24];
    snprintf(buf, sizeof(buf), "%d, %d", _touchX, _touchY);
    UI::centerText(tft, buf, Cfg::SCREEN_W / 2, Cfg::SCREEN_H - 16, 1, Theme::TEXT);
  }
}

void DiagnosticsApp::drawInfoPage(TFT_eSPI& tft) {
  int16_t y = Cfg::STATUS_BAR_H + 34;
  const int16_t lh = 20;
  char buf[48];

  tft.setTextColor(Theme::TEXT, Theme::BG);
  tft.setTextDatum(TL_DATUM);

  snprintf(buf, sizeof(buf), "Chip: %s @ %dMHz", ESP.getChipModel(), ESP.getCpuFreqMHz());
  tft.drawString(buf, 12, y, 1); y += lh;

  snprintf(buf, sizeof(buf), "Free heap: %lu KB", (unsigned long)(ESP.getFreeHeap() / 1024));
  tft.drawString(buf, 12, y, 1); y += lh;

  snprintf(buf, sizeof(buf), "Flash: %lu MB", (unsigned long)(ESP.getFlashChipSize() / (1024UL * 1024UL)));
  tft.drawString(buf, 12, y, 1); y += lh;

  uint32_t upSec = millis() / 1000;
  snprintf(buf, sizeof(buf), "Uptime: %luh %lum %lus",
           (unsigned long)(upSec / 3600), (unsigned long)((upSec / 60) % 60), (unsigned long)(upSec % 60));
  tft.drawString(buf, 12, y, 1); y += lh;

  if (_sd && _sd->available()) {
    snprintf(buf, sizeof(buf), "SD card: %lu MB", (unsigned long)(SD.cardSize() / (1024UL * 1024UL)));
  } else {
    snprintf(buf, sizeof(buf), "SD card: not detected");
  }
  tft.drawString(buf, 12, y, 1); y += lh;

  if (_battery && _battery->available()) {
    snprintf(buf, sizeof(buf), "Battery: %.2fV (%d%%)", _battery->voltage(), _battery->percent());
  } else {
    snprintf(buf, sizeof(buf), "Battery: monitor disabled");
  }
  tft.drawString(buf, 12, y, 1);
}

void DiagnosticsApp::draw(TFT_eSPI& tft) {
  if (!_dirty) return;
  _dirty = false;
  UI::clearContent(tft);
  drawTabBar(tft);
  switch (_tab) {
    case TAB_DISPLAY: drawDisplayPage(tft); break;
    case TAB_TOUCH: drawTouchPage(tft); break;
    case TAB_INFO: drawInfoPage(tft); break;
    default: break;
  }
}

void DiagnosticsApp::onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {
  if (down) {
    for (uint8_t i = 0; i < TAB_COUNT; i++) {
      if (tabRect(i).contains(x, y)) {
        _tab = (Tab)i;
        if (_tab == TAB_TOUCH) _touchX = _touchY = -1;
        updateTitle();
        _dirty = true;
        return;
      }
    }
  }

  if (_tab == TAB_DISPLAY) {
    if (down) {
      _pattern = (_pattern + 1) % PATTERN_COUNT;
      updateTitle();
      _dirty = true;
    }
    return;
  }

  if (_tab == TAB_TOUCH) {
    _touchX = x;
    _touchY = y;
    _dirty = true;
    return;
  }
}
