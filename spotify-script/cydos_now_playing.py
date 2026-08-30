#!/usr/bin/env python3
"""
CydOs Spotify Now Playing - a small standalone companion (not an OBS
script - Spotify has nothing to do with OBS, this just follows the same
"run a tiny local HTTP server your CYD can poll" shape as
obs-script/cydos_scene_switcher.py) that exposes what's currently playing
on your Spotify account so the CydOs Spotify app can show it.

One-time setup:
  1. Create a Spotify app at https://developer.spotify.com/dashboard
     (free). Add "http://127.0.0.1:8899/callback" as a Redirect URI in
     its settings.
  2. Note its Client ID and Client Secret.
  3. Run this once to log in:
       python3 cydos_now_playing.py --client-id XXX --client-secret YYY
     It opens your browser for a one-time Spotify login/consent, then
     saves a refresh token to cydos_spotify_token.json next to this
     script so you don't have to log in again.
  4. Leave it running. On your CYD: Spotify app -> Edit -> enter this
     PC's LAN IP and the port below (default 8090).

Endpoints:
  GET /now-playing -> JSON: {"playing": bool, "track": str, "artist": str,
                             "album": str, "progress_ms": int, "duration_ms": int}
                      or {"playing": false} with no track fields when
                      nothing is playing / paused.

No authentication on the local server - meant for your own LAN, don't
port-forward it. Uses only the standard library (urllib, http.server) -
no `pip install` needed.
"""
import argparse
import http.server
import json
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from pathlib import Path

TOKEN_FILE = Path(__file__).resolve().parent / "cydos_spotify_token.json"
AUTH_URL = "https://accounts.spotify.com/authorize"
TOKEN_URL = "https://accounts.spotify.com/api/token"
NOW_PLAYING_URL = "https://api.spotify.com/v1/me/player/currently-playing"
SCOPE = "user-read-currently-playing user-read-playback-state"
DEFAULT_PORT = 8090
AUTH_CALLBACK_PORT = 8899


def load_token():
    if TOKEN_FILE.exists():
        return json.loads(TOKEN_FILE.read_text())
    return None


def save_token(data):
    TOKEN_FILE.write_text(json.dumps(data, indent=2))


def post_form(url, fields):
    body = urllib.parse.urlencode(fields).encode()
    req = urllib.request.Request(url, data=body, method="POST",
                                  headers={"Content-Type": "application/x-www-form-urlencoded"})
    with urllib.request.urlopen(req, timeout=15) as resp:
        return json.loads(resp.read())


class _AuthCallbackHandler(http.server.BaseHTTPRequestHandler):
    code_holder = {}

    def log_message(self, *args):
        pass

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/callback":
            qs = urllib.parse.parse_qs(parsed.query)
            code = qs.get("code", [None])[0]
            _AuthCallbackHandler.code_holder["code"] = code
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
            self.end_headers()
            self.wfile.write(b"<html><body>Logged in - you can close this tab.</body></html>")
        else:
            self.send_response(404)
            self.end_headers()


def authorize(client_id, client_secret):
    redirect_uri = f"http://127.0.0.1:{AUTH_CALLBACK_PORT}/callback"
    params = {
        "client_id": client_id, "response_type": "code",
        "redirect_uri": redirect_uri, "scope": SCOPE,
    }
    url = AUTH_URL + "?" + urllib.parse.urlencode(params)

    httpd = http.server.HTTPServer(("127.0.0.1", AUTH_CALLBACK_PORT), _AuthCallbackHandler)
    server_thread = threading.Thread(target=httpd.handle_request, daemon=True)
    server_thread.start()

    print("Opening your browser to log in to Spotify...")
    print(f"If it doesn't open automatically, visit:\n  {url}")
    webbrowser.open(url)
    server_thread.join(timeout=120)
    httpd.server_close()

    code = _AuthCallbackHandler.code_holder.get("code")
    if not code:
        raise SystemExit("Didn't receive a login code within 2 minutes - try again.")

    token_data = post_form(TOKEN_URL, {
        "grant_type": "authorization_code", "code": code,
        "redirect_uri": redirect_uri,
        "client_id": client_id, "client_secret": client_secret,
    })
    token_data["client_id"] = client_id
    token_data["client_secret"] = client_secret
    token_data["obtained_at"] = time.time()
    save_token(token_data)
    print(f"Logged in. Saved refresh token to {TOKEN_FILE}")
    return token_data


def refresh_access_token(token_data):
    fresh = post_form(TOKEN_URL, {
        "grant_type": "refresh_token", "refresh_token": token_data["refresh_token"],
        "client_id": token_data["client_id"], "client_secret": token_data["client_secret"],
    })
    token_data["access_token"] = fresh["access_token"]
    token_data["obtained_at"] = time.time()
    if "refresh_token" in fresh:
        token_data["refresh_token"] = fresh["refresh_token"]
    save_token(token_data)
    return token_data


def get_now_playing(token_data):
    req = urllib.request.Request(
        NOW_PLAYING_URL, headers={"Authorization": f"Bearer {token_data['access_token']}"})
    try:
        with urllib.request.urlopen(req, timeout=10) as resp:
            if resp.status == 204:
                return {"playing": False}
            data = json.loads(resp.read())
    except urllib.error.HTTPError as e:
        if e.code == 401:
            return None  # signal: needs a token refresh
        raise

    if not data or not data.get("item"):
        return {"playing": False}
    item = data["item"]
    return {
        "playing": bool(data.get("is_playing")),
        "track": item.get("name", ""),
        "artist": ", ".join(a["name"] for a in item.get("artists", [])),
        "album": item.get("album", {}).get("name", ""),
        "progress_ms": data.get("progress_ms", 0),
        "duration_ms": item.get("duration_ms", 0),
    }


def make_handler(state):
    class Handler(http.server.BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def do_GET(self):
            if urllib.parse.urlparse(self.path).path != "/now-playing":
                self.send_response(404)
                self.end_headers()
                return
            with state["lock"]:
                body = json.dumps(state["last"]).encode("utf-8")
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(body)))
            self.send_header("Access-Control-Allow-Origin", "*")
            self.end_headers()
            self.wfile.write(body)

    return Handler


def poll_loop(token_data, state, interval):
    while True:
        try:
            result = get_now_playing(token_data)
            if result is None:
                token_data = refresh_access_token(token_data)
                result = get_now_playing(token_data) or {"playing": False}
            with state["lock"]:
                state["last"] = result
        except Exception as e:
            with state["lock"]:
                state["last"] = {"playing": False, "error": str(e)}
        time.sleep(interval)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--client-id")
    ap.add_argument("--client-secret")
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--poll-seconds", type=float, default=3.0)
    args = ap.parse_args()

    token_data = load_token()
    if not token_data:
        if not args.client_id or not args.client_secret:
            raise SystemExit(
                "No saved login found - run once with --client-id and --client-secret "
                "(see the top of this file for how to get them).")
        token_data = authorize(args.client_id, args.client_secret)
    else:
        token_data = refresh_access_token(token_data)

    state = {"lock": threading.Lock(), "last": {"playing": False}}
    threading.Thread(target=poll_loop, args=(token_data, state, args.poll_seconds), daemon=True).start()

    httpd = http.server.ThreadingHTTPServer(("0.0.0.0", args.port), make_handler(state))
    print(f"CydOs Spotify Now Playing: listening on 0.0.0.0:{args.port}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
