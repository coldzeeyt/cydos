#pragma once
#include <TFT_eSPI.h>
#include <stdint.h>

// Every CydOs app implements this. AppManager owns the status bar (title +
// battery/back icon); an app only ever draws/handles touches below it, in
// the area described by Cfg::STATUS_BAR_H.
class App {
public:
  virtual ~App() {}

  virtual const char* name() const = 0;

  // Called once when the app becomes active. Do full-screen setup here
  // (the content area has already been cleared to the theme background).
  virtual void onEnter(TFT_eSPI& tft) {}

  // Called once when leaving the app (e.g. user hit Home).
  virtual void onExit() {}

  // Called every main-loop tick regardless of touch state. Use for
  // animation/timers/non-blocking sensor polling. Return true if the
  // app needs a redraw this tick.
  virtual bool update() { return false; }

  // Full (or partial, app's choice) redraw of the content area.
  virtual void draw(TFT_eSPI& tft) = 0;

  // down=true on the tick a press starts, false while held/dragging.
  // (x,y) are in full-screen coordinates.
  virtual void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {}

  virtual void onTouchUp() {}
};
