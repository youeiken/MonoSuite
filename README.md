# MonoLauncher

[![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32--S3-orange?logo=platformio)](https://platformio.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

A multi-app launcher for the **M5Stack Paper Mono Lite** (ESP32-S3, 480×800 e-paper).

MonoLauncher bundles five small apps and an e-book reader into a single firmware
image, with a shared header, save system, sleep screen, and RTC clock.

## Features

### Apps

| App | Description |
|---|---|
| **Reversi** | Othello against a heuristic AI with undo and pass detection |
| **2048** | Classic tile-merging puzzle with best-score persistence |
| **Minesweeper** | 12×14 grid with flag mode and flood-fill reveal |
| **Sokoban** | XSB level loader with deadlock detection and single-step undo |
| **Reader** | Plain-text and EPUB reader with pagination, resume, and adjustable font size |

### Shared

- **Sleep mode** — idle timeout or manual entry via A+B chord. Shows a clock, updates on the minute, wakes on A+B.
- **RTC clock** — syncs from host over USB serial at boot, falls back to NTP over WiFi if enabled.
- **Header bar** — back button, centered clock, battery indicator, info modal.
- **Save system** — per-app progress saved to SD or LittleFS, restored on next boot.
- **Confirmation dialogs** — destructive actions (quit, restart) require a tap.

## Hardware

- **Board:** [M5Stack Paper Mono Lite](https://shop.m5stack.com/) (ESP32-S3, 16 MB flash, 8 MB PSRAM)
- **Display:** 480×800 e-paper
- **Touch:** GT911 capacitive
- **Storage:** microSD card (SDMMC 4-bit mode)
- **Buttons:** BtnA (GPIO 2), BtnB (GPIO 3)

The `platformio.ini` uses `board = esp32-s3-devkitm-1` with M5Unified's Paper
Mono board fallback. See the hardware reference block at the top of
`src/main.cpp` for the SD pin map.

## SD card layout

The firmware reads everything from a mounted microSD card. Create the
following structure before first boot:

```
/books/                          # Reader: .txt and .epub files
/fonts/NotoSansTC-Subset.ttf     # Reader: required TTF for CJK and Latin
/sleep/*.bmp                     # Optional; shown on the sleep screen randomly
/sokoban/*.xsb                   # Sokoban: one or more XSB level files
/config/wifi.txt                 # Optional; see wifi.txt.example
```

If the SD card is absent or fails to mount, the firmware falls back to
LittleFS on the ESP32's internal flash — but save files and books will not
persist across reboots, and the reader will be unable to find the font.

### Font

The reader requires `/fonts/NotoSansTC-Subset.ttf`. Any TTF with the glyphs
you want to read will work; the name is only a convention. A subsetted
Noto Sans TC covers Traditional Chinese, Japanese Kana, and Latin. Place
the file on the SD card before first use.

### Level files

Sokoban reads `.xsb` files from `/sokoban/`. The XSB format is a
plain-text convention used by the Sokoban community; many public level
sets exist. Place one or more `.xsb` files in the directory and they
will be loaded in filename order.

## Build

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VSCode extension)
- USB-C cable connected to the Paper Mono Lite

### Build and upload

```bash
git clone https://github.com/youeiken/MonoLauncher.git
cd MonoLauncher
pio run -t upload
```

The default environment is `m5stack-papermono-lite`. To open the serial
monitor after flashing:

```bash
pio device monitor
```

### Time sync over serial

At boot, the firmware waits up to a few seconds for a host time sync over
USB serial. `scripts/send_time.py` implements this — it is registered as a
PlatformIO post-upload hook, so a normal `pio run -t upload` sends the
current UTC time to the device automatically.

The protocol is a single line on the serial port:

```
TIME=YYYY-MM-DDThh:mm:ssZ
```

If no serial sync arrives and WiFi credentials are present, the firmware
will attempt NTP instead. Adjust the local time offset by editing
`TIME_SYNC_TZ_OFFSET_HOURS` in `platformio.ini`.

## Controls

### Global

| Input | Action |
|---|---|
| Hold **BtnA + BtnB** | Enter sleep / wake from sleep |
| Tap the **info icon** (top right) | Show the info modal for the current app |
| Tap the **back icon** (top left) | Return to the previous screen |

### Launcher

| Input | Action |
|---|---|
| Tap an app tile | Open the app |
| **BtnB** (short) | Cycle screen brightness |

### Reader

| Input | Action |
|---|---|
| **BtnA** / **BtnB** | Previous / next page |
| Swipe left / right | Previous / next page |
| Swipe up / down | Increase / decrease font size |
| Tap left / right half | Previous / next page |
| Back icon (reading view) | Return to the book list |
| Back icon (book list) | Return to the launcher |

### Games

Each game uses the same conventions:

| Input | Action |
|---|---|
| **BtnA** (short) | Primary action (Pass / Flag mode / etc.) |
| **BtnB** (short) | Secondary action (Undo / Toggle / etc.) |
| Hold **BtnA** | Confirmation prompt → return to launcher |
| Hold **BtnB** | Confirmation prompt → restart |
| Swipe / tap on board | Game-specific input |

The in-app info modal lists the exact bindings for the running app.

## Planned features

The reader is intentionally minimal for now. The following are on the
roadmap and not yet implemented:

- Chapter navigation (jump to next/previous XHTML chapter in an EPUB)
- Table of contents (parse `toc.ncx` or `nav.xhtml`)
- Bookmarks
- Line spacing and margin adjustment
- Search within a book
- Landscape two-column layout

## Project layout

```
MonoLauncher/
├── platformio.ini          Build configuration
├── src/
│   └── main.cpp            Boot sequence, mode dispatch, sleep handling
├── lib/
│   ├── appmode/            Forward declarations for mode entry points
│   ├── appstate/           Sleep flag, activity timestamp
│   ├── display/            Display helpers (begin/end, clear, BMP load)
│   ├── ebook_decode/       Format dispatcher + EPUB/TXT backends
│   ├── fontconfig/         Built-in font subsetting switches
│   ├── game2048/           2048
│   ├── launcher/           Home screen, header, modals
│   ├── minesweeper/        Minesweeper
│   ├── miniz/              Bundled miniz (renamed symbols for EPUB)
│   ├── ofr_sd_preset/      OpenFontRender SD file hooks
│   ├── reader/             E-book reader + font management
│   ├── reversi/            Reversi
│   ├── savegame/           SD/LittleFS save helpers
│   ├── sokoban/            Sokoban
│   ├── storage/            SD mount state + filesystem helpers
│   ├── time_sync/          RTC, NTP, serial time sync
│   ├── touch/              Touch calibration and mapping
│   └── wifi_setup/         WiFi credential loading and connection
└── scripts/
    ├── send_time.py        Host-side time sync (post-upload hook)
    ├── subset_font.py      Font subsetting utility (unused at runtime)
    └── convert_hzk24h.py   Font conversion utility (unused at runtime)
```

## Configuration

`wifi.txt` (optional) enables NTP time sync when no serial sync is
available. Copy `wifi.txt.example` to `wifi.txt` and fill in your
credentials:

```
ssid=YourNetwork
password=YourPass
```

The file is read once at boot and is not committed to the repository.

The local time offset is set at compile time via `TIME_SYNC_TZ_OFFSET_HOURS`
in `platformio.ini`. The default is `8` (UTC+8).

## Dependencies

- [M5Unified](https://github.com/m5stack/M5Unified) — unified M5Stack hardware API
- [M5GFX](https://github.com/m5stack/M5GFX) — display and font drivers
- [M5PM1](https://github.com/m5stack/M5PM1) — power management IC driver
- [M5IOE1](https://github.com/m5stack/M5IOE1) — I/O expander driver
- [OpenFontRender](https://github.com/takkaO/OpenFontRender) — TTF rendering to M5GFX
- [miniz](https://github.com/richgel999/miniz) — single-file ZIP library, used for EPUB parsing

All dependencies are declared in `platformio.ini` and fetched automatically
by PlatformIO.

## License

[MIT](LICENSE) © YOUEIKEN

The bundled `lib/miniz/` directory is released under the MIT-0 / public
domain license; see the header of `miniz.c` for the full text.

## Acknowledgements

- The M5Stack team for M5Unified and M5GFX.
- [takkaO](https://github.com/takkaO) for OpenFontRender, which makes
  TTF rendering on e-paper viable.
- [Rich Geldreich](https://github.com/richgel999) for miniz, whose single-file
  design makes EPUB support possible without dragging in zlib.
- The XSB and EPUB communities for the file formats this project consumes.
