# DuoWake — firmware

MakeCU 2026 · Healthcare track · MLH "Best Use of Gemini API"

Two identical ESP32 alarm clocks. At alarm time the MASTER starts a physical "game" (tap a flashing LED,
catch a spinning LED, or sprint between the two clocks) and the alarm does **not** stop until the game is
fully completed. Gemini picks the game and composes brand-new alarm melodies the night before.

## Pin map (`lib/Hardware/pins.h` — the only place pins live)

| Function                | GPIO | Notes                                              |
|-------------------------|------|----------------------------------------------------|
| WS2812B ring data       | 27   | 12 LEDs, 330 Ω series resistor, 5 V from VIN, brightness capped 60 |
| Buzzer (passive, single)| 25   | LEDC PWM; GPIO -> 1k -> 2N3904 base, low-side; HIGH = on |
| Push button             | 33   | to GND, `INPUT_PULLUP`, pressed = LOW, 30 ms debounce |

Both clocks have identical wiring. Role is a build flag.

## Repo layout

```
firmware/
  platformio.ini         envs: master, slave, test_*
  src/main.cpp           thin: picks MasterApp / SlaveApp from the ROLE_* flag
  lib/Hardware/          pins.h, alarm_config.h (all tunables), buzzer, button, ring
  lib/Melody/            melody format + note table + looping player, builtin_tunes.h, melody_store (LittleFS)
  lib/Games/             game.h interface, game_reaction, game_rotate, game_sprint, game_registry
  lib/Comms/             espnow_link (typed messages), wifi_manager (10 s watchdog), secrets_loader.h
  lib/Gemini/            gemini_client (HTTPS, JSON mode, validation, retry, model listing on 404)
  lib/Alarm/             timekeeper (NTP/manual), alarm_scheduler (NVS settings + fire), alarm_prep (Gemini work at save time), alarm_runner (runs a game)
  lib/WebUI/             web_server (ESPAsyncWebServer API), data/index.html (phone UI)
  lib/App/               master_app, slave_app (the two role loops)
  test/<env>/main.cpp    one standalone main per test env
  data/index.html        uploaded to LittleFS
  secrets.example.h      copy to include/secrets.h
```

## First-time setup

1. Install PlatformIO (`pip install platformio` or the VS Code extension).
2. `cp secrets.example.h include/secrets.h` and fill in:
   - `WIFI_SSID` / `WIFI_PASS` — the phone hotspot (Android, 2.4 GHz).
   - `GEMINI_API_KEY` — from Google AI Studio.
   - `MASTER_MAC` / `SLAVE_MAC` — flash `test_espnow` to each board; it prints `MY MAC: ...` on boot.
   (If `include/secrets.h` is missing the build still compiles with placeholder values and a `#warning`.)

## Build / flash

```
pio run -e master                      # compile
pio run -e master -t upload            # flash clock A
pio run -e master -t uploadfs          # upload data/ (index.html) to LittleFS — needed once for the web UI
pio run -e slave  -t upload            # flash clock B
pio device monitor -b 115200           # serial log
```

If two boards are plugged in at once add `--upload-port COMx`. A bare `pio run` builds only `master`
(`default_envs`); name the env with `-e` for anything else. The partition table is `min_spiffs.csv`
(~1.9 MB app, ~190 KB LittleFS); the default table was 88% full with the MASTER image.

## Finding the IP

MASTER prints this on every (re)connect:

```
[master] >>> phone UI: http://192.168.43.123/   (or http://duowake.local/) <<<
```

Open that URL on the phone that runs the hotspot. `duowake.local` (mDNS) usually does not resolve on
Android hotspots, so use the IP. Android hotspot settings also list connected devices with their IPs.

## Phone UI

`data/index.html` is one file (inline CSS/JS, no CDN), mobile-first, rendered entirely from `/api/options`,
so adding a game or a tune needs zero HTML changes. Top to bottom: status strip (countdown, WiFi bars,
Clock B link dot, device time), big time picker, game cards (one per registry game plus Random and
Surprise me, with little CSS icons), tune list (New Gemini melody and Shuffle pinned, then saved Gemini
melodies newest first, then built-ins, each with a 12-bar pitch "waveform"), distance slider (only for
Dash/Random/Surprise, shows allowed seconds per leg), a **style box** (only for New Gemini melody / Surprise me: free text like
"spooky" or "8-bit boss fight", max 80 chars, sent as `tuneStyle` and appended to the Gemini melody prompt), **Set alarm**, and a "Tomorrow's plan" panel that
shows "Gemini is composing…" and polls until the plan is filled in. The collapsed **More** section has Test
alarm now, Sync time from phone, History, and the demo-only Stop.

