# SD Card Reader

## Description

ESP32-based handheld file browser for SD cards. Displays the SD card file system on a 1.5" RGB OLED and allows navigation using a thumbstick and two buttons.

Built with ESP-IDF 5.5.1, targeting the ESP32-WROOM.

## Author

Aneel Badesha

---

## Hardware

| Interface | Peripheral | Pins |
|-----------|------------|------|
| GPIO | Green button (select / enter) | GPIO 16 |
| GPIO | Red button (back / cancel) | GPIO 17 |
| ADC | Thumbstick X axis | GPIO 39 (ADC1 CH3) |
| ADC | Thumbstick Y axis | GPIO 36 (ADC1 CH0) |
| SPI (VSPI) | Micro SD card reader | MOSI 23, MISO 19, SCLK 18, CS 5, VCC **5V** |
| SPI (VSPI, shared) | W5500 Ethernet | CS 26, INT 25, RST 33 (shares MOSI/MISO/SCLK with SD) |
| SPI (HSPI) | 1.5" RGB OLED (SSD1351, 128×128) | MOSI 13, SCLK 14, CS 15, DC 2, RST 4 |

> **Note:** The SD card module requires **5V** VCC — it has an onboard 5V→3.3V regulator. Connecting to the ESP32 3.3V pin will cause init failures.

---

## Project Structure

```
/components
  /button        - GPIO button driver (active-low, pull-up)
  /thumbstick    - Continuous ADC sampling, ISR-notified background task
  /sdcard        - SPI SD card driver, FatFS mounted at /sdcard
  /oled_display  - Custom SSD1351 driver (ESP-IDF SPI, 5×8 bitmap font)
  /ethernet      - W5500 Ethernet driver, TCP file transfer
/main            - FreeRTOS tasks, file browser UI state machine
/tools           - Raspberry Pi helper scripts
/docs            - Design document and SSD1351 datasheet
```

---

## Architecture

Four independent FreeRTOS tasks communicate through a shared mutex:

| Task | Role |
|------|------|
| `s_button_task` | Polls GPIO 16/17 every 20 ms, writes to shared input state |
| `s_thumbstick_task` | Reads ADC via ISR-notified loop, writes to shared input state |
| `s_sd_card_task` | Inits SD card, writes boot timestamp, signals SD ready |
| `s_oled_task` | Waits for SD ready, renders file browser, handles navigation |

The SD card stays mounted for the full session. The OLED task uses `opendir`/`readdir` directly on the FatFS mount point.

---

## File Browser UI

```
┌──────────────────────┐
│ /sdcard/photos/      │  ← current path (yellow)
│ /vacation/           │
│ /misc/               │
│▶img001.jpg           │  ← selected (white highlight)
│ img002.jpg           │
│ img003.jpg           │
│ ...                  │
└──────────────────────┘
```

| Input | Action |
|-------|--------|
| Thumbstick up | Move selection up |
| Thumbstick down | Move selection down |
| Green button | Enter directory |
| Red button | Go up one directory |

---

## Progress

- [x] GPIO button driver — active-low, pull-up input
- [x] Thumbstick driver — continuous ADC at 20 kHz, mutex-protected reads
- [x] SD card driver — SPI at 4 MHz, FatFS, read/write/deinit
- [x] SD card task writes boot-uptime timestamp to `/sdcard/timestamp.txt`
- [x] OLED driver — custom SSD1351, ESP-IDF SPI, 5×8 bitmap font
- [x] File browser UI — directory listing, scroll, enter/back navigation
- [x] Pre-commit hooks — clang-format + trailing whitespace/EOF checks
- [ ] W5500 Ethernet driver — SPI3 shared bus, esp_eth, TCP/IP via lwIP
- [ ] TCP file transfer to Raspberry Pi
- [ ] Raspberry Pi Python receive script

---

## Design Decisions

**FatFS over littlefs** — SD card needs to be readable on a PC. FatFS is the standard for SD cards and optimized for 512-byte block architecture. Downside: no power-loss protection.

**SPI over SDMMC** — SPI can be routed to any GPIO via the matrix and is simpler to share with the OLED on a breadboard. Downside: 1-bit mode only, lower throughput than native 4-bit SDMMC.

**Separate SPI buses** — SD card on VSPI (SPI3), OLED on HSPI (SPI2). Each component owns its bus independently with no shared state or mutex needed between them. W5500 shares SPI3 with the SD card via a separate CS pin.

**Custom OLED driver** — Written directly against the SSD1351 datasheet (see `docs/SSD1351.pdf`) rather than porting the Waveshare Linux driver. Result is a clean, ESP-IDF-native implementation with no dead code.

**Permanent SD mount** — Card stays mounted for the session rather than mounting per operation. Eliminates ~300 ms remount latency on every directory change, making the browser feel responsive.

**W5500 over LAN8720** — LAN8720 requires the ESP32's internal EMAC via RMII (9 dedicated pins, GPIO 0 boot conflict, not available on S3). W5500 is a self-contained SPI Ethernet chip that works on any ESP32 variant, shares the existing SPI3 bus, and has native ESP-IDF support via `esp_eth`. Trade-off: 20–30 Mbps vs 100 Mbps for LAN8720, but sufficient for file transfer.
