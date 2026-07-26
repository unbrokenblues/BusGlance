# BusGlance — Progress Notes

_Last updated: 2026-07-27_

## Update 2026-07-27 — live on hardware

- **Flashing DONE.** Firmware runs on the real board: WiFi + LTA + weather all
  live, crisp black-on-white, overnight sleep re-enabled (`DEBUG_NO_NIGHT_SLEEP
  = false`).
- **Partial refresh does NOT work on this panel.** Tested on hardware: partial
  updates render as grey, ghosted mush (this is a 3-color GDEY042Z98 panel
  driven as B/W). Kept the light-sleep + partial-refresh plumbing but set
  `FULL_REFRESH_EVERY = 1` (full refresh every cycle = crisp, with the black
  flash). The flash takes ~10-14s to settle — unavoidable on this panel.
- **Fix = swap to a TRUE B/W panel: `GDEY042T81`** (WeAct 4.2" Black-White,
  SSD1683, ~$19.46, WeAct Studio Official Store on AliExpress). Confirmed by a
  buyer review running it with partial refresh. Drop-in: same wiring, same case,
  same driver — just set `FULL_REFRESH_EVERY = 15` for smooth flash-free ~0.4s
  partial refresh. NOT yet ordered.
- **Font/icon polish:** minutes auto-shrink so 3-digit bus numbers never overlap
  the "N min"; weather icons redrawn as uniform 2px outlines.
- **Battery:** LiPo confirmed safe (board `+` = red wire). Charge via USB-C
  (board charges while running). Battery low-warning (on-screen + ntfy push to
  topic `busglance-dicky`) is a no-op on this board — pin 35 can't read VBAT, so
  it fail-safes silent. Decision: just **charge weekly** (~7-10 day runtime).

An ESP32 + 4.2" e-paper display that shows live Singapore bus arrival times
(LTA DataMall) for two stops, plus a daily uplifting quote. Battery-powered,
sleeps overnight.

---

## Status at a glance

- **Software: DONE.** The sketch compiles cleanly (arduino-cli, ESP32 core).
- **Blocked on ONE hardware step:** the ESP32's header pins need to be
  **soldered on** before anything can connect. A soldering iron kit (KIPRUN,
  ~$13) was ordered. Everything else is solderless.
- **Not yet uploaded** to the board (waiting on the soldering).

**When you come back:** get the pins soldered → wire the display (map below)
→ plug in USB-C → upload → verify. Roughly 15 minutes once the iron is here.

---

## Hardware inventory

| Item | What it is | Notes |
|---|---|---|
| ESP32 "Lite" (USB-C) | The brain. Confirmed **classic ESP32** (so GPIO 23/18 = SPI). | Came with **loose header pins** — must be soldered on. |
| WeAct 4.2" e-paper, 400x300 | The screen. Physically a **3-color (b/w/red)** panel (GDEY042Z98). | Driven in **black & white** on purpose (see decisions). Factory demo showed red = confirmed 3-color. |
| Rainbow JST cable | 8-wire cable, JST plug → 8 female DuPont ends. | Plugs into the display's white socket (no solder). Other end pushes onto ESP32 pins. |
| 2x4 pin block (spare) | Alt connector for the display. | **Not used** — we use the JST cable. |
| 2 long pin strips | Male headers for the ESP32 edges. | These get soldered onto the ESP32. |

---

## Wiring map (display label → ESP32 pin)

Plug the JST cable into the display's white socket, then push each wire onto
the matching ESP32 pin. **Power is 3.3V — never 5V.**