### IDs (the UI only ever sends ids)
- Game ids: `reaction` (Flash Tag), `rotate` (Lighthouse), `sprint` (Dash), plus pseudo `random` and
  `surprise`. Display name and tagline come from `displayName()` / `tagline()` on each `Game`.
- Tune ids: `builtin:<slug>`, `gen:YYYY-MM-DD_<slug>` (LittleFS filename without `.json`), plus pseudo
  `new` (Gemini composes tonight) and `shuffle` (random saved melody picked at alarm time).

### API (MASTER, all JSON)
| Method | Path                | Body / notes                                                        |
|--------|---------------------|---------------------------------------------------------------------|
| GET    | `/api/state`        | settings, nextAlarm (+epoch), plan {gameName,tuneName,geminiReason,pending}, wifi, link, uptime, isRinging |
| GET    | `/api/options`      | `{games:[{id,name,tagline}], tunes:[{id,name,source,date,bars}]}`   |
| POST   | `/api/settings`     | `{alarmTime:"HH:MM", gameId, tuneId, distanceM, tuneStyle?}` → validated, saved to NVS, Gemini prep queued, returns new state |
| POST   | `/api/test`         | `{gameId?, tuneId?}` start now (pseudo ids resolved locally, `new` → shuffle) |
| POST   | `/api/stop`         | emergency stop, logged loudly                                       |
| POST   | `/api/time`         | `{epoch}` manual sync                                               |
| GET    | `/api/history`      | last 30 entries `{date, game, tuneId, tune, reason, completed}`     |
| GET    | `/api/melody/:id`   | note JSON for a tune id (also `?id=`)                               |

Each endpoint is its own function in `lib/WebUI/web_server.cpp`; `/api/options` is built by
`lib/WebUI/web_options.cpp`. Develop the page without the alarm using `test_webui` (fake state, real options).

### Previewing the page on a laptop (no ESP32)
```
python tools/mock_server.py            # http://localhost:8080/  (F12 then Ctrl+Shift+M for the phone view)
python tools/mock_server.py 0.0.0.0    # also reachable from a phone on the same WiFi at http://<laptop-ip>:8080/
```
The mock fakes every endpoint: Save shows "Gemini is composing" for 6 s, Test shows ringing for 8 s, the
Clock B dot toggles every 15 s. `MOCK_PORT` / `MOCK_ROOT` env vars override the port and the html folder.

Page features worth showing in a demo: the night sky brightens toward sunrise as the alarm gets closer, the
12-LED ring in the hero mirrors the real ring and previews the selected game, each tune has a play button
that renders the notes with WebAudio, Gemini's reason is typed out in a speech bubble, and confetti fires
when the plan lands. Everything is one 38 KB file, no CDN.

### End-to-end UI test (laptop, no ESP32)
`tools/ui_e2e.mjs` drives the real page in headless Chrome over the DevTools protocol and asserts the flows a
judge will try: unsynced clock is flagged and "Sync time from phone" fixes it, picking a time previews the new
countdown before saving, "Set alarm" updates the backend and the UI, and the value survives polling.
```
python tools/mock_server.py
"C:/Program Files/Google/Chrome/Application/chrome.exe" --headless=new --remote-debugging-port=9222 --user-data-dir=%TEMP%/chromeprof about:blank
node tools/ui_e2e.mjs
```
The mock boots with a wrong clock on purpose (`MOCK_SYNCED=1` to skip) so the sync flow can be demoed.

## Running each test env

Each test is a standalone `main.cpp` that exercises one module on real hardware and logs to Serial.

