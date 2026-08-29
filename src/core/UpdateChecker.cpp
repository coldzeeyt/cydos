#include "UpdateChecker.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "Version.h"

// Pulls the string value for "key" out of a small, flat JSON object like
// {"version":"1.2"}. Good enough for a file we control ourselves - not a
// general JSON parser, and deliberately not pulling in a library for one
// field.
static String extractJsonString(const String& body, const char* key) {
  String needle = String("\"") + key + "\"";
  int k = body.indexOf(needle);
  if (k < 0) return "";
  int colon = body.indexOf(':', k + needle.length());
  if (colon < 0) return "";
  int q1 = body.indexOf('"', colon + 1);
  if (q1 < 0) return "";
  int q2 = body.indexOf('"', q1 + 1);
  if (q2 < 0) return "";
  return body.substring(q1 + 1, q2);
}

// Returns >0 if a is newer than b, <0 if older, 0 if equal.
static int compareVersions(const char* a, const char* b) {
  int majA = 0, minA = 0, patA = 0;
  int majB = 0, minB = 0, patB = 0;
  sscanf(a, "%d.%d.%d", &majA, &minA, &patA);
  sscanf(b, "%d.%d.%d", &majB, &minB, &patB);
  if (majA != majB) return majA - majB;
  if (minA != minB) return minA - minB;
  return patA - patB;
}

void UpdateChecker::performCheck() {
  String ssid = _prefs ? _prefs->getString("wssid", Cfg::WIFI_SSID) : String(Cfg::WIFI_SSID);
  String pass = _prefs ? _prefs->getString("wpass", Cfg::WIFI_PASSWORD) : String(Cfg::WIFI_PASSWORD);
  if (ssid.length() == 0) {
    setResult("no WiFi configured");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
    delay(50);
  }

  if (WiFi.status() != WL_CONNECTED) {
    setResult("couldn't connect");
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // just reading a small public version file, not worth pinning a cert for
  HTTPClient http;
  http.setTimeout(6000);

  if (http.begin(client, Cfg::UPDATE_CHECK_URL)) {
    int code = http.GET();
    if (code == HTTP_CODE_OK) {
      String body = http.getString();
      String latest = extractJsonString(body, "version");
      if (latest.length() > 0) {
        latest.toCharArray(_latestVersion, sizeof(_latestVersion));
        if (compareVersions(latest.c_str(), CYDOS_VERSION) > 0) {
          _hasUpdate = true;
          _dismissed = false;
          setResult("update available");
        } else {
          setResult("up to date");
        }
      } else {
        setResult("bad response");
      }
    } else {
      char buf[24];
      snprintf(buf, sizeof(buf), "server error %d", code);
      setResult(buf);
    }
    http.end();
  } else {
    setResult("check failed");
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

void UpdateChecker::update() {
  if (millis() < _nextCheckAt) return;
  _nextCheckAt = millis() + Cfg::UPDATE_CHECK_INTERVAL_MS;
  performCheck();
}