| Display pin | ESP32 pin | Purpose |
|---|---|---|
| VCC | **3V3** | power (3.3V only) |
| GND | **GND** | ground |
| SDA | **GPIO23** | data (labeled SDA but it's SPI MOSI) |
| SCL | **GPIO18** | clock (labeled SCL but it's SPI SCK) |
| CS  | **GPIO5**  | chip select |
| D/C | **GPIO17** | data/command |
| RES | **GPIO16** | reset |
| BUSY| **GPIO4**  | busy |

Leave the display's "3-Lines / 4-Lines SPI" jumper at its factory setting.

**You do NOT need to solder all the ESP32 pins.** Only these matter:
`3V3, GND, 23, 18, 5, 17, 16, 4` (+ `35` if using the battery monitor).
On this board, the left edge (USB-C at top) conveniently has 3V3 + all six
display signals (23,18,5,17,16,4) — solder that whole strip. GND and 35 are on
the other edge — solder at least GND (and 35), plus a support pin so the strip
isn't wobbly. Skip the rest. Battery: 2000 mAh LiPo (~10 days/charge).

---

## Key settings (top of `bus_display/bus_display.ino`)

- **WiFi + LTA API key:** set in `bus_display/secrets.h` (git-ignored). Copy
  `secrets.h.example` to `secrets.h` and fill in your values.
- **Stops:** Home = `59399` (buses 807, 860, 806, 663) · Opposite = `59391` (buses 804, 807).
- **Refresh:** every `REFRESH_SECONDS = 60`.
- **Active hours:** 05:00–24:00. Sleeps 00:00–05:00 with WiFi off.
- **Battery monitor:** on-screen low-batt icon + phone push (ntfy.sh). Reads
  `BATTERY_ADC_PIN = 35` (÷2 divider), warns below `LOW_BATT_VOLTS = 3.45`.
  Set `NTFY_TOPIC` to something private; fail-safe if the pin isn't wired.
- **Charge frequency:** ~once a week on a 1500–2000 mAh LiPo (draw ~200 mAh/day,
  dominated by the 60s WiFi wakes). Cheap-board idle draw is the wild card.

---

## Design decisions (why things are the way they are)

1. **Black & white, not red.** The panel is physically 3-color, but 3-color
   mode is stuck at ~15s refresh + a 3-minute minimum interval. Driving it in
   B/W (the `GDEY042T81` driver) unlocks a **~1.2s fast refresh with no
   minimum**, so we can update every 60s. Verified against the datasheet and
   the installed library. Red was the *only* thing forcing slow updates.
2. **"Arriving soon" cue = black tag.** A bus <= 3 min away (or NOW) shows its
   minutes as **white text on a filled black box** (replaces the red idea,
   works in B/W, and is more Bauhaus anyway).
3. **Layout = Bauhaus minimalist.** Centered uppercase HOME / OPPOSITE on solid
   black bars; Home in a 2x2 grid; Opposite side-by-side; numbers hard-left,
   minutes hard-right on a clean grid.
4. **Overnight sleep saves battery.** Old code hit WiFi every wake all night.
   Now it reads the clock from the ESP32's RTC (kept alive through deep sleep)
   and, if it's 00:00–05:00, sleeps straight to 5am **without powering WiFi**.
5. **Quotes are built-in, not an API.** Researched quote APIs live: Quotable is
   dead, DummyJSON has mangled capitalization, FavQs can be dark, Bible verses
   are mostly too long. Most stable = a **built-in curated list**. Now **365
   quotes** (scripture + affirmations + proverbs + classic motivation), one per
   day by date, all validated ASCII + short. Zero network, nothing to maintain.

---

## VERIFY ON FIRST BOOT (the 3 things we couldn't test without hardware)

1. **Display driver line.** We drive a 3-color panel in B/W mode. Very likely
   fine (same SSD1683 controller), but if the screen is **blank, faint, or
   ghosted**, that's the switch to flip. Search `DISPLAY DRIVER` in the sketch.
2. **HTTPS / TLS.** Both the bus fetch and (previously) quotes use HTTPS. If
   fetches fail on the real board, the fix is a `WiFiClientSecure` +
   `client.setInsecure()` before `http.begin(...)`. Quick change.
3. **RTC time across deep sleep.** The overnight-sleep logic assumes the clock
   survives deep sleep (standard ESP32 behavior). Confirm it wakes correctly
   at ~5am and doesn't sit online all night.
4. **Battery-voltage pin.** The Serial log prints `Battery volts:` each cycle.
   Confirm it reads realistically (~3.7–4.2V on a charged LiPo). If it prints
   -1 or nonsense, the board doesn't expose battery voltage on pin 35 — either
   fix `BATTERY_ADC_PIN`/`BATTERY_DIVIDER`, add a 2-resistor divider, or set
   `BATTERY_MONITOR = false`. Also set up ntfy (below) for the phone push.

---

## Exact next steps

1. **Solder the two header strips onto the ESP32** (pins pointing down, USB-C
   side up). Or have a makerspace/shop do it.
2. **Wire the display** using the map above (all unplugged from USB).
3. **Plug in USB-C.** Find the serial port:
   ```bash
   arduino-cli board list
   ```
4. **Upload** (replace PORT with what step 3 shows, e.g. /dev/cu.usbserial-XXXX):
   ```bash
   cd "/Users/dickyagustiady/Projects/BusSchedule/bus_display"
   arduino-cli upload -p PORT --fqbn esp32:esp32:esp32 .
   ```
   (If upload fails to start, hold the BOOT button on the ESP32 while it connects.)
5. **Watch the serial log** to confirm WiFi + fetch:
   ```bash
   arduino-cli monitor -p PORT -c baudrate=115200
   ```

To recompile after any edit:
```bash
cd "/Users/dickyagustiady/Projects/BusSchedule/bus_display"
arduino-cli compile --fqbn esp32:esp32:esp32 .
```

---

## First-time soldering cheat sheet (for the header pins)

The easiest kind of soldering — big holes, forgiving. Steps:

1. Let the iron reach ~350C (the kit's digital display shows temp).
2. Sit the pin strip in the ESP32's holes, **plastic spacer flush**, USB side up.
3. **Solder ONE end pin first**, then check the strip sits straight/flush. If
   it tilted, re-melt that joint and nudge it square. *Then* do the rest.
4. Per joint: touch the iron to the pin **and** the gold ring for ~1 second,
   feed a little solder into the joint (not onto the tip), remove solder, then
   remove iron. You want a small shiny cone, not a blob.
5. GND pins take a touch longer to heat (they sink heat) — be patient.
6. Do all pins on both strips so the header is mechanically solid.
7. Rest the hot iron in its stand; wet the sponge to wipe the tip.

Then the display side is 100% solderless — the JST cable just clicks in.

---

## Environment (already set up on this Mac)

- `arduino-cli` v1.5.1 (via Homebrew)
- ESP32 core `esp32:esp32` v3.3.11
- Libraries: GxEPD2, ArduinoJson (+ Adafruit GFX/BusIO)
- Board FQBN: `esp32:esp32:esp32`
- Sketch: `BusSchedule/bus_display/bus_display.ino` (compiles at ~86% flash)