| env                  | what it does                                                                        |
|----------------------|-------------------------------------------------------------------------------------|
| `test_buzzer`        | C5-B6 scale sweep, fail burst, then each built-in tune (single passive buzzer)      |
| `test_ring`          | R/G/B fills, progress fill 0..12, spinner, breathing                                |
| `test_button`        | prints each debounced press, ring lights while held                                 |
| `test_game_reaction` | plays the reaction game standalone (press the button, GPIO 33, when an LED flashes)              |
| `test_game_rotate`   | plays the rotate game standalone (press when the chaser is on/next to blue LED 0)   |
| `test_espnow`        | same binary on both boards: prints MAC, pings peer, button rings the other board    |
| `test_gemini`        | joins hotspot, asks Gemini for a game pick + a melody, validates, saves, plays it   |
| `test_webui`         | serves the UI with fake data so the HTML can be iterated (`-t uploadfs` first)      |

```
pio run -e test_buzzer -t upload && pio device monitor -b 115200
```

## How the alarm flows

1. Night before: user saves settings → `AlarmPrep` builds a **plan** (resolved game, resolved tune, Gemini's
   reason) and stores it in NVS. Gemini is only called here. Failures fall back to a local random pick /
   random built-in tune and say so in the reason.
2. `AlarmScheduler` checks local time (NTP, `America/New_York`) once a second and fires at HH:MM.
3. `AlarmRunner` loads the melody, sends it to SLAVE over ESP-NOW, creates the game from `GameRegistry`,
   and pumps `update()` / `onButton()` until `isComplete()`. Failing a round restarts it with a fail burst.
4. SLAVE is dark/silent unless it receives `RING`; it reports `BUTTON_PRESSED` and silences itself.

### Sprint timing
MASTER rings → press A → MASTER silent, SLAVE rings, leg timer starts → press B → SLAVE silent, reports
`BUTTON_PRESSED`, MASTER checks leg time and rings again → … 4 legs (A→B→A→B→A). A leg over the limit
(also detected live, without waiting for a late press) beeps both clocks and restarts from leg 1.

## ESP-NOW + WiFi coexistence
Both boards join the hotspot in station mode. On connect, `EspNowLink::lockChannel()` re-adds the peer on the
hotspot channel and stores it in NVS; on boot (before WiFi is up) the stored channel is applied so ESP-NOW
works with no WiFi at all. If WiFi is down and the peer stays silent for 15 s the link slowly scans
channels 1-13 until it hears a PONG. Link state and RTT are shown in the UI.

Known trade-off: while the hotspot is unreachable the 10 s reconnect watchdog makes the radio scan
channels for a couple of seconds each attempt, which can drop an ESP-NOW packet during a sprint. The link
re-applies its channel before every ping, so the gap is short. If you demo with no hotspot at all, set a
bogus SSID so attempts fail fast, or lengthen `WIFI_RETRY_MS` in `alarm_config.h`.

## Adding a game
1. Create `lib/Games/game_<name>.h/.cpp` implementing `Game` (`begin/update/onButton/isComplete/reset/name`,
   optionally `configure(GameParams)`). No `delay()`; everything from `update(now)`.
2. Add one line to the table in `lib/Games/game_registry.cpp`.
3. Add `<option>` to the game dropdown in `data/index.html`. Gemini sees the new name automatically via
   `GameRegistry::namesCsv()`; you may want to describe it in the prompt in `gemini_client.cpp`.

## Melody format
```json
{"name":"Wasp Nest","notes":[{"n":"F#5","ms":80},{"n":"REST","ms":50}]}
```
Notes `C5`..`B6` only (two octaves, sharps; flats are normalised to sharps; anything below C5 is rejected because a passive buzzer is too quiet there), `REST`; 8-16 notes; 50-2000 ms each.
Generated melodies are saved as `/melodies/YYYY-MM-DD_<slug>.json` (tune id `gen:YYYY-MM-DD_<slug>`); `/history.json` records which melody
and game ran on which date.

## Gemini
`POST https://generativelanguage.googleapis.com/v1beta/models/gemini-flash-lite-latest:generateContent` with
`responseMimeType: application/json` and a `responseSchema`. If the model name 404s, the client GETs
`/v1beta/models` and logs the names that support `generateContent`; change `GEMINI_MODEL` in
`lib/Hardware/alarm_config.h`.

## Tunables
All non-pin constants live in `lib/Hardware/alarm_config.h` (`RUN_PACE_MPS`, `LEG_SLACK_S`, `SPRINT_LEGS`,
reaction/rotate timings, WiFi retry, NTP server, TZ, model name).
