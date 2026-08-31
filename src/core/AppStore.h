#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include <TFT_eSPI.h>
#include "UI.h"
#include "Config.h"
#include "SdCard.h"

// WiFi-side of the on-device App Store's "Get More" tab: fetch the tiny
// pipe-delimited catalogs scripts/generate_ondevice_catalog.py publishes,
// and download a chosen file straight to the SD card with a progress bar.
// Both blocking - called from CommunityStoreApp::onTouch() only, same
// "block during an explicit user action" tradeoff ObsApp::sendScene()
// already makes, never from update()/draw().
namespace AppStore {

constexpr const char* BASE_URL = "https://coldzeeyt.github.io/cydos";

// slug|name|path, one per line - see scripts/generate_ondevice_catalog.py.
// Parses into caller-provided fixed arrays; returns how many it found
// (capped at maxEntries). Any failure (no WiFi, bad response) just
// returns 0 rather than partial/garbage entries.
inline uint8_t fetchManifest(const char* urlPath, char names[][32], char paths[][48], uint8_t maxEntries) {
  if (WiFi.status() != WL_CONNECTED) return 0;

  HTTPClient http;
  http.setTimeout(6000);
  String url = String(BASE_URL) + urlPath;
  if (!http.begin(url)) return 0;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return 0;
  }
  String body = http.getString();
  http.end();

  uint8_t count = 0;
  int pos = 0;
  int len = body.length();
  while (pos < len && count < maxEntries) {
    int nl = body.indexOf('\n', pos);
    String line = (nl < 0) ? body.substring(pos) : body.substring(pos, nl);
    pos = (nl < 0) ? len : nl + 1;
    line.trim();
    if (line.length() == 0) continue;

    int p1 = line.indexOf('|');
    int p2 = (p1 < 0) ? -1 : line.indexOf('|', p1 + 1);
    if (p1 < 0 || p2 < 0) continue;

    line.substring(p1 + 1, p2).toCharArray(names[count], 32);
    line.substring(p2 + 1).toCharArray(paths[count], 48);
    count++;
  }
  return count;
}

// Downloads BASE_URL + urlPath to destPath on the SD card, redrawing a
// progress bar as bytes arrive. Returns true only if the full file (per
// the server's declared Content-Length, when it sends one) was written.
inline bool downloadToSd(TFT_eSPI& tft, const char* urlPath, const char* destPath, const char* label) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.setTimeout(10000);
  String url = String(BASE_URL) + urlPath;
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  int total = http.getSize(); // -1 if the server didn't send Content-Length
  WiFiClient* stream = http.getStreamPtr();
  // Neither WiFi nor the display (its own dedicated SPI peripheral, see
  // USE_HSPI_PORT) are affected by which pins the shared SD/touch bus
  // currently points at - see SdCard.h - so it's fine for this to stay
  // pointed at SD for the whole download loop below.
  SdCard::useSdBus();
  File f = SD.open(destPath, FILE_WRITE);
  if (!f) {
    SdCard::useTouchBus();
    http.end();
    return false;
  }

  const int16_t barX = 40, barW = Cfg::SCREEN_W - 80, barH = 16;
  const int16_t barY = Cfg::SCREEN_H / 2;
  uint8_t buf[512];
  uint32_t written = 0;
  uint32_t lastDraw = 0;

  while (http.connected() && (total < 0 || (int)written < total)) {
    size_t avail = stream->available();
    if (avail == 0) {
      if (!http.connected()) break;
      delay(2);
      continue;
    }
    size_t n = stream->readBytes(buf, min(avail, sizeof(buf)));
    if (n == 0) break;
    f.write(buf, n);
    written += n;

    uint32_t now = millis();
    if (now - lastDraw > 80) {
      lastDraw = now;
      UI::centerText(tft, label, Cfg::SCREEN_W / 2, barY - 20, 1, Theme::MUTED);
      tft.fillRoundRect(barX, barY, barW, barH, 4, Theme::PANEL2);
      if (total > 0) {
        int16_t fillW = (int32_t)((int64_t)barW * written / total);
        if (fillW > 0) tft.fillRoundRect(barX, barY, fillW, barH, 4, Theme::ACCENT);
      } else {
        // No Content-Length - a small block sweeping across instead of a
        // fill level, so it still reads as "working," not "stuck."
        int16_t knobW = 24;
        int16_t sweep = (now / 4) % (barW - knobW);
        tft.fillRoundRect(barX + sweep, barY, knobW, barH, 4, Theme::ACCENT);
      }
      char pct[16];
      if (total > 0) snprintf(pct, sizeof(pct), "%lu%%", (unsigned long)(100UL * written / total));
      else snprintf(pct, sizeof(pct), "%lu KB", (unsigned long)(written / 1024));
      UI::centerText(tft, pct, Cfg::SCREEN_W / 2, barY + barH + 14, 1, Theme::TEXT);
    }
  }
  f.close();
  SdCard::useTouchBus();
  http.end();
  return total < 0 || (int)written >= total;
}

} // namespace AppStore
