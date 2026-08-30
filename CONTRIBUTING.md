# Contributing to CydOs

There are three ways to add something to CydOs, in order of how much
commitment they need:

1. **[SD Card Apps](#sd-card-apps)** - a plain-text file on a microSD
   card. No code, no reflash, no PR even required (you can just make one
   for yourself) - see that section for the format. Requires the CYD to
   have a microSD card in it.
2. **[Community Edition apps](#community-edition-apps)** - real C++
   against the `App` interface below, submitted via PR, automatically
   compiled into a separate opt-in firmware anyone can flash. No SD card
   needed, but it is a full reflash.
3. **Built into the base firmware** - for apps general enough that
   everyone should have them. Fork the repo, add your app under
   `src/apps/`, register it in `src/main.cpp` directly. Expect more
   scrutiny in review, since it changes what every device ships with -
   most submissions should go through Community Edition instead.

This doc covers all three, plus [wallpapers](#wallpapers) (not code at
all, just an image).

## The `App` interface

Both code paths (Community Edition and built-in) implement
[`src/apps/App.h`](src/apps/App.h):

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

## Built into the base firmware, or Community Edition?

Fork the repo, add your app under `src/apps/`, register it directly in
`src/main.cpp`:
```cpp
YourApp yourApp;
uint8_t yourIdx = appManager.registerApp(&yourApp);
homeApp.addTile(UI::iconYourThing, yourIdx); // add an icon fn to UI.h if you need one
```
Do this only for apps general enough that everyone should have them -
expect more scrutiny in review, since it changes what every device ships
with. Everything else (anything hitting a personal API, a specific
home-automation setup, or just not something everyone needs) goes under
`community-apps/<slug>/` instead, and only ends up in the opt-in
Community Edition build, never the official firmware. This is the path
most submissions should take - see [Community Edition apps](#community-edition-apps)
below.

**Why compiled apps can't just live on the SD card too:** the CYD has no
operating system, no process loader, nothing that can map and run
arbitrary machine code at runtime the way a phone or PC loads an app -
Arduino/ESP32-IDF firmware is one single compiled binary, full stop.
Community Edition apps still need a real flash because of that; the SD
card path (below) is for content that doesn't need to be compiled at
all, not a way to sideload compiled ones.

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

## Community Edition apps

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

## SD Card Apps

The lightweight, no-code path - and the only one that doesn't need a PR
at all if you're just making one for yourself. Requires a microSD card
(FAT32) in the CYD. Not a general programming interface - a declarative
static screen: a name, a background color, and a few lines of text, read
from a plain-text file by [`src/apps/SdCardApp.h`](src/apps/SdCardApp.h).

Create `/cydos_apps/yourfile.cydapp` on the card:
```
name=WiFi Card
bg=#101820
text=WiFi: MyNetwork
text=Pass: hunter2
```
- One `key=value` per line; blank lines and unknown keys are ignored
  (forward-compatible with fields a future build might add).
- `name` - shown as the tile label and the app's title bar. Defaults to
  "SD App" if omitted.
- `bg` - `#RRGGBB` hex background color. Defaults to the theme background.
- `text` - up to 6 lines, each shown centered, stacked, in the order they
  appear. With none, the screen just shows `name` centered instead.

CydOs scans `/cydos_apps/*.cydapp` once at boot (up to 6 files) and adds
a Home tile for each one it can parse - editing the card takes effect on
the next power-cycle, not live. No submission process needed for
personal use; if you want it listed on the store page's directory of
example files, open a PR adding it under `community-apps/` alongside an
`app.json` the same shape as a Community Edition one (see above) so
others can find and copy it - the file itself still only ever runs off
an SD card, never compiled into any firmware.

**Hardware note:** the SD slot shares its SPI bus with the touch
controller on this board (same CLK/MOSI/MISO, separate CS lines - see
`Cfg::SD_CS` in `include/Config.h` and `src/core/SdCard.h`). This was
written against the CYD's documented wiring but not verified against a
real card in real hardware - if apps aren't being picked up, that
sharing is the first thing to check.

## Wallpapers

Home's background image, also read from the SD card -
[`src/core/Wallpaper.h`](src/core/Wallpaper.h) looks for
`/cydos_wallpaper.bmp` at boot. Format: a 24-bit uncompressed BMP,
exactly 320×214 pixels (the content area below the status bar - not the
full 320×240 screen). Getting an arbitrary image into that exact format
by hand is annoying, so don't - use the **Wallpaper Creator** on the
[store page](https://coldzeeyt.github.io/cydos/store.html)'s Wallpapers
tab: upload any image, it crops to fit client-side (a `<canvas>` and a
~40-line BMP encoder, no upload to any server), and download the result
already correctly formatted.

To submit one to the gallery, use the store page's "Submit a wallpaper"
form to generate the issue text (name/author/description), then attach
the image file to that issue yourself before posting it - GitHub issues
can't be pre-filled with an attached file, only text. A maintainer adds
it to `docs/wallpapers/` and an entry to `docs/wallpapers.json`
(`name`, `author`, `description`, `image` - a `wallpapers/...` relative
path - and optionally `repo`).

Same hardware caveat as SD Card Apps above - the BMP-reading logic was
checked byte-for-byte against real BMP files during development
(including round-tripping one through the store page's own encoder), but
never against the actual SD card hardware.

## Everything else

For changes to the base firmware itself (not a new app), the normal
rules in the main [README](README.md) apply: keep it simple, don't add
speculative configuration, and update `docs/demo.html` alongside any
firmware change so the two don't drift.
