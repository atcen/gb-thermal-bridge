GB Thermal Bridge Demo

This is a self-contained Arduino sketch that prints a 24-bit BMP from SD to a BLE thermal printer.

Steps:
1) Put a 24-bit BMP at `/demo.bmp` on your SD card.
2) Adjust the SD and BLE settings in `GB_Thermal_Bridge_Demo.ino`.
3) Open `GB_Thermal_Bridge_Demo.ino` in Arduino IDE and upload to your ESP32.

Notes:
- This demo is only for validating the printer path.
- The full Game Boy printer emulator lives in `firmware/NeoGB_Printer/NeoGB_Printer.ino`.
