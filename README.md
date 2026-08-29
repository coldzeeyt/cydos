# CydOs

A tiny, fast "operating system" for the ESP32-2432S028R **Cheap Yellow
Display** (CYD) — a home screen you tap to launch small full-screen apps,
built with PlatformIO + Arduino.

## Apps

- **WiFi Radar** — sweeping radar view of nearby networks. Each SSID gets
  a stable blip position; distance from center tracks signal strength, so
  walking toward a stronger signal visibly pulls its blip inward. Tap a
  blip for SSID/RSSI/channel/security details.
- **Flashlight** — full white or full red screen with a brightness slider
  that drives the backlight PWM directly.
- **Clock** — one app, three tabs: a manually-set clock, a stopwatch, and
  a countdown timer.
- **QR Beamer** — type a short message on an on-screen keyboard, beam it
  as a QR code for another phone to scan.
- **Dice** — D6, D20, and coin flip, with a quick roll animation.
- **Settings** — global brightness (persisted across reboots) and a touch
  test screen for calibrating the resistive touch panel.

## Web flasher & browser demo

[**coldzeeyt.github.io/cydproject**](https://coldzeeyt.github.io/cydproject/) —
flash CydOs onto a CYD straight from Chrome/Edge over Web Serial (no
Arduino IDE, no esptool), or try the [interactive
demo](https://coldzeeyt.github.io/cydproject/demo.html) first, which runs
the actual launcher and apps in a canvas, no hardware required.

This is served from `docs/` via GitHub Pages. If the site above 404s, Pages
hasn't been turned on yet for this repo — under **Settings → Pages**, set
Source to "Deploy from a branch" and pick this branch with the `/docs`
folder. `docs/firmware/CydOs.bin` is a merged image (bootloader +
partitions + app in one file, built from `pio run` and
`esptool.py merge_bin`); rebuild and copy it there after firmware changes,
or the web flasher will keep serving stale firmware.

## Hardware

Targets the common **ESP32-2432S028R** CYD board (ILI9341 320x240 SPI
display + XPT2046 resistive touch, ESP32-WROOM-32). All pin numbers live
in `include/Config.h` — check that file first if your board revision
wires things differently.

### Battery (your own addition)

CydOs shows a battery pill in the status bar if you wire up a voltage
divider:

1. Two resistors (e.g. 100k + 100k) from BAT+ to GND, midpoint to GPIO35.
2. This halves a ~4.2V LiPo down to a safe ~2.1V for the ESP32's ADC.
3. If you use a different pin or ratio, update `Cfg::BATTERY_ADC_PIN` /
   `Cfg::BATTERY_DIVIDER_RATIO` in `include/Config.h`.
4. Don't have a battery wired up yet? Set `Cfg::BATTERY_MONITOR_ENABLED`
   to `false` and the status bar just hides the battery icon.

## Building & flashing

Requires [PlatformIO](https://platformio.org/) (CLI or the VS Code
extension).

```bash
pio run                # build
pio run -t upload       # flash over USB
pio device monitor      # serial monitor (115200 baud)
```

First build downloads the ESP32 toolchain + libraries automatically
(TFT_eSPI, XPT2046_Touchscreen, QRCode) — no manual library setup needed,
TFT_eSPI is fully configured via `build_flags` in `platformio.ini`.

## If the screen or touch looks wrong

- **Colors inverted / wrong** → in `platformio.ini`, swap
  `ILI9341_2_DRIVER` for `ILI9341_DRIVER` (or vice versa).
- **Upside down** → change `Cfg::SCREEN_ROTATION` in `Config.h` (try `3`).
- **Touch offset / mirrored** → open Settings > Touch Test on the device
  and drag your finger around while watching the raw numbers; tune
  `Cfg::TOUCH_RAW_X_MIN/MAX`, `TOUCH_RAW_Y_MIN/MAX`, and the
  `TOUCH_SWAP_XY` / `TOUCH_INVERT_X` / `TOUCH_INVERT_Y` flags in
  `Config.h` until the dot tracks your finger exactly.

## Project layout

```
include/Config.h        all hardware pins & tunables - start here
src/main.cpp             boot + app registration
src/core/                display/touch/battery drivers, UI toolkit, app manager
src/apps/                one file (or header+cpp) per app
docs/                    GitHub Pages site: web flasher (index.html) + browser demo (demo.html)
```

Adding a new app: implement the `App` interface in `src/apps/`, register
it in `src/main.cpp` with `appManager.registerApp(&yourApp)`, and give it
a launcher tile with `homeApp.addTile(iconFn, index)`.
