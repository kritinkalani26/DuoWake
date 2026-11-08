#!/usr/bin/env python3
"""Serve data/index.html with a fake /api so the phone UI can be previewed on a laptop without an ESP32.

    python tools/mock_server.py            # then open http://localhost:8080/
    python tools/mock_server.py 0.0.0.0    # also reachable from a phone on the same WiFi: http://<laptop-ip>:8080/

In Chrome press F12 then Ctrl+Shift+M to switch to the phone layout.
"""
import json, os, sys, time, random
from http.server import BaseHTTPRequestHandler, HTTPServer

ROOT = os.environ.get("MOCK_ROOT") or os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")
PORT = int(os.environ.get("MOCK_PORT", "8080"))

GAMES = [
    {"id": "reaction", "name": "Flash Tag", "tagline": "Hit the button when the light flashes"},
    {"id": "rotate", "name": "Lighthouse", "tagline": "Catch the spinning light on the arrow"},
    {"id": "sprint", "name": "Dash", "tagline": "Run between the clocks before the timer runs out"},
    {"id": "random", "name": "Random", "tagline": "Gemini picks tomorrow's game for you", "pseudo": True},
    {"id": "surprise", "name": "Surprise me", "tagline": "Gemini picks the game AND composes a new tune", "pseudo": True},
]
def bars(seed):
    r = random.Random(seed); return [round(r.random(), 2) for _ in range(12)]
TUNES = [
    {"id": "new", "name": "New Gemini melody", "source": "pseudo"},
    {"id": "shuffle", "name": "Shuffle saved", "source": "pseudo"},
    {"id": "gen:2026-11-07_sunrise-dash", "name": "Sunrise Dash", "source": "gemini", "date": "2026-11-07", "bars": bars(1)},
    {"id": "gen:2026-11-06_pancake-panic", "name": "Pancake Panic", "source": "gemini", "date": "2026-11-06", "bars": bars(2)},
    {"id": "builtin:siren_stairs", "name": "Siren Stairs", "source": "builtin", "bars": bars(3)},
    {"id": "builtin:wasp_nest", "name": "Wasp Nest", "source": "builtin", "bars": bars(4)},
    {"id": "builtin:hiccup_march", "name": "Hiccup March", "source": "builtin", "bars": bars(5)},
]
HISTORY = [
    {"date": "2026-11-05", "game": "rotate", "tuneId": "builtin:siren_stairs", "tune": "Siren Stairs", "reason": "", "completed": True},
    {"date": "2026-11-06", "game": "sprint", "tuneId": "gen:2026-11-06_pancake-panic", "tune": "Pancake Panic", "reason": "Gemini chose Dash because mornings are for moving.", "completed": True},
    {"date": "2026-11-07", "game": "reaction", "tuneId": "builtin:wasp_nest", "tune": "Wasp Nest", "reason": "", "completed": False},
]
settings = {"alarmTime": "07:30", "enabled": True, "gameId": "random", "tuneId": "new", "distanceM": 6, "tuneStyle": ""}
plan_ready_at = 0.0
ringing_until = 0.0
# The fake board boots with no idea of the time (like a real one with no NTP): wrong by -5h17m and
# timeSource "millis" until the phone POSTs /api/time. Start with MOCK_SYNCED=1 to skip that.
clock_offset = 0.0 if os.environ.get("MOCK_SYNCED") else -(5 * 3600 + 17 * 60)
clock_source = "ntp" if os.environ.get("MOCK_SYNCED") else "millis"

def state():
    now = time.time() + clock_offset
    pending = time.time() < plan_ready_at
    lt = time.localtime(now)
    h, m = map(int, settings["alarmTime"].split(":"))
    alarm = time.mktime((lt.tm_year, lt.tm_mon, lt.tm_mday, h, m, 0, 0, 0, -1))
    if alarm <= now: alarm += 86400
    return {
        "now": int(now), "time": time.strftime("%H:%M:%S", lt), "date": time.strftime("%Y-%m-%d", lt),
        "timeSource": clock_source, "uptime": 1234, "isRinging": time.time() < ringing_until, "ringingGame": "sprint" if time.time() < ringing_until else "",
        "settings": settings,
        "nextAlarm": time.strftime("%Y-%m-%dT%H:%M", time.localtime(alarm)), "nextAlarmEpoch": int(alarm),
        "plan": {"ready": not pending, "pending": pending, "status": "asking Gemini..." if pending else "ready",
                 "gameId": "" if pending else "sprint", "gameName": "" if pending else "Dash",
                 "tuneId": "" if pending else "gen:2026-11-07_sunrise-dash", "tuneName": "" if pending else "Sunrise Dash",
                 "geminiReason": "" if pending else "Gemini chose Dash because your legs looked suspiciously well-rested."},
        "gemini": "fake", "wifi": {"connected": True, "ip": "192.168.43.42", "rssi": -58, "channel": 6},
        "link": {"ok": int(now / 15) % 2 == 0, "rtt": 3}, "version": 1,
    }

class H(BaseHTTPRequestHandler):
    def _json(self, obj, code=200):
        b = json.dumps(obj).encode(); self.send_response(code)
        self.send_header("Content-Type", "application/json"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        p = self.path.split("?")[0]
        if p == "/":
            b = open(os.path.join(ROOT, "index.html"), "rb").read(); self.send_response(200)
            self.send_header("Content-Type", "text/html"); self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
        elif p == "/api/state": self._json(state())
        elif p == "/api/options": self._json({"games": GAMES, "tunes": TUNES, "version": 1})
        elif p == "/api/history": self._json(HISTORY)
        elif p.startswith("/api/melody/"): self._json({"id": p[12:], "name": "Fake", "notes": [{"n": "C5", "ms": 120}] * 8})
        else: self.send_error(404)
    def do_POST(self):
        global plan_ready_at, ringing_until, clock_offset, clock_source
        n = int(self.headers.get("Content-Length", 0)); body = json.loads(self.rfile.read(n) or b"{}")
        if self.path == "/api/settings":
            settings.update({k: body[k] for k in ("alarmTime", "gameId", "tuneId", "distanceM", "tuneStyle") if k in body})
            if settings.get("tuneStyle"): print(f"   Gemini would compose in style: {settings['tuneStyle']!r}")
            plan_ready_at = time.time() + 6; s = state(); s["ok"] = True; self._json(s)
        elif self.path == "/api/test": ringing_until = time.time() + 8; self._json({"ok": True, "msg": "alarm starting"})
        elif self.path == "/api/stop": ringing_until = 0; self._json({"ok": True, "msg": "stopping"})
        elif self.path == "/api/time":
            clock_offset = float(body.get("epoch", time.time())) - time.time(); clock_source = "manual"
            print(f"   clock synced from phone, offset now {clock_offset:+.0f}s"); self._json({"ok": True, "msg": "time set"})
        else: self.send_error(404)
    def log_message(self, fmt, *a): print(self.command, self.path)

if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    print(f"mock UI at http://{host}:{PORT}/  (Ctrl+C to stop)")
    HTTPServer((host, PORT), H).serve_forever()
