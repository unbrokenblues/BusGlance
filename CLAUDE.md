# CLAUDE.md — BusGlance

> Read this first, every session, before touching anything. BusGlance is its
> own project — a sibling folder to `Waktu` and `hardware-notes`, not part of
> the OpenClaw project.

## What this is

**BusGlance** — a working ESP32 + e-paper display showing live Singapore bus
arrival times (LTA DataMall) + weather. Built, flashed, running. GitHub:
**unbrokenblues/BusGlance**.

**Docs:** `README.md` (overview), `PROGRESS.md` (build history / wiring map /
design decisions), `logs/` (one file per past session, newest = most recent
state). **Read the newest `logs/*.md` file at the start of a session** to know
exactly where things were left.

## ⚠️ Always read this first
**`~/Projects/hardware-notes/ESP32-HARDWARE-PLAYBOOK.md`** — every hard-won
hardware/firmware lesson from building this device (USB-C CC-resistor trap,
e-paper firmware gotchas, CAD workflow, fail-loud patterns). Written FROM this
project's mistakes — don't re-learn them.

## Current state (as of the last session)
- **Working, cordless, on battery** (2000mAh LiPo, auto-charges over USB-C).
- Display: 3 bus timings/service, weather header, **fail-loud OFFLINE banner**
  if data goes stale, **NTP re-syncs every 10min** (fixes clock-drift bug that
  was corrupting "minutes away" — see git log for the full story).
- Case: rebuilt in **CadQuery** (`case/busglance_case_cq.py`) using the REAL
  WeAct board STEP model (`case/models/weact_42.step`) — exact dims, 0.00mm
  hole misalignment. Display screws to the front; ESP32 screws to a cradle on
  the back. STLs ready to print (`front_shell.stl`, `back_cover.stl`).
- Battery % (approximate, via voltage-divider): **code written, compiled,
  NOT YET FLASHED** — waiting on 2x 100kΩ resistors to be soldered (GPIO 34).
  Once wired: flash as-is, no further code changes needed.
- Panel: current display is 3-color (B/W/Red) driven as B/W — works but full
  refresh only (~10-14s, flashes). **Upgrade path identified**: true B/W
  `GDEY042T81` panel (drop-in, same wiring/case) unlocks fast (~0.4s) clean
  partial refresh — not yet ordered.

## Hardware quirks specific to THIS board
- USB-C has no CC resistors → **needs a USB-A-to-C cable** (+ dongle if the Mac
  has no USB-A port). A straight C-to-C cable will not power or enumerate it.
- Serial monitor (`arduino-cli monitor`) is unreliable through this CH340
  dongle — often shows nothing even when the board is fine. **Trust the
  display, not the serial log**, when diagnosing.
- Typical port: `/dev/cu.usbserial-210` (can change — check with
  `ls /dev/cu.usbserial-*` if a flash fails to find the port).
- Upload command: `arduino-cli upload -p /dev/cu.usbserial-210 --fqbn "esp32:esp32:esp32:UploadSpeed=115200" bus_display/` (115200, not 921600 — higher speed fails on this board).

## Git / secrets
- `bus_display/secrets.h` holds WiFi + LTA API key + ntfy topic — **git-ignored,
  never commit it**. Copy from `secrets.h.example` if missing.
- Always scan `git diff --cached` for the real WiFi password / LTA key strings
  before committing (see recent commit messages for the pattern) — this repo
  is public.

## Reminders
- Don't touch `~/Projects/Waktu/` from here — separate hardware unit, separate
  design decisions (Waktu is wall-powered/no-battery, no button, different
  BOM). BusGlance and Waktu happen to share the same display panel and ESP32
  board family, nothing else.
- This is a git repo with a GitHub remote — commit/push as normal; this folder
  is also Drive-synced, which can drop 0-byte `Icon` marker files into `.git`
  and break `git fetch`/`status`. If that happens again: `find . -type f
  -iname "Icon*" -size 0 -delete`, scoped to this repo.
