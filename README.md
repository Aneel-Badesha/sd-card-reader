
# SD Card Reader

## Description
ESP32 file manager for SD cards, enabling read/write/delete/copy/transfer to and from SD cards.

Built with ESP-IDF 5.5.1, targeting the ESP32.

## Author
Aneel Badesha

## Hardware

| Interface | Peripheral | Pins |
|-----------|------------|------|
| GPIO | Green button | GPIO 16 |
| GPIO | Red button | GPIO 17 |
| I2C (ADC) | Thumbstick (X/Y axes) | ADC CH0, ADC CH2 |
| SPI | Micro SD card reader | MOSI 23, MISO 19, SCLK 18, CS 5, VCC **5V** |
| SPI | 1.5" RGB OLED display (SSD1351, 128x128) | TBD |
| UART | Raspberry Pi | TBD |

## Project Structure

```
/components
  /button        - GPIO button driver
  /thumbstick    - ADC thumbstick driver
  /sdcard        - SPI SD card driver with FatFS
  /oled_display  - SSD1351 OLED driver (not started)
/main            - Application entry point and FreeRTOS tasks
/1.5inch-RGB-OLED-Module  - Waveshare driver reference (git submodule)
```

## Progress

- [x] GPIO button driver (`components/button`) — active-low, pull-up input
- [x] Thumbstick driver (`components/thumbstick`) — continuous ADC sampling at 20 kHz with mutex-protected reads
- [x] SD card driver (`components/sdcard`) — SPI mode at 4 MHz, FatFS mount, read/write/deinit
- [x] FreeRTOS task structure in `main.c` for buttons, thumbstick, and SD card
- [x] SD card task writes a boot-uptime timestamp to `/sdcard/timestamp.txt` on startup
- [x] Pre-commit hooks — clang-format (Linux brace style, 4-space indent, 120 col) + trailing whitespace/EOF checks
- [x] Unit tests for all three components with GitHub Actions CI (plain CMake + Unity, no ESP-IDF required)
- [x] Added Waveshare 1.5" RGB OLED submodule as driver reference
- [ ] OLED display driver (`components/oled_display`) — SSD1351, SPI
- [ ] UART bridge to Raspberry Pi

## Design Decisions

**File System: FatFS** — chosen over littlefs because the SD card needs to be readable on a PC. FatFS is the industry standard for SD cards and is optimized for 512-byte block architecture.

**SD Interface: SPI** — chosen over SDMMC for pin flexibility (any GPIO via matrix) and easy bus sharing with the OLED display via separate CS pins. Trade-off is lower throughput vs native SDMMC 4-bit mode.
