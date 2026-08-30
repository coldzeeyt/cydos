# CydOs

A tiny, fast "operating system" for the ESP32-2432S028R **Cheap Yellow
Display** (CYD) — a home screen you tap to launch small full-screen apps,
built with PlatformIO + Arduino.

**Open source, MIT licensed** (see [`LICENSE`](LICENSE)) — fork it, wire it
up differently, add your own app, send a PR. No account, subscription, or
cloud service required to build or use it.

## Apps

The home screen is a 3x3 grid per page; swipe left/right when there are
more than nine apps (a row of dots at the bottom shows which page you're
on). A tap opens a tile, a drag past a small threshold flips the page -
same gesture the rest of the launcher uses to tell a tap from a drag.

- **WiFi Radar** — sweeping radar view of nearby networks. Each SSID gets
  a stable blip position; distance from center tracks signal strength, so
  walking toward a stronger signal visibly pulls its blip inward. Tap a
  blip for SSID/RSSI/channel/security details.
- **Flashlight** — full-screen light in white, red, amber, cyan, or green,
  with a brightness slider that drives the backlight PWM directly. **Sleep**
  turns the screen black (and the backlight off) to save battery or avoid
  blinding anyone at night - tap anywhere to wake it back up to whatever
  color/brightness you had.
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
- **Morse** — two tabs. **Send**: type a message (or tap the one-button
  **SOS**) and flash it out in Morse code with the screen - a visual
  signal for getting someone's attention at a distance. Shows the
  dot/dash pattern as you type and again while it's flashing, so you can
  follow along or learn the code, not just read the plain-text message.
  **Decode**: the reverse direction - big **DOT**/**DASH** buttons to key
  in a code by hand, **Gap** to close out a letter, **Word** to close out
  a word, and the translated text builds up live as you go.
- **Browser** — a "browser" in the loosest sense: type a URL, it fetches
  the page over WiFi, strips every tag, and shows whatever text is left,
  word-wrapped and scrollable. No CSS, no images, no JS - just enough to
  read an article or check a status page from a 320x240 screen.
- **OBS** — a 3x3 grid of your real OBS scenes, wired up to a companion
  script that runs inside OBS itself (see [OBS scene switcher](#obs-scene-switcher)
  below). Tap a tile to switch scenes live; the grid shows your actual
  scene names once synced.
- **Spotify** — shows what's currently playing on your account (track,
  artist, album, a progress bar), via a small companion script on your PC
  (see [Spotify Now Playing](#spotify-now-playing) below).
- **Files** — a plain SD-card file browser. Navigate folders, see file
  sizes, delete files or empty folders. Read-only beyond delete - no
  create/rename, since there's no keyboard-free way to name something new
  that's worth the screen space this device has.
- **Settings** — global brightness (persisted across reboots), a manual
  time set (Clock reads the same clock), a battery-icon show/hide toggle,
  a Lock Screen toggle (see below), a **Wallpapers** picker (switch
  between the SD card's default wallpaper and anything you drop in
  `/cydos_wallpapers/`, with a live preview before applying), WiFi setup
  (used only for the "New Update!" check-in below), a touch test screen
  for calibrating the resistive touch panel, and a **Dev Mode** toggle
  that reveals the Diagnostics app's Home tile (off by default; takes
  effect on the next boot).
- **Diagnostics** *(Dev Mode only)* — a hardware check with three tabs:
  **Display** (solid red/green/blue/white/black fills, a checkerboard for
  catching a stuck pixel a solid fill would hide, and grayscale/RGB
  gradients for spotting color banding - tap anywhere to cycle), **Touch**
  (a live crosshair under your finger, plus the same raw ADC readout as
  Settings' Touch Test), and **Info** (chip/CPU, reset reason, heap and
  sketch space, flash size, uptime, SD card size, battery voltage, WiFi
  status).
- **App Store** — two tabs. **Installed**: every Community Edition app
  (see below) baked into this firmware build, in one shared 3x3 grid
  instead of each claiming its own top-level Home tile. **Get More**:
  fetches a small catalog from the website over WiFi and downloads a
  wallpaper or SD Card App straight onto the SD card - a downloaded app
  becomes a real Home tile immediately, no reboot (see the App Store
  section below for how this differs from Community Edition apps, which
  can't work this way).
- **Notes** — one persistent plain-text note (up to 200 characters), typed
  on the same on-screen keyboard as OBS/Spotify/WiFi setup and saved
  across reboots. Not a multi-note app - just a single scratchpad that's
  always there.
- **Converter** — unit conversion across three tabs: **Length** (m/ft),
  **Weight** (kg/lb), and **Temp** (C/F). Type a number on the keypad, read
  the converted value live above it, **Swap** flips which unit you're
  typing in.

## Lock Screen

Off by default. Turn it on in **Settings → Lock Screen** and the next
time you power on the device, it boots straight to a lock screen (time +
"Double-tap to unlock") instead of Home — two taps anywhere unlock it for
the rest of that session. It shows your current wallpaper (see
**Settings → Wallpapers** above) behind the clock, same as Home. It doesn't
re-lock when you go back to Home from an app; this is a "keep it off your
screen at a glance" feature, not a security boundary — there's no PIN,
and anyone can unlock it the same way you do. Flipping the setting takes
effect on the next boot, not immediately, so turning it on from inside
Settings can't lock you out of
Settings itself.

## Web flasher & browser demo

[**coldzeeyt.github.io/cydos**](https://coldzeeyt.github.io/cydos/) —
flash CydOs onto a CYD straight from Chrome/Edge over Web Serial (no
Arduino IDE, no esptool install needed), or try the [interactive
demo](https://coldzeeyt.github.io/cydos/demo.html) first, which runs
the actual launcher and apps in a canvas, no hardware required. Prefer to
flash it yourself with esptool or the Arduino IDE? There's a plain
[download link for the .bin](https://coldzeeyt.github.io/cydos/firmware/CydOs.bin)
on the flasher page too (offset `0x0`, it's the bootloader+partitions+app
already merged into one file).

This is served from `docs/` via GitHub Pages. If the site above 404s, Pages
hasn't been turned on yet for this repo — under **Settings → Pages**, set
Source to "Deploy from a branch" and pick this branch with the `/docs`
folder. `docs/firmware/CydOs.bin` is a merged image (bootloader +
partitions + app in one file, built from `pio run` and
`esptool.py merge_bin`); rebuild and copy it there after firmware changes,
or the web flasher will keep serving stale firmware.

## App Store

[**coldzeeyt.github.io/cydos/store.html**](https://coldzeeyt.github.io/cydos/store.html)
— apps and wallpapers other people made for CydOs. **The real "install"
path requires a microSD card in your CYD** — everything below except
Community Edition reads files straight off the card, no reflash needed.

- **SD Card Apps** — simple, no-code screens. Put a plain-text
  `name=`/`bg=`/`text=` file (see [`CONTRIBUTING.md`](CONTRIBUTING.md)
  for the format) at `/cydos_apps/yourfile.cydapp` on a FAT32 microSD
  card, power-cycle the CYD, and it shows up as a Home tile — up to 6 at
  once. Editing the card by hand takes effect on the next boot, not live -
  but see **App Store → Get More** below for a way that doesn't.
- **Wallpapers** — the store page has an in-browser creator: upload any
  image, it crops to fit, and you download a `.bmp` already in the exact
  format CydOs reads. Drop one at `/cydos_wallpaper.bmp` on the card and
  it becomes Home's background on next boot - or drop several into
  `/cydos_wallpapers/` and switch between them any time from
  **Settings → Wallpapers**, with a live preview before you apply one.
- **App Store → Get More (WiFi, no SD card handling)** — the on-device
  **App Store** app's second tab fetches a small catalog straight from
  the website over WiFi and downloads whatever you pick directly onto the
  SD card - no computer needed at all. A wallpaper just becomes available
  in Settings → Wallpapers; an SD Card App becomes a real Home tile
  immediately, no reboot. This only carries SD Card Apps and wallpapers -
  Community Edition apps are compiled into the firmware image itself, and
  this hardware has no way to load compiled code at runtime, so those
  still need the reflash described below no matter what.
- **Community Edition** — for real interactive apps (not static
  screens), written in C++ against the same `App` interface the built-in
  apps use. No SD card needed, but it's a full firmware reflash: submit
  through the store page's "Generate a submission" form (packages your
  code into a pre-filled GitHub issue, no git required), a maintainer
  reviews and merges it, and CI automatically bakes every merged app into
  a separate, opt-in **Community Edition** firmware you flash like
  normal. The official `CydOs.bin` above never changes because of this.
  On the device, every Community Edition app shows up together inside the
  **App Store** Home tile (see the Apps section above) rather than each
  claiming its own top-level tile.

Everything here is empty until someone submits the first one. See
[`CONTRIBUTING.md`](CONTRIBUTING.md) for the exact `.cydapp` field
reference, the wallpaper file format, the `App` interface, and the full
submission formats.

**Caveat worth knowing:** the SD card support (reading the shared SPI bus
with the touch controller, decoding BMP wallpapers) was written against
the CYD's documented wiring but hasn't been verified on real hardware
with a card inserted — if `.cydapp` files or a wallpaper aren't picked up,
please open an issue with what you tried.

## OBS scene switcher

The OBS app talks directly to a small Python script that runs inside OBS
itself — no separate program to install or keep running, and no compiled
plugin to build. The **OBS** app also has its own **?** help button (on
both the host-entry screen and the scene grid) with this same walkthrough
for Windows, in case you're setting it up away from a computer.

**Things you need first (Windows):**

- **Python, and OBS pointed at it.** OBS's scripting console doesn't
  bundle its own Python — if `Tools → Scripts` shows a red warning at the
  bottom instead of letting you add a script, you need this step.
  1. Install [Python 3.9 or 3.10 for Windows](https://www.python.org/downloads/windows/)
     (OBS's bundled script engine doesn't support newer 3.11+ yet on every
     OBS version — 3.9/3.10 is the safe choice).
  2. In OBS: **Tools → Scripts → Python Settings** tab, and browse to the
     folder where Python installed (e.g.
     `C:\Users\<you>\AppData\Local\Programs\Python\Python310`).
- **Your PC's LAN IP**, so the CYD knows where to send requests:
  1. Open **Command Prompt** (Win+R, type `cmd`, Enter).
  2. Run `ipconfig`.
  3. Under your active **Wi-Fi** or **Ethernet adapter**, read the
     **IPv4 Address** line (e.g. `192.168.1.50`).

**Setup:**

1. In OBS: **Tools → Scripts → +** and add
   [`obs-script/cydos_scene_switcher.py`](obs-script/cydos_scene_switcher.py).
2. Set the **Port** field if you don't want the default `8088`.
3. That's it — the script starts a tiny local HTTP server for as long as
   OBS and the script are loaded:
   - `GET /scenes` → JSON array of your scene names.
   - `GET /switch?scene=Name` → switches to that scene (or pass a 1-based
     number instead of a name).
4. On the CYD, open the **OBS** app, tap **Edit**, and type your PC's IP
   and the port from step 2 (e.g. `192.168.1.50:8088`) on the on-screen
   keyboard, then **Done**. Tap **Sync** to pull your real scene names
   into the grid.
5. Tap any tile to switch OBS to that scene — a green border confirms it
   worked, red means the request failed (wrong host/port, OBS closed,
   script not running, a Windows Firewall prompt you haven't answered
   yet, etc).

No authentication, so only run it on a network you trust — it's meant for
your own LAN, don't port-forward the port. The CYD only talks to it while
the OBS app is open (WiFi connects on entering the app, disconnects on
leaving), so it doesn't fight the "New Update!" WiFi check-in below.

## Spotify Now Playing

Unlike OBS, Spotify has no built-in scripting console to hook into, so
this is a small **standalone script** you run yourself (once logged in,
it keeps running in the background) rather than something you load
inside another app. The **Spotify** app also has its own **?** help
button (on both the host-entry screen and the now-playing screen) with
this same walkthrough, in case you're setting it up away from a computer.

**Before you start:** you need Python installed on the PC that'll run the
script (not the CYD) - any Python **3.9 or newer** works, unlike OBS's
bundled scripting console above, since this runs as your own standalone
script with no OBS version constraint.
- **Windows**: [python.org/downloads/windows](https://www.python.org/downloads/windows/)
  → grab the latest **"Windows installer (64-bit)"** under Stable
  Releases. During install, check **"Add python.exe to PATH"** on the
  first screen, or the `python3` command below won't be found.
- **macOS**: already has Python 3 on modern versions, or `brew install python3`.
- **Linux**: already installed on virtually every distro (`python3 --version` to check).

**One-time setup:**

1. Create a free app at the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard)
   and add `http://127.0.0.1:8899/callback` as a Redirect URI in its
   settings. Note the app's **Client ID** and **Client Secret**.
2. Run:
   ```bash
   python3 spotify-script/cydos_now_playing.py --client-id YOUR_ID --client-secret YOUR_SECRET
   ```
   This opens your browser for a one-time Spotify login/consent, then
   saves a refresh token next to the script (`cydos_spotify_token.json`)
   so future runs (`python3 cydos_now_playing.py`, no arguments needed)
   log in automatically.
3. Leave it running. It serves `GET /now-playing` on port `8090` by
   default (`--port` to change it) with the track/artist/album/progress
   Spotify reports, polled every few seconds.
4. On the CYD: open the **Spotify** app, tap **Edit**, enter this PC's
   LAN IP and the port (same "find your PC's IP" steps as OBS, above),
   **Done**.

No authentication on the local server (same LAN-only assumption as OBS -
don't port-forward it), and it only uses Python's standard library, no
`pip install` needed.

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
- A microSD card, FAT32-formatted — optional, only needed for
  [SD Card Apps and wallpapers](#app-store). Most CYD boards have the slot
  built in.

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
obs-script/              cydos_scene_switcher.py - the OBS-side companion script (see above)
```

Adding a new app: implement the `App` interface in `src/apps/`, register
it in `src/main.cpp` with `appManager.registerApp(&yourApp)`, and give it
a launcher tile with `homeApp.addTile(iconFn, index)`. See
[`CONTRIBUTING.md`](CONTRIBUTING.md) for the full interface and how to
get it listed on the [App Store](#app-store) page.
