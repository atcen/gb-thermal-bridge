#include "config.h"

#ifdef ENABLE_THERMAL_PRINTER

#include <FS.h>
#include <Thermal_Printer.h>
#include "./includes/gblink/gbp_tiles.h"

#ifndef THERMAL_PRINTER_NAME
#define THERMAL_PRINTER_NAME ""
#endif
#ifndef THERMAL_PRINTER_MAC
#define THERMAL_PRINTER_MAC ""
#endif
#ifndef THERMAL_SCAN_SECONDS
#define THERMAL_SCAN_SECONDS 5
#endif
#ifndef THERMAL_ENERGY
#define THERMAL_ENERGY 0
#endif
#ifndef THERMAL_THRESHOLD
#define THERMAL_THRESHOLD 160
#endif
#ifndef THERMAL_INVERT
#define THERMAL_INVERT 0
#endif
#ifndef THERMAL_PRINTER_WIDTH
#define THERMAL_PRINTER_WIDTH 0
#endif
#ifndef THERMAL_FEED_LINES
#define THERMAL_FEED_LINES 0
#endif

#ifndef USE_SD_STORAGE
#define USE_SD_STORAGE 0
#endif

static bool thermal_printer_connect()
{
  if (tpIsConnected()) {
    return true;
  }

  const char *mac = THERMAL_PRINTER_MAC;
  const char *name = THERMAL_PRINTER_NAME;

  if (mac[0] != '\0') {
    if (!tpConnect(mac)) {
      return false;
    }
  } else {
    if (name[0] != '\0') {
      tpScan(name, THERMAL_SCAN_SECONDS);
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

static bool thermal_bmp_read_header(File &file, int32_t *width, int32_t *height, uint32_t *offset, uint16_t *bpp)
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

static uint8_t thermal_luma(uint8_t r, uint8_t g, uint8_t b)
{
  return (uint8_t)((r * 30 + g * 59 + b * 11) / 100);
}

bool thermal_print_bmp(const char *bmpPath)
{
  if (!thermal_printer_connect()) {
    Serial.println("Thermal printer: not connected");
    return false;
  }

  File file = FSYS.open(bmpPath, "r");
  if (!file) {
    Serial.println("Thermal printer: failed to open BMP");
    return false;
  }

  int32_t width = 0;
  int32_t height = 0;
  uint32_t offset = 0;
  uint16_t bpp = 0;

  if (!thermal_bmp_read_header(file, &width, &height, &offset, &bpp)) {
    Serial.println("Thermal printer: invalid BMP header");
    file.close();
    return false;
  }

  if (bpp != 24 || width <= 0 || height == 0) {
    Serial.println("Thermal printer: unsupported BMP format");
    file.close();
    return false;
  }

  bool topDown = false;
  if (height < 0) {
    height = -height;
    topDown = true;
  }

  int printerWidth = THERMAL_PRINTER_WIDTH;
  if (printerWidth <= 0) {
    printerWidth = tpGetWidth();
  }
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
    Serial.println("Thermal printer: buffer alloc failed");
    file.close();
    return false;
  }

  uint8_t *lineBuffer = (uint8_t *)malloc((size_t)lineStride);
  if (!lineBuffer) {
    free(backBuffer);
    Serial.println("Thermal printer: line buffer alloc failed");
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
      Serial.println("Thermal printer: BMP read failed");
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
      uint8_t lum = thermal_luma(r, g, b);
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

#if !USE_SD_STORAGE
static uint8_t thermal_tone_to_luma(uint8_t tone)
{
  static const uint8_t toneMap[4] = {255, 170, 85, 0};
  return toneMap[tone & 0x03];
}

bool thermal_print_tiles(const gbp_tile_t *gbp_tiles)
{
  if (!thermal_printer_connect()) {
    Serial.println("Thermal printer: not connected");
    return false;
  }

  const int imageWidth = GBP_TILE_PIXEL_WIDTH * GBP_TILES_PER_LINE;
  const int imageHeight = (int)(gbp_tiles->tileRowOffset * GBP_TILE_PIXEL_HEIGHT);
  if (imageHeight <= 0) {
    return false;
  }

  int printerWidth = THERMAL_PRINTER_WIDTH;
  if (printerWidth <= 0) {
    printerWidth = tpGetWidth();
  }
  if (printerWidth <= 0) {
    printerWidth = imageWidth;
  }

  int xOffset = 0;
#ifdef THERMAL_CENTER_OUTPUT
  if (printerWidth > imageWidth) {
    xOffset = (printerWidth - imageWidth) / 2;
  }
#endif

  const int pitch = (printerWidth + 7) / 8;
  size_t bufferSize = (size_t)pitch * (size_t)imageHeight;
  uint8_t *backBuffer = (uint8_t *)calloc(1, bufferSize);
  if (!backBuffer) {
    Serial.println("Thermal printer: buffer alloc failed");
    return false;
  }

  tpSetBackBuffer(backBuffer, printerWidth, imageHeight);

  for (int y = 0; y < imageHeight; y++) {
    for (int x = 0; x < imageWidth; x++) {
      uint8_t packed = gbp_tiles->bmpLineBuffer[y][GBP_TILE_2BIT_LINEPACK_INDEX(x)];
      uint8_t tone = (packed >> GBP_TILE_2BIT_LINEPACK_BITOFFSET(x)) & 0x03;
      uint8_t lum = thermal_tone_to_luma(tone);
      bool pixelOn = (lum < THERMAL_THRESHOLD);
      if (THERMAL_INVERT) {
        pixelOn = !pixelOn;
      }
      int outX = x + xOffset;
      if (pixelOn && outX >= 0 && outX < printerWidth) {
        backBuffer[y * pitch + (outX >> 3)] |= (uint8_t)(0x80 >> (outX & 7));
      }
    }
  }

  tpPrintBuffer();
  if (THERMAL_FEED_LINES > 0) {
    tpFeed(THERMAL_FEED_LINES);
  }

  free(backBuffer);
  return true;
}
#endif

#endif // ENABLE_THERMAL_PRINTER
