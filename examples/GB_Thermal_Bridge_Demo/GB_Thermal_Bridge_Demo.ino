#include <Arduino.h>
#include <SPI.h>
#include <FS.h>
#include <SD.h>
#include <Thermal_Printer.h>

// Demo config
// SD card wiring (SPI):
// SD_CS   -> CS/SS
// SD_SCK  -> SCK
// SD_MOSI -> MOSI
// SD_MISO -> MISO
#define SD_CS   15
#define SD_SCK  14
#define SD_MOSI 13
#define SD_MISO 26

#define THERMAL_PRINTER_NAME ""
#define THERMAL_PRINTER_MAC  ""
#define THERMAL_SCAN_SECONDS 5
#define THERMAL_ENERGY       12000  // 1-65535, 0 to skip
#define THERMAL_THRESHOLD    160    // 0-255, lower = darker
#define THERMAL_INVERT       0
#define THERMAL_CENTER_OUTPUT
#define THERMAL_FEED_LINES   4

static bool demo_printer_connect()
{
  if (tpIsConnected()) {
    return true;
  }

  if (THERMAL_PRINTER_MAC[0] != '\0') {
    if (!tpConnect(THERMAL_PRINTER_MAC)) {
      return false;
    }
  } else {
    if (THERMAL_PRINTER_NAME[0] != '\0') {
      tpScan(THERMAL_PRINTER_NAME, THERMAL_SCAN_SECONDS);
    } else {
      tpScan("", THERMAL_SCAN_SECONDS);
    }
    if (!tpConnect()) {
      return false;
    }
  }

  if (THERMAL_ENERGY > 0) {
    tpSetEnergy(THERMAL_ENERGY);
  }

  return true;
}

static uint8_t demo_luma(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
}

static bool demo_read_bmp_header(File &file, int32_t *width, int32_t *height, uint32_t *offset, uint16_t *bpp)
{
  uint8_t header[54];

  if (file.read(header, sizeof(header)) != (int)sizeof(header)) {
    return false;
  }
  if (header[0] != 'B' || header[1] != 'M') {
    return false;
  }

  *offset = (uint32_t)header[10] | ((uint32_t)header[11] << 8) | ((uint32_t)header[12] << 16) | ((uint32_t)header[13] << 24);
  *width = (int32_t)header[18] | ((int32_t)header[19] << 8) | ((int32_t)header[20] << 16) | ((int32_t)header[21] << 24);
  *height = (int32_t)header[22] | ((int32_t)header[23] << 8) | ((int32_t)header[24] << 16) | ((int32_t)header[25] << 24);
  *bpp = (uint16_t)header[28] | ((uint16_t)header[29] << 8);

  return true;
}

static bool demo_print_bmp(const char *bmpPath)
{
  if (!demo_printer_connect()) {
    Serial.println("Demo: printer not connected");
    return false;
  }

  File file = SD.open(bmpPath, "r");
  if (!file) {
    Serial.println("Demo: failed to open BMP");
    return false;
  }

  int32_t width = 0;
  int32_t height = 0;
  uint32_t offset = 0;
  uint16_t bpp = 0;

  if (!demo_read_bmp_header(file, &width, &height, &offset, &bpp)) {
    Serial.println("Demo: invalid BMP header");
    file.close();
    return false;
  }

  if (bpp != 24 || width <= 0 || height == 0) {
    Serial.println("Demo: unsupported BMP format");
    file.close();
    return false;
  }

  bool topDown = false;
  if (height < 0) {
    height = -height;
    topDown = true;
  }

  int printerWidth = tpGetWidth();
  if (printerWidth <= 0) {
    printerWidth = width;
  }

  int xOffset = 0;
#ifdef THERMAL_CENTER_OUTPUT
  if (printerWidth > width) {
    xOffset = (printerWidth - width) / 2;
  }
#endif

  int lineStride = (width * 3 + 3) & ~3;
  size_t bufferSize = (size_t)((printerWidth + 7) / 8) * (size_t)height;
  uint8_t *backBuffer = (uint8_t *)calloc(1, bufferSize);
  if (!backBuffer) {
    Serial.println("Demo: buffer alloc failed");
    file.close();
    return false;
  }

  uint8_t *lineBuffer = (uint8_t *)malloc((size_t)lineStride);
  if (!lineBuffer) {
    free(backBuffer);
    Serial.println("Demo: line buffer alloc failed");
    file.close();
    return false;
  }

  tpSetBackBuffer(backBuffer, printerWidth, height);
  tpFill(0x00);

  for (int y = 0; y < height; y++) {
    int row = topDown ? y : (height - 1 - y);
    uint32_t seekPos = offset + (uint32_t)(row * lineStride);
    file.seek(seekPos, SeekSet);
    if (file.read(lineBuffer, lineStride) != lineStride) {
      Serial.println("Demo: BMP read failed");
      free(lineBuffer);
      free(backBuffer);
      file.close();
      return false;
    }

    for (int x = 0; x < width; x++) {
      int lineIndex = x * 3;
      uint8_t b = lineBuffer[lineIndex + 0];
      uint8_t g = lineBuffer[lineIndex + 1];
      uint8_t r = lineBuffer[lineIndex + 2];
      uint8_t lum = demo_luma(r, g, b);
      bool pixelOn = (lum < THERMAL_THRESHOLD);
      if (THERMAL_INVERT) {
        pixelOn = !pixelOn;
      }
      int outX = x + xOffset;
      if (pixelOn && outX >= 0 && outX < printerWidth) {
        tpSetPixel(outX, y, 1);
      }
    }
  }

  tpPrintBuffer();
  if (THERMAL_FEED_LINES > 0) {
    tpFeed(THERMAL_FEED_LINES);
  }

  free(lineBuffer);
  free(backBuffer);
  file.close();

  return true;
}

void setup()
{
  Serial.begin(115200);
  delay(500);

  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS)) {
    Serial.println("Demo: SD init failed");
    return;
  }

  Serial.println("Demo: place /demo.bmp on SD (24-bit BMP)");
  demo_print_bmp("/demo.bmp");
}

void loop()
{
}
