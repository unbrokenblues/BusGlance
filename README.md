# BusGlance

A tiny ESP32 + 4.2" e-paper display that shows live Singapore bus arrival
times (from LTA DataMall) for two stops, plus a daily uplifting quote.
Battery-powered, sleeps overnight, and pings your phone when the battery is low.

![status](https://img.shields.io/badge/status-pre--hardware-yellow)

## Features

- **Live bus times** for a home stop and the opposite stop, refreshed every 60s.
- **Fast black & white e-paper** (~1.2s refresh) with a Bauhaus-minimalist layout.
- **"Arriving soon" tag** — buses <= 3 min show white-on-black.
- **365 built-in quotes** (scripture, motivation, manifesting), one per day.
- **Battery smart:** sleeps 00:00-05:00 with WiFi off; on-screen low-battery
  icon plus a phone push via [ntfy.sh](https://ntfy.sh).
- Runs ~10 days on a 2000 mAh LiPo.

## Hardware

- ESP32 dev board (classic ESP32)
- WeAct 4.2" e-paper, 400x300 (driven in black & white)
- LiPo battery (2000 mAh)

Wiring and full build notes are in [PROGRESS.md](PROGRESS.md).

## Setup

1. Install [arduino-cli](https://arduino.github.io/arduino-cli/) and the ESP32 core:
   ```bash
   arduino-cli core install esp32:esp32
   arduino-cli lib install GxEPD2 ArduinoJson
   ```
2. Copy the secrets template and fill in your own values:
   ```bash
   cp bus_display/secrets.h.example bus_display/secrets.h
   ```
   Set your WiFi, your [LTA DataMall](https://datamall.lta.gov.sg/) API key,
   and a private ntfy topic.
3. Adjust the stops, bus numbers, and options at the top of
   `bus_display/bus_display.ino`.
4. Build and upload:
   ```bash
   cd bus_display
   arduino-cli compile --fqbn esp32:esp32:esp32 .
   arduino-cli upload -p YOUR_PORT --fqbn esp32:esp32:esp32 .
   ```

## Note

`secrets.h` is git-ignored so your WiFi password and API key never get
committed. Never commit real credentials.
