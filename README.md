# CydOs

A tiny, fast "operating system" for the ESP32-2432S028R **Cheap Yellow
Display** (CYD) — a home screen you tap to launch small full-screen apps,
built with PlatformIO + Arduino.

**Open source, MIT licensed** (see [`LICENSE`](LICENSE)) — fork it, wire it
up differently, add your own app, send a PR. No account, subscription, or
cloud service required to build or use it.

## Apps

- **WiFi Radar** — sweeping radar view of nearby networks. Each SSID gets
  a stable blip position; distance from center tracks signal strength, so
  walking toward a stronger signal visibly pulls its blip inward. Tap a
  blip for SSID/RSSI/channel/security details.
- **Flashlight** — full white or full red screen with a brightness slider
  that drives the backlight PWM directly.
- **Clock** — one app, three tabs: a manually-set clock, a stopwatch, and
  a countdown timer that flashes the screen red/white when it hits zero
  (no speaker to beep with).
- **QR Beamer** — type a short message on an on-screen keyboard (letters,
  digits, and `./:-_@` for URLs), beam it as a QR code for another phone
  to scan.
- **Dice** — D6, D20, and coin flip, with a quick roll animation.
- **Calculator** — plain four-function calculator.
- **Password** — generates a random password (configurable length and
  character set) and can beam it as a QR code so you don't have to type
  it in by hand.
- **Morse** — type a message (or tap the one-button **SOS**) and flash it
  out in Morse code with the screen - a visual signal for getting
  someone's attention at a distance.
- **Settings** — global brightness (persisted across reboots), a manual
  time set (Clock reads the same clock), a battery-icon show/hide toggle,
  WiFi setup (used only for the "New Update!" check-in below), and a
  touch test screen for calibrating the resistive touch panel.

## Web flasher & browser demo

[**coldzeeyt.github.io/cydproject**](https://coldzeeyt.github.io/cydproject/) —
flash CydOs onto a CYD straight from Chrome/Edge over Web Serial (no
Arduino IDE, no esptool install needed), or try the [interactive
demo](https://coldzeeyt.github.io/cydproject/demo.html) first, which runs
the actual launcher and apps in a canvas, no hardware required. Prefer to
flash it yourself with esptool or the Arduino IDE? There's a plain
[download link for the .bin](https://coldzeeyt.github.io/cydproject/firmware/CydOs.bin)
on the flasher page too (offset `0x0`, it's the bootloader+partitions+app
already merged into one file).

This is served from `docs/` via GitHub Pages. If the site above 404s, Pages
hasn't been turned on yet for this repo — under **Settings → Pages**, set
Source to "Deploy from a branch" and pick this branch with the `/docs`
folder. `docs/firmware/CydOs.bin` is a merged image (bootloader +
partitions + app in one file, built from `pio run` and
`esptool.py merge_bin`); rebuild and copy it there after firmware changes,
or the web flasher will keep serving stale firmware.

## "New Update!" notifications

Every CYD running CydOs can optionally check in and flag when a newer
version exists — a small amber **NEW: v1.2** badge appears in the status
bar on every screen; tap it to dismiss.

There's no server pushing anything. Each device that has WiFi configured
polls `docs/version.json` on the GitHub Pages site every 20 minutes (and
once shortly after boot), compares it to the version it was built with
(`include/Version.h`), and raises the badge if the site's version is
newer. A device with no WiFi set up, or one that's out of range, simply
never checks in — which is the whole mechanism for "only the ones that
are online get notified": nothing is tracking or targeting devices, they
each independently pull.

**To set up WiFi on a device:** Settings → WiFi Setup → tap the SSID/password
fields to type on the on-screen keyboard (letters, digits, and `-_.@!` —
for anything else, e.g. uppercase or other symbols, hardcode
`Cfg::WIFI_SSID` / `Cfg::WIFI_PASSWORD` in `include/Config.h` before
flashing instead) → **Save**. **Test Now** forces an immediate check-in so
you can confirm it's working without waiting.

**To cut a release that notifies everyone:**
1. Bump `CYDOS_VERSION` in `include/Version.h` (e.g. `"1.2"`) and make
   your firmware changes.
2. Build, flash your own board, confirm it's good.
3. Bump the version in `docs/version.json` to match and push — this is
   the actual trigger. Every CydOs device that checks in from now on will
   see it's out of date and raise the badge.
4. Rebuild `docs/firmware/CydOs.bin` too (see below) so the web flasher
   serves the new version to anyone who goes looking after seeing the badge.

Two things worth knowing: WiFi credentials are stored as plaintext in
flash (fine for a hobby multitool on your own network, don't reuse a
sensitive password); and the HTTPS request skips certificate validation
(`WiFiClientSecure::setInsecure()`) since it's only ever reading a small
public version file, not worth pinning a cert for.

## Hardware

Targets the common **ESP32-2432S028R** CYD board (ILI9341 320x240 SPI
display + XPT2046 resistive touch, ESP32-WROOM-32). All pin numbers live
in `include/Config.h` — check that file first if your board revision
wires things differently.

### Parts used

- [CYD board (ELEGOO 2.8" ESP32 touch display, ILI9341, USB-C)](https://www.amazon.com/dp/B0FJQ6RK39)
- [3.7V LiPo battery, 3000mAh, JST connector](https://www.amazon.com/dp/B0FH9BLZWB) — check the
  connector matches your board (or your divider wiring) before ordering.

Neither link is sponsored — just what's in the build this was written against.

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

Uses the `huge_app.csv` partition table (~3MB for the app, no OTA slot)
instead of the ESP32 Arduino default, which only gives the app ~1.3MB -
the WiFi/TLS stack the update checker needs eats into that fast. These
boards have a full 4MB flash chip and CydOs doesn't need OTA (it's
flashed whole over USB or the web flasher), so there's no downside.

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
docs/                    GitHub Pages site: web flasher (index.html), browser demo (demo.html),
                         version.json (the "New Update!" trigger - see above)
```

Adding a new app: implement the `App` interface in `src/apps/`, register
it in `src/main.cpp` with `appManager.registerApp(&yourApp)`, and give it
a launcher tile with `homeApp.addTile(iconFn, index)`.
