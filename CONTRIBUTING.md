# Contributing an app to CydOs

CydOs has no OTA or plugin-loading system - the CYD's flash is just a
single compiled firmware image, so nothing installs itself onto a running
device. What actually happens when you submit an app:

1. You write a self-contained `App` subclass and submit it (see below).
2. A maintainer reviews it and merges it as a PR that adds
   `community-apps/<your-app>/`.
3. A GitHub Action (`.github/workflows/community-firmware.yml`) rebuilds
   **CydOs Community Edition** - every merged community app baked into
   one firmware image alongside the standard apps - and republishes
   `docs/firmware/CydOsCommunity.bin` automatically.
4. Anyone can then flash that one file from the
   [App Store page](https://coldzeeyt.github.io/cydos/store.html) with
   the same one-click web installer the main site uses. That's the
   "install" step - still a full reflash under the hood, just automated
   so nobody has to touch PlatformIO or git to get your app.

The official `CydOs.bin` on the main [Flash page](https://coldzeeyt.github.io/cydos/)
is unaffected either way - it only ever contains the apps in this doc's
main README, built and released manually like always.

This doc covers writing an app and getting it into that pipeline.

## The `App` interface

Every app implements [`src/apps/App.h`](src/apps/App.h):

```cpp
class App {
public:
  virtual const char* name() const = 0;

  // Called once when the app becomes active. Content area is already
  // cleared to the theme background.
  virtual void onEnter(TFT_eSPI& tft) {}

  // Called once when leaving the app (e.g. user hit Home).
  virtual void onExit() {}

  // Called every main-loop tick. Use for animation/timers/non-blocking
  // polling. Return true if the app needs a redraw this tick.
  virtual bool update() { return false; }

  // Full (or partial) redraw of the content area. Required.
  virtual void draw(TFT_eSPI& tft) = 0;

  // down=true on the tick a press starts, false while held/dragging.
  // (x, y) are full-screen coordinates.
  virtual void onTouch(TFT_eSPI& tft, int16_t x, int16_t y, bool down) {}

  virtual void onTouchUp() {}
};
```

Look at an existing simple app for the shape of it - `DiceApp` or
`CalculatorApp` are good, small starting points. `src/core/UI.h` has the
button/slider widgets, icon-drawing helpers, and the `Theme` color
palette every built-in app uses; reuse it so your app looks native
instead of hand-rolling widgets.

A few conventions the built-in apps all follow, worth keeping:

- **Dirty-flag rendering.** Keep a `bool _dirty` member, set it whenever
  state changes, and have `draw()` bail out immediately if it's false.
  Full-screen SPI redraws every frame are visibly slow on this display.
- **Respect `Cfg::STATUS_BAR_H`.** `AppManager` owns everything above
  that line (title, battery, back). Your app only draws below it.
- **No blocking calls in `update()` or `draw()`.** They run on every
  main-loop tick; a `delay()` or blocking network call in either one
  freezes touch input and every other app's timing (see the Morse timing
  fix in the commit history for what happens when this gets missed - the
  periodic update-checker's blocking WiFi connect was scrambling the
  flash timing until it got excluded).
- **No dynamic allocation in the hot path.** Fixed-size buffers (see
  `MorseBeaconApp`'s `_text[MAX_LEN + 1]`) - this is a 320KB-RAM
  microcontroller, not a phone.

## Two ways to add an app

**Built into the base firmware** (what every app in the main README is):
fork the repo, add your app under `src/apps/`, register it directly in
`src/main.cpp`:
```cpp
YourApp yourApp;
uint8_t yourIdx = appManager.registerApp(&yourApp);
homeApp.addTile(UI::iconYourThing, yourIdx); // add an icon fn to UI.h if you need one
```
This is for apps general enough that everyone should have them - expect
more scrutiny in review, since it changes what every device ships with.

**A community app** (everything else - anything hitting a personal API, a
specific home-automation setup, or just not something everyone needs):
goes under `community-apps/<slug>/` instead, and only ends up in the
opt-in Community Edition build, never the official firmware. This is the
path most submissions should take. See "Getting listed" below.

Either way, before submitting:

1. Build it with PlatformIO and try it on a device, or at least mirror
   the logic into `docs/demo.html` (see below) and click through it
   there. Type-checking and a clean build are not the same as the
   feature actually working - test the golden path and a couple of edge
   cases by hand.
2. If you're touching anything WiFi/HTTP-related, write setup docs and
   consider an in-app help screen for anything non-obvious - see how the
   OBS app's README section and its on-device `?` help screen stay in
   sync.

## Mirroring into the browser demo (optional but appreciated)

`docs/demo.html` is a from-scratch JS/Canvas reimplementation of the
whole launcher - same 320x240 logical resolution, same theme, same
gesture handling - so people can try CydOs with no hardware. It is not
required for a submission, but a PR that includes a demo version of the
new app is much easier to review (screenshots in a PR description are
nice; a demo people can actually click through is better). If your app
does hardware-specific things (WiFi scanning, a companion script over
LAN), simulate rather than actually perform them in the demo - see how
WiFi Radar uses invented networks and the OBS app's demo "Sync" button
loads sample scene names instead of really fetching, since a page served
over `https://` can't reach a plain-`http://` LAN device anyway
(mixed-content blocking).

## Getting listed in the App Store

The easiest way is the **[Generate a submission](https://coldzeeyt.github.io/cydos/store.html)**
form on the store page itself: fill in the name/author/icon/description,
upload your app's header file, and it opens a pre-filled GitHub issue for
you - no git required. A maintainer turns that into the PR described
below.

To do it yourself with git, create `community-apps/<slug>/` (lowercase
letters, digits, `-`/`_` only) containing:

- Your app's source file(s) - a single self-contained header is simplest;
  a header+cpp pair works too.
- `app.json`:
  ```json
  {
    "name": "Your App Name",
    "author": "your GitHub handle or name",
    "description": "One sentence, 140 characters or fewer - what it does, not how.",
    "icon": "🎲",
    "class": "YourAppClassName",
    "header": "YourApp.h",
    "sources": ["YourApp.h"],
    "repo": "https://github.com/you/your-fork-or-repo"
  }
  ```
  - `class` must exactly match your `App` subclass's name and be
    default-constructible (no required constructor arguments) - the
    generated glue does `static YourAppClassName app;`.
  - `sources` lists every file to copy into the build (header first,
    plus a `.cpp` if you have one); `header` must be one of them.
  - `icon` is shown on the store page only - the device can't render
    emoji, so your Home-screen tile uses a generic icon instead.
  - `repo` is optional but recommended - link wherever you'll keep the
    app updated.

Verify it locally before opening the PR:
```bash
python3 scripts/generate_community.py   # writes src/community_registration.inc + docs/store.json
pio run -e cyd                          # builds with your app included
git checkout -- src/community_registration.inc docs/store.json  # don't commit the generated output
```
(`community-apps/_example/` is a working reference template you can copy
- it's excluded from the store listing on purpose, since it's not a real
submission.)

Open the PR against `coldzeeyt/cydos` with just your new
`community-apps/<slug>/` folder. Once merged, CI regenerates
`docs/store.json` and rebuilds `docs/firmware/CydOsCommunity.bin`
automatically - nothing else to update by hand. There's a cap of 8
community apps per build (`MAX_COMMUNITY_APPS` in
`scripts/generate_community.py`); the generator fails the build loudly,
before it reaches anyone, if that's exceeded.

## Everything else

For changes to the base firmware itself (not a new app), the normal
rules in the main [README](README.md) apply: keep it simple, don't add
speculative configuration, and update `docs/demo.html` alongside any
firmware change so the two don't drift.
