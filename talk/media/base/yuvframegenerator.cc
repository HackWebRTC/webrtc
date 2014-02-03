#include "talk/media/base/yuvframegenerator.h"

#include <string.h>
#include <sstream>

#include "talk/base/basictypes.h"
#include "talk/base/common.h"

namespace cricket {

// These values were figured out by trial and error. If you change any
// basic parameters e.g. unit-bar size or bars-x-offset, you may need to change
// background-width/background-height.
const int kBarcodeBackgroundWidth = 160;
const int kBarcodeBackgroundHeight = 100;
const int kBarsXOffset = 12;
const int kBarsYOffset = 4;
const int kUnitBarSize = 2;
const int kBarcodeNormalBarHeight = 80;
const int kBarcodeGuardBarHeight = 96;
const int kBarcodeMaxEncodableDigits = 7;
const int kBarcodeMaxEncodableValue = 9999999;

YuvFrameGenerator::YuvFrameGenerator(int width, int height,
                                     bool enable_barcode) {
  width_ = width;
  height_ = height;
  frame_index_ = 0;
  int size = width_ * height_;
  int qsize = size / 4;
  frame_data_size_ = size + 2 * qsize;
  y_data_ = new uint8[size];
  u_data_ = new uint8[qsize];
  v_data_ = new uint8[qsize];
  if (enable_barcode) {
    ASSERT(width_ >= kBarcodeBackgroundWidth);
    ASSERT(height_>= kBarcodeBackgroundHeight);
    barcode_start_x_ = 0;
    barcode_start_y_ = height_ - kBarcodeBackgroundHeight;
  } else {
    barcode_start_x_ = -1;
    barcode_start_y_ = -1;
  }
}

YuvFrameGenerator::~YuvFrameGenerator() {
  delete y_data_;
  delete u_data_;
  delete v_data_;
}

void YuvFrameGenerator::GenerateNextFrame(uint8* frame_buffer,
                                          int32 barcode_value) {
  int size = width_ * height_;
  int qsize = size / 4;
  memset(y_data_, 0, size);
  memset(u_data_, 0, qsize);
  memset(v_data_, 0, qsize);

  DrawLandscape(y_data_, width_, height_);
  DrawGradientX(u_data_, width_/2, height_/2);
  DrawGradientY(v_data_, width_/2, height_/2);
  DrawMovingLineX(u_data_, width_/2, height_/2, frame_index_);
  DrawMovingLineY(v_data_, width_/2, height_/2, frame_index_);
  DrawBouncingCube(y_data_, width_, height_, frame_index_);

  if (barcode_value >= 0) {
      ASSERT(barcode_start_x_ != -1);
      DrawBarcode(barcode_value);
  }

  memcpy(frame_buffer, y_data_, size);
  frame_buffer += size;
  memcpy(frame_buffer, u_data_, qsize);
  frame_buffer += qsize;
  memcpy(frame_buffer, v_data_, qsize);

  frame_index_ = (frame_index_ + 1) & 0x0000FFFF;
}

void YuvFrameGenerator::DrawLandscape(uint8 *p, int w, int h) {
  int x, y;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      p[x + y * w] = x % (y+1);
      if (((x > w / 2 - (w / 32)) && (x < w / 2 + (w / 32))) ||
          ((y > h / 2 - (h / 32)) && (y < h / 2 + (h / 32)))) {
        p[x + y * w] = (((x + y) / 8 % 2)) ? 255 : 0;
      }
    }
  }
}

void YuvFrameGenerator::DrawGradientX(uint8 *p, int w, int h) {
  int x, y;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      p[x + y * w] = (x << 8) / w;
    }
  }
}

void YuvFrameGenerator::DrawGradientY(uint8 *p, int w, int h) {
  int x, y;
  for (y = 0; y < h; y++) {
    for (x = 0; x < w; x++) {
      p[x + y * w] = (y << 8) / h;
    }
  }
}

void YuvFrameGenerator::DrawMovingLineX(uint8 *p, int w, int h, int n) {
  int x, y;
  x = n % (w * 2);
  if (x >= w) x = w + w - x - 1;
  for (y = 0; y < h; y++) {
    p[x + y * w] = 255;
  }
}

void YuvFrameGenerator::DrawMovingLineY(uint8 *p, int w, int h, int n) {
  int x, y;
  y = n % (h * 2);
  if (y >= h) y = h + h - y - 1;
  for (x = 0; x < w; x++) {
    p[x + y * w] = 255;
  }
}

void YuvFrameGenerator::DrawBouncingCube(uint8 *p, int w, int h, int n) {
  int x, y, pw, ph, px, py;
  pw = w / 16;
  ph = h / 16;
  px = n % (w * 2);
  py = n % (h * 2);
  if (px >= w) px = w + w - px - 1;
  if (py >= h) py = h + h - py - 1;
  for (y = py - ph; y < py + ph; y++) {
    if (y >=0 && y < h) {
      for (x = px - pw; x < px + pw; x++) {
        if (x >= 0 && x < w) {
          p[x + y * w] = 255;
        }
      }
    }
  }
}

