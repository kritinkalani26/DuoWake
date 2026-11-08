# DuoWake

**Two alarm clocks that act as one. The alarm only stops once you've beaten the morning's wake-up game across both boxes.**

MakeCU 2026 · Healthcare track · MLH "Best Use of Gemini API"

Firmware, build, flashing and test instructions: [firmware/README.md](firmware/README.md).

## Inspiration

Every alarm we've ever owned has the same flaw: the off button is within arm's reach. The opening of our video is real: one of us silenced his phone and was asleep again within ten seconds. The classic fix, putting the phone across the room, fails too: you walk over, hit snooze, and walk back to bed. We wanted an alarm you can't beat on autopilot, so we split it in two and gave it a sloth as a mascot, since that's who it's built for.

## What it does

DuoWake is two alarm clocks that act as one. You place them apart (16.5 m in our demo), and the alarm only stops once you've beaten that morning's wake-up game across both boxes:

- **Flash Tag**: hit the button when the light flashes.
- **Lighthouse**: catch the spinning light on the arrow.
- **Dash**: run between the clocks before time runs out.
- Or let **Random** / **Surprise me** pick.

Everything is set from a phone web app: alarm time, game, the distance between the clocks (which sets the time allowed for each run), and the tune. Pick a built-in tune like *Siren Stairs* or *Wasp Nest*, or type a mood like "spooky" and Gemini composes a fresh melody for the next morning.

## Quick start

```
cd firmware
cp secrets.example.h include/secrets.h      # hotspot SSID/password, Gemini key, both board MACs
python -m platformio run -e master -t upload
python -m platformio run -e master -t uploadfs   # the phone page, once
python -m platformio run -e slave  -t upload     # second board
python -m platformio device monitor -b 115200    # prints the phone URL, e.g. http://172.20.10.2/
```

Preview the phone app with no hardware: `python tools/mock_server.py` then open http://localhost:8080/.

## How we built it

**Electronics (×2).** Each box has an ESP32 dev board, a 12-LED WS2812B ring, a buzzer and a big momentary pushbutton, hand-soldered on a half-size perma-proto board. Pins: button GPIO 33 (to GND, internal pull-up), buzzer GPIO 25, ring data GPIO 27 (see `firmware/lib/Hardware/pins.h`).

The buzzer is switched by a 2N3904. We sized the base resistor so the transistor saturates even at worst-case gain, using a forced gain of 10 and about 30 mA of buzzer current:

$$R_B=\frac{V_{OH}-V_{BE,\mathrm{sat}}}{I_C/10}\approx\frac{3.3\ \mathrm{V}-0.75\ \mathrm{V}}{30\ \mathrm{mA}/10}\approx 850\ \Omega$$

where $V_{OH}$ is the ESP32's output-high voltage, $V_{BE,\mathrm{sat}}$ the base-emitter voltage in saturation, and $I_C$ the buzzer current (we fitted 1 kΩ). A 10 kΩ pull-down keeps the buzzer silent while the ESP32 boots, and a flyback diode protects the transistor.

**Firmware.** One non-blocking loop runs the alarm state machine, the games and the melody player, so the buttons and lights never freeze while a tune plays. Each note is converted to a buzzer frequency with equal temperament:

$$f(n)=440\ \mathrm{Hz}\cdot 2^{(n-69)/12}$$

where $n$ is the MIDI note number and 69 is A4. Tunes are written in the C5 to B6 range, roughly 0.5 to 2 kHz, close to the buzzer's mechanical resonance, where it is loudest.

**Comms.** The two boxes coordinate over ESP-NOW, Espressif's peer-to-peer protocol, while the master also stays on Wi-Fi to serve the app and call Gemini. That combination is harder than it sounds: the ESP32 has one radio, so ESP-NOW is forced onto whatever channel the hotspot picked, and ESP-NOW has no application-level retries. A packet either lands or it doesn't, and the sender only learns which from a callback. So the link is built to tolerate loss: the master keeps repeating the slave's current order (ring, stop, progress LEDs) every 500 ms while a game is live and again during the end celebration, the slave treats a repeated ring as a no-op and repeats every button press three times, and the master counts presses that arrive within 300 ms as one. A dropped packet can delay a game event, but it can't silently let you win or lose, and it can't leave a box ringing after the game is over. On boot the stored channel is applied so ESP-NOW works with no Wi-Fi at all; if the peer stays silent the link slowly scans channels until it hears a reply.

