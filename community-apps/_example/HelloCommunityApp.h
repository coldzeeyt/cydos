#pragma once
#include "apps/App.h"
#include "core/UI.h"

// Reference template for community-apps/ submissions - copy this folder,
// rename the class/file, and edit app.json to point at your own. This one
// is skipped by the App Store listing (folders starting with "_" are
// templates, not real submissions) but still gets built by CI, so it's
// proof the whole pipeline - copy, compile, register, add a tile - works.
class HelloCommunityApp : public App {
public:
  const char* name() const override { return "Hello"; }

  void onEnter(TFT_eSPI& tft) override { _dirty = true; }

  void draw(TFT_eSPI& tft) override {
    if (!_dirty) return;
    _dirty = false;
    UI::clearContent(tft);
    UI::centerText(tft, "Hello from a community app!", Cfg::SCREEN_W / 2, Cfg::SCREEN_H / 2, 2, Theme::TEXT);
  }

private:
  bool _dirty = true;
};