void YuvFrameGenerator::GetBarcodeBounds(int* top, int* left,
                                         int* width, int* height) {
  ASSERT(barcode_start_x_ != -1);
  *top = barcode_start_y_;
  *left = barcode_start_x_;
  *width = kBarcodeBackgroundWidth;
  *height = kBarcodeBackgroundHeight;
}

static void ComputeBarcodeDigits(uint32 value, std::stringstream* result) {
  // Serialize |value| as 7-char string, padded with 0's to the left.
  result->width(kBarcodeMaxEncodableDigits);
  result->fill('0');
  *result << value;

  // Compute check-digit and append to result. Steps described here:
  // http://en.wikipedia.org/wiki/European_Article_Number#Calculation_of_checksum_digit
  int sum = 0;
  for (int pos = 1; pos <= kBarcodeMaxEncodableDigits; pos++) {
    char next_char;
    result->get(next_char);
    uint8 digit = next_char - '0';
    sum += digit * (pos % 2 ? 3 : 1);
  }
  uint8 check_digit = sum % 10;
  if (check_digit != 0) {
    check_digit = 10 - check_digit;
  }

  *result << static_cast<int>(check_digit);
  result->seekg(0);
}

void YuvFrameGenerator::DrawBarcode(uint32 value) {
  std::stringstream value_str_stream;
  ComputeBarcodeDigits(value, &value_str_stream);

  // Draw white filled rectangle as background to barcode.
  DrawBlockRectangle(y_data_, barcode_start_x_, barcode_start_y_,
                     kBarcodeBackgroundWidth, kBarcodeBackgroundHeight,
                     width_, 255);
  DrawBlockRectangle(u_data_, barcode_start_x_ / 2, barcode_start_y_ / 2,
                     kBarcodeBackgroundWidth / 2, kBarcodeBackgroundHeight / 2,
                     width_ / 2, 128);
  DrawBlockRectangle(v_data_, barcode_start_x_ / 2, barcode_start_y_ / 2,
                     kBarcodeBackgroundWidth / 2, kBarcodeBackgroundHeight / 2,
                     width_ / 2, 128);

  // Scan through chars (digits) and draw black bars.
  int x = barcode_start_x_ + kBarsXOffset;
  int y = barcode_start_y_ + kBarsYOffset;
  int pos = 0;
  x = DrawSideGuardBars(x, y, kBarcodeGuardBarHeight);
  while (true) {
    char next_char;
    value_str_stream.get(next_char);
    if (!value_str_stream.good()) {
      break;
    }
    if (pos++ == 4) {
      x = DrawMiddleGuardBars(x, y, kBarcodeGuardBarHeight);
    }
    uint8 digit = next_char - '0';
    x = DrawEanEncodedDigit(digit, x, y, kBarcodeNormalBarHeight, pos > 4);
  }
  x = DrawSideGuardBars(x, y, kBarcodeGuardBarHeight);
}

int YuvFrameGenerator::DrawMiddleGuardBars(int x, int y, int height) {
  x += kUnitBarSize;
  DrawBlockRectangle(y_data_, x, y, kUnitBarSize, height, width_, 0);
  x += (kUnitBarSize * 2);
  DrawBlockRectangle(y_data_, x, y, kUnitBarSize, height, width_, 0);
  return x + (kUnitBarSize * 2);
}

int YuvFrameGenerator::DrawSideGuardBars(int x, int y, int height) {
  DrawBlockRectangle(y_data_, x, y, kUnitBarSize, height, width_, 0);
  x += (kUnitBarSize * 2);
  DrawBlockRectangle(y_data_, x, y, kUnitBarSize, height, width_, 0);
  return x + kUnitBarSize;
}

// For each digit: 0-9, |kEanEncodings| contains a bit-mask indicating
// which bars are black (1) and which are blank (0). These are for the L-code
// only. R-code values are bitwise negation of these. Reference:
// http://en.wikipedia.org/wiki/European_Article_Number#Binary_encoding_of_data_digits_into_EAN-13_barcode // NOLINT
const uint8 kEanEncodings[] = { 13, 25, 19, 61, 35, 49, 47, 59, 55, 11 };

int YuvFrameGenerator::DrawEanEncodedDigit(int digit, int x, int y,
                                           int height, bool flip) {
  uint8 ean_encoding = kEanEncodings[digit];
  if (flip) {
    ean_encoding = ~ean_encoding;
  }
  uint8 mask = 0x40;
  for (int i = 6; i >= 0; i--, mask >>= 1) {
    if (ean_encoding & mask) {
      DrawBlockRectangle(y_data_, x, y, kUnitBarSize, height, width_, 0);
    }
    x += kUnitBarSize;
  }
  return x;
}

void YuvFrameGenerator::DrawBlockRectangle(uint8* p,
    int x_start, int y_start, int width, int height, int pitch, uint8 value) {
  for (int x = x_start; x < x_start + width; x++) {
    for (int y = y_start; y < y_start + height; y++) {
      p[x + y * pitch] = value;
    }
  }
}

}  // namespace cricket