**App.** A web app served straight from the box at `duowake.local` (or its IP), so there's nothing to install. It's one 43 KB HTML file with no CDN, rendered from the firmware's `/api/options`, so adding a game or a tune needs zero HTML changes. Gemini runs on the microcontroller with no backend: the ESP32 calls the Gemini API directly, over HTTPS, from a 240 MHz chip with 320 KB of RAM. Most teams put a server between a microcontroller and an LLM. We skipped it, and used structured output with a response schema so the model returns a melody as a fixed JSON shape the firmware can parse without guessing.

Gemini is the composer, not a chatbot. The prompt gives it a constrained vocabulary of notes and durations chosen to match what the buzzer can physically play well, plus the mood you typed. When the model drifts outside that range anyway, the firmware repairs the output instead of rejecting it, transposing stray notes by octaves until they fit and clamping durations. You always get a playable tune, even from a slightly disobedient model. If the primary model is overloaded, the third attempt goes to a fallback model.

Composition happens when you save the alarm, not when it fires. Gemini writes the next morning's melody the night before, the firmware stores it in flash, and the 7 am alarm never touches the network. If the Wi-Fi is down overnight, you still wake up, just to the built-in tune, and the app tells you why.

**Enclosure.** 3D-printed cases, one white and one black, each with a laser-cut plywood plaque. The logo (`duowake-mark1.svg`) is cut through the plaque as a stencil.

**Testing.** Every hardware module has its own PlatformIO test env (`test_buzzer`, `test_ring`, `test_button`, `test_espnow`, `test_gemini`, ...), `test_pinscan` finds a miswired button by watching every GPIO, and `tools/ui_e2e.mjs` drives the real phone page in headless Chrome against a mock server and checks each flow end to end, including that saved settings actually land on the backend.

## Challenges we ran into

**Two radios' worth of work on one radio.** Getting ESP-NOW and Wi-Fi to coexist meant chasing a channel mismatch that made the boxes stop hearing each other whenever the master reconnected to the hotspot. Locking ESP-NOW to the Wi-Fi channel at runtime fixed it, and the repeat-and-dedupe scheme covers the drops that remain.

**Getting an LLM to respect a buzzer.** Early melodies came back with notes far below the buzzer's range, where it's nearly silent, or with rhythms the loop couldn't time. Tightening the schema helped; the repair step in firmware is what made it reliable. The default Gemini model was also overloaded on the day (36 s replies, 503s), so we moved to the lite model and added a fallback.

**A broken button, and no spares left.** Partway through the build, one of our pushbuttons broke, and there were no replacements available, so we had to work around it.

**A button holder that didn't fit.** The holder for the button came out the wrong size. Rather than start the enclosure over, we did some mechanical hacking on the inside of the box to make it work.

## What we learned

Buy spares and test-fit early. One broken button and one wrongly sized part cost us more time than any of the electronics did.

Design for the moment of failure. Deciding the melody at save time, and treating every ESP-NOW packet as possibly lost, meant the alarm still works when the network or the link doesn't. The parts of the system we trusted least are the parts we now trust most.

## What's next

Batteries instead of a power bank so the second box can go anywhere, wake-up stats in the app.

## Repo layout

```
firmware/               PlatformIO project (ESP32, Arduino core): envs master, slave, test_*
  lib/                  Hardware, Melody, Games, Comms, Gemini, Alarm, WebUI, App
  data/index.html       the phone app (uploaded to LittleFS)
  tools/mock_server.py  laptop preview of the app with a fake backend
  tools/ui_e2e.mjs      headless-Chrome end-to-end test of the app
arduino_button_buzzer/  minimal Arduino IDE sketch: button -> buzzer wiring check
arduino_smoke_test/     original ring + buzzer smoke test
duowake-mark1.svg       the logo
```
