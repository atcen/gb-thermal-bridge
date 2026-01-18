# GB Thermal Bridge

ESP32-based Game Boy Printer emulator that outputs to a standard thermal printer.

This repo merges the NeoGB-Printer firmware with the Thermal_Printer driver library so we can pipe Game Boy printer data directly to a modern thermal printer.

## Status

- Base firmware imported from NeoGB-Printer.
- Thermal_Printer library imported.
- Next: wire the decoded image pipeline to the thermal printer output.

## Repo layout

- `firmware/NeoGB_Printer` - Arduino-ESP32 firmware (NeoGB-Printer base).
- `libraries/Thermal_Printer` - Thermal_Printer Arduino library.
- `docs/neogb` - Original NeoGB-Printer docs/assets.
- `hardware/NeoGB_Printer_PCB` - Original PCB files.

## Build (Arduino-ESP32)

1) Install Arduino-ESP32 core.
2) Open `firmware/NeoGB_Printer/NeoGB_Printer.ino` in Arduino IDE.
3) Copy `libraries/Thermal_Printer` into your Arduino libraries folder (or add it via Arduino CLI `--libraries` path).
4) Configure pins and options in `firmware/NeoGB_Printer/config.h`.
5) Build/flash for your ESP32 target.

## Thermal printer output (BLE)

Enable BLE thermal printing by uncommenting the `ENABLE_THERMAL_PRINTER` block in `firmware/NeoGB_Printer/config.h` and setting the BLE name or MAC address. The firmware will convert the temporary 24-bit BMP into a 1-bit buffer and send it to the printer when each Game Boy print is converted.

## SD card usage

By default (`USE_SD_STORAGE=0`), the firmware runs without an SD card and streams Game Boy packets directly to the thermal printer. Set `USE_SD_STORAGE=1` if you want dumps, BMP/PNG output, and the web server features.

## Licensing

- This project is GPL-3.0 (see `LICENSE`).
- The Thermal_Printer library remains under Apache-2.0 (see `LICENSES/Apache-2.0.txt`).
- Original NeoGB-Printer license text is preserved in `LICENSES/NeoGB-Printer-GPLv3.txt`.
