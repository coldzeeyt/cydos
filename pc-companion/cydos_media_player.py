#!/usr/bin/env python3
"""
CydOs Media Player companion - the CYD has no speaker or audio output of
any kind (it's a display/touch board, nothing more), so this is what
actually plays sound. It's a small standalone HTTP server (same "run this
on your PC, point the CYD app at its LAN IP" shape as
obs-script/cydos_scene_switcher.py and spotify-script/cydos_now_playing.py)
that scans a music folder and plays files through *this PC's* speakers -
the CydOs Media Player app is a remote control for it, not a player
itself.

Setup:
  1. pip install pygame   (the one dependency - handles mp3/wav/ogg
     decoding and playback; unlike the OBS/Spotify companions this one
     can't get away with the standard library alone, since Python has no
     built-in audio codec/output support)
  2. python3 cydos_media_player.py --dir "C:\\Users\\you\\Music"
     (--dir defaults to the current folder if omitted)
  3. On your CYD: Media Player app -> Edit -> enter this PC's LAN IP and
     the port below (default 8095).

Format support: .mp3, .wav, .ogg play natively via pygame.mixer. .mp4 and
other video containers are NOT decoded here - pygame.mixer is an audio
mixer, not a video/container demuxer. If you have audio-only rips
(mp3/wav/ogg) this handles them directly; for audio trapped inside mp4s,
extract it first (e.g. `ffmpeg -i in.mp4 -vn out.mp3`).

Endpoints (all GET, all return JSON, CORS-open for the CYD's own use):
  /tracks              -> {"tracks": ["song1.mp3", "song2.wav", ...]}
  /play?track=NAME      -> starts NAME playing; {"ok": bool, "error"?: str}
  /pause                -> pauses the current track
  /resume               -> resumes a paused track
  /stop                 -> stops playback entirely
  /next                 -> plays the next track in the scanned list
  /prev                 -> plays the previous track in the scanned list
  /volume?level=0-100    -> sets playback volume
  /status               -> {"playing": bool, "track": str, "volume": int}

No authentication - meant for your own LAN, don't port-forward it.
"""
import argparse
import json
import threading
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

try:
    import pygame
except ImportError:
    raise SystemExit("This needs pygame - run: pip install pygame")

EXTENSIONS = (".mp3", ".wav", ".ogg")
DEFAULT_PORT = 8095


class Player:
    def __init__(self, music_dir: Path):
        self.music_dir = music_dir
        self.lock = threading.Lock()
        self.current = ""
        self.paused = False
        self.volume = 70
        pygame.mixer.init()
        pygame.mixer.music.set_volume(self.volume / 100)

    def tracks(self):
        return sorted(p.name for p in self.music_dir.iterdir() if p.suffix.lower() in EXTENSIONS)

    def play(self, name: str):
        path = self.music_dir / name
        if not path.is_file() or path.suffix.lower() not in EXTENSIONS:
            return False, "not found"
        with self.lock:
            try:
                pygame.mixer.music.load(str(path))
                pygame.mixer.music.play()
                pygame.mixer.music.set_volume(self.volume / 100)
                self.current = name
                self.paused = False
            except Exception as e:
                return False, str(e)
        return True, None

    def play_offset(self, offset: int):
        names = self.tracks()
        if not names:
            return False, "no tracks"
        if self.current in names:
            idx = (names.index(self.current) + offset) % len(names)
        else:
            idx = 0
        return self.play(names[idx])

    def pause(self):
        with self.lock:
            if pygame.mixer.music.get_busy() and not self.paused:
                pygame.mixer.music.pause()
                self.paused = True

    def resume(self):
        with self.lock:
            if self.paused:
                pygame.mixer.music.unpause()
                self.paused = False

    def stop(self):
        with self.lock:
            pygame.mixer.music.stop()
            self.current = ""
            self.paused = False

    def set_volume(self, level: int):
        level = max(0, min(100, level))
        with self.lock:
            self.volume = level
            pygame.mixer.music.set_volume(level / 100)

    def status(self):
        with self.lock:
            playing = pygame.mixer.music.get_busy() and not self.paused
            return {"playing": playing, "track": self.current, "volume": self.volume}


def make_handler(player: Player):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def _json(self, obj, status=200):
            body = json.dumps(obj).encode("utf-8")
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            parsed = urllib.parse.urlparse(self.path)
            qs = urllib.parse.parse_qs(parsed.query)

            if parsed.path == "/tracks":
                self._json({"tracks": player.tracks()})
            elif parsed.path == "/play":
                name = urllib.parse.unquote(qs.get("track", [""])[0])
                ok, err = self.play_track(name)
                self._json({"ok": ok, "error": err} if not ok else {"ok": True})
            elif parsed.path == "/pause":
                player.pause()
                self._json({"ok": True})
            elif parsed.path == "/resume":
                player.resume()
                self._json({"ok": True})
            elif parsed.path == "/stop":
                player.stop()
                self._json({"ok": True})
            elif parsed.path == "/next":
                ok, err = player.play_offset(1)
                self._json({"ok": ok, "error": err} if not ok else {"ok": True})
            elif parsed.path == "/prev":
                ok, err = player.play_offset(-1)
                self._json({"ok": ok, "error": err} if not ok else {"ok": True})
            elif parsed.path == "/volume":
                try:
                    level = int(qs.get("level", ["70"])[0])
                except ValueError:
                    level = 70
                player.set_volume(level)
                self._json({"ok": True})
            elif parsed.path == "/status":
                self._json(player.status())
            else:
                self._json({"error": "not found"}, status=404)

        def play_track(self, name):
            if not name:
                return False, "no track given"
            return player.play(name)

    return Handler


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=".", help="Folder of .mp3/.wav/.ogg files to serve (default: current folder)")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    args = ap.parse_args()

    music_dir = Path(args.dir).expanduser().resolve()
    if not music_dir.is_dir():
        raise SystemExit(f"Not a folder: {music_dir}")

    player = Player(music_dir)
    found = len(player.tracks())
    print(f"CydOs Media Player: serving {found} track(s) from {music_dir}")
    if found == 0:
        print(f"(no .mp3/.wav/.ogg files found directly in that folder - subfolders aren't scanned)")

    httpd = ThreadingHTTPServer(("0.0.0.0", args.port), make_handler(player))
    print(f"Listening on 0.0.0.0:{args.port} - point the CYD's Media Player app at this PC's LAN IP and that port")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
