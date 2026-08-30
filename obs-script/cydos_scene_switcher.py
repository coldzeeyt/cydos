"""
CydOs Scene Switcher - an OBS script (Tools > Scripts, no compiling, no
separate companion app) that runs a tiny local HTTP server so a CydOs
multitool - or anything else on your network that can do a GET request -
can list your scenes and switch between them.

Endpoints:
  GET /scenes          -> JSON array of scene names, e.g. ["Starting Soon","Gameplay","BRB"]
  GET /switch?scene=X  -> switches to the scene named X (or, if X is a
                          plain number, the scene at that 1-based position
                          in the list /scenes just returned)

No authentication - this is meant for your own LAN. Don't port-forward it.
"""

import obspython as obs
import http.server
import threading
import json
import urllib.parse

DEFAULT_PORT = 8088

httpd = None
server_thread = None


def get_scenes():
    """Returns the list of scene source objects (caller must release)."""
    return obs.obs_frontend_get_scenes()


def get_scene_names():
    names = []
    scenes = get_scenes()
    if scenes is not None:
        for s in scenes:
            names.append(obs.obs_source_get_name(s))
        obs.source_list_release(scenes)
    return names


def switch_to_scene(identifier):
    scenes = get_scenes()
    if scenes is None:
        return False
    target = None
    if identifier.isdigit():
        idx = int(identifier) - 1
        if 0 <= idx < len(scenes):
            target = scenes[idx]
    if target is None:
        for s in scenes:
            if obs.obs_source_get_name(s) == identifier:
                target = s
                break
    ok = target is not None
    if ok:
        obs.obs_frontend_set_current_scene(target)
    obs.source_list_release(scenes)
    return ok


class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass  # keep OBS's script log quiet - errors still print via print()

    def _send(self, code, body=b"", content_type="text/plain"):
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        if body:
            self.wfile.write(body)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/scenes":
            body = json.dumps(get_scene_names()).encode("utf-8")
            self._send(200, body, "application/json")
        elif parsed.path == "/switch":
            qs = urllib.parse.parse_qs(parsed.query)
            name = qs.get("scene", [""])[0]
            ok = bool(name) and switch_to_scene(name)
            self._send(200 if ok else 404, b"ok" if ok else b"scene not found")
        else:
            self._send(404, b"not found")


def stop_server():
    global httpd, server_thread
    if httpd is not None:
        httpd.shutdown()
        httpd.server_close()
    httpd = None
    server_thread = None


def start_server(port):
    global httpd, server_thread
    stop_server()
    try:
        httpd = http.server.ThreadingHTTPServer(("0.0.0.0", port), Handler)
    except OSError as e:
        print("CydOs Scene Switcher: couldn't bind port {}: {}".format(port, e))
        return
    server_thread = threading.Thread(target=httpd.serve_forever, daemon=True)
    server_thread.start()
    print("CydOs Scene Switcher: listening on 0.0.0.0:{}".format(port))


def script_description():
    return (
        "<b>CydOs Scene Switcher</b><br><br>"
        "Runs a small local HTTP server so a CydOs multitool (or anything "
        "else on your network) can see your scene list and switch between "
        "them.<br><br>"
        "GET /scenes &nbsp;&nbsp;&nbsp;&nbsp;&#8594; JSON array of scene names<br>"
        "GET /switch?scene=X &#8594; switch to scene X<br><br>"
        "No authentication - only run this on a network you trust."
    )


def script_properties():
    props = obs.obs_properties_create()
    obs.obs_properties_add_int(props, "port", "Port", 1024, 65535, 1)
    return props


def script_defaults(settings):
    obs.obs_data_set_default_int(settings, "port", DEFAULT_PORT)


def script_update(settings):
    start_server(obs.obs_data_get_int(settings, "port"))


def script_unload():
    stop_server()
