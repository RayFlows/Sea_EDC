#include "SurfaceRenderer.h"

#include <Arduino.h>
#include <cmath>
#include <esp_heap_caps.h>

namespace sea {

namespace {

inline uint8_t channel5To8(uint16_t value) {
  return static_cast<uint8_t>((value << 3) | (value >> 2));
}

inline uint8_t channel6To8(uint16_t value) {
  return static_cast<uint8_t>((value << 2) | (value >> 4));
}

void unpack(uint16_t color, uint8_t& r, uint8_t& g, uint8_t& b) {
  r = channel5To8((color >> 11) & 0x1F);
  g = channel6To8((color >> 5) & 0x3F);
  b = channel5To8(color & 0x1F);
}

uint16_t pack(int r, int g, int b) {
  if (r < 0) r = 0;
  if (r > 255) r = 255;
  if (g < 0) g = 0;
  if (g > 255) g = 255;
  if (b < 0) b = 0;
  if (b > 255) b = 255;
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

uint32_t mixHash(int x, int y, uint32_t salt) {
  uint32_t v = static_cast<uint32_t>(x) * 2246822519u ^
               static_cast<uint32_t>(y) * 3266489917u ^
               salt * 668265263u;
  v ^= v >> 15;
  v *= 2246822519u;
  v ^= v >> 13;
  return v;
}

int sparkleJitter(int x, int y, uint32_t bucket) {
  return static_cast<int>((mixHash(x, y, bucket) >> 24) % kSparkleRange) -
         kSparkleRange / 2;
}

}  // namespace

SurfaceRenderer::SurfaceRenderer()
    : display_(nullptr),
      frame_(nullptr),
      ready_(false),
      firstSmooth_(true),
      firstMotion_(true),
      sceneMotion_(0.0f),
      smoothMass_{},
      lastMass_{} {}

void SurfaceRenderer::begin(Adafruit_ST7789& display) {
  display_ = &display;
  if (frame_ == nullptr) {
    frame_ = static_cast<uint16_t*>(
        heap_caps_malloc(kViewWidth * kViewHeight * sizeof(uint16_t), MALLOC_CAP_8BIT));
  }
  ready_ = frame_ != nullptr;
  if (ready_) clearFrame();
}

void SurfaceRenderer::clearFrame() {
  for (int i = 0; i < kViewWidth * kViewHeight; ++i) frame_[i] = kEmptyColor;
}

void SurfaceRenderer::rect(int x, int y, int w, int h, uint16_t color) {
  if (x < 0) {
    w += x;
    x = 0;
  }
  if (y < 0) {
    h += y;
    y = 0;
  }
  if (x + w > kViewWidth) w = kViewWidth - x;
  if (y + h > kViewHeight) h = kViewHeight - y;
  if (w <= 0 || h <= 0) return;

  for (int py = y; py < y + h; ++py) {
    uint16_t* row = frame_ + py * kViewWidth + x;
    for (int px = 0; px < w; ++px) row[px] = color;
  }
}

void SurfaceRenderer::dot(int x, int y, uint16_t color) {
  if (x < kPaintLeft || x >= kPaintLeft + kWaterCols * kPixelW) return;
  if (y < kPaintTop || y >= kPaintTop + kWaterRows * kPixelH) return;
  frame_[y * kViewWidth + x] = color;
}

void SurfaceRenderer::colorFromLevel(float level, int& r, int& g, int& b) const {
  if (level < 0.0f) level = 0.0f;
  if (level > 7.0f) level = 7.0f;

  int low = static_cast<int>(level);
  if (low > 6) low = 6;
  float t = level - static_cast<float>(low);
  uint8_t r0, g0, b0, r1, g1, b1;
  unpack(kSeaPalette[low], r0, g0, b0);
  unpack(kSeaPalette[low + 1], r1, g1, b1);

  r = static_cast<int>(r0 * (1.0f - t) + r1 * t);
  g = static_cast<int>(g0 * (1.0f - t) + g1 * t);
  b = static_cast<int>(b0 * (1.0f - t) + b1 * t);
}

void SurfaceRenderer::drawGlint(int cx, int cy, int airX, int airY, uint8_t hash, float energy) {
  int boost = kGlintLight + static_cast<int>(energy * 56.0f);
  uint16_t core = pack(150 + boost, 218 + boost, 230 + boost);
  uint16_t mid = pack(86 + boost, 184 + boost, 214 + boost);
  uint16_t soft = pack(42 + boost / 2, 150 + boost / 2, 202 + boost / 2);

  int baseX = kPaintLeft + cx * kPixelW;
  int baseY = kPaintTop + cy * kPixelH;

  if (airY != 0) {
    int y = airY < 0 ? 1 + ((hash >> 3) & 1) : kPixelH - 3 + ((hash >> 3) & 1);
    int x = 1 + (hash & 0x01);
    int len = 3 + ((hash >> 5) & 0x01);
    for (int i = 0; i < len; ++i) dot(baseX + x + i, baseY + y, i < 2 ? core : mid);
    dot(baseX + x + 1, baseY + y + (airY < 0 ? 1 : -1), mid);
    if (hash & 0x20) dot(baseX + x - 2, baseY + y + 1, soft);
    if (hash & 0x40) dot(baseX + x + len + 1, baseY + y - 1, soft);
    if (hash & 0x80) dot(baseX + x + len, baseY + y + (airY < 0 ? 2 : -2), mid);
  } else {
    int x = airX < 0 ? 1 + ((hash >> 3) & 1) : kPixelW - 3 + ((hash >> 3) & 1);
    int y = 1 + (hash & 0x01);
    int len = 3 + ((hash >> 5) & 0x01);
    for (int i = 0; i < len; ++i) dot(baseX + x, baseY + y + i, i < 2 ? core : mid);
    dot(baseX + x + (airX < 0 ? 1 : -1), baseY + y + 1, mid);
    if (hash & 0x20) dot(baseX + x + 1, baseY + y - 2, soft);
    if (hash & 0x40) dot(baseX + x - 1, baseY + y + len + 1, soft);
    if (hash & 0x80) dot(baseX + x + (airX < 0 ? 2 : -2), baseY + y + len, mid);
  }
}

void SurfaceRenderer::draw(const float* mass, int, float) {
  if (!ready_ || display_ == nullptr) return;

  clearFrame();

  if (firstSmooth_) {
    for (size_t i = 0; i < TankGrid::kCellCount; ++i) smoothMass_[i] = mass[i];
    firstSmooth_ = false;
  } else {
    for (size_t i = 0; i < TankGrid::kCellCount; ++i) {
      smoothMass_[i] = smoothMass_[i] * kDensitySmoothing + mass[i] * (1.0f - kDensitySmoothing);
    }
  }

  float motion = 0.0f;
  if (firstMotion_) {
    for (size_t i = 0; i < TankGrid::kCellCount; ++i) lastMass_[i] = mass[i];
    firstMotion_ = false;
  } else {
    for (int y = 0; y < kWaterRows; ++y) {
      for (int x = 0; x < kWaterCols; ++x) {
        int idx = TankGrid::at(x + 1, y + 1);
        motion += fabsf(mass[idx] - lastMass_[idx]);
      }
    }
    motion /= static_cast<float>(kWaterCols * kWaterRows);
  }
  sceneMotion_ = sceneMotion_ * 0.82f + motion * 0.18f;

  uint32_t sparkleBucket = millis() / kSparkleMs;
  uint32_t glintBucket = millis() / kGlintMs;

  for (int y = 0; y < kWaterRows; ++y) {
    for (int x = 0; x < kWaterCols; ++x) {
      int cell = TankGrid::at(x + 1, y + 1);
      float m = smoothMass_[cell];
      float level = m / kFullCellMass * kPaletteSlope;

      bool edge = false;
      if (m >= kSurfaceMass) {
        int gx = x + 1;
        int gy = y + 1;
        edge = smoothMass_[TankGrid::at(gx, gy - 1)] < kAirNeighborMass ||
               smoothMass_[TankGrid::at(gx, gy + 1)] < kAirNeighborMass ||
               smoothMass_[TankGrid::at(gx - 1, gy)] < kAirNeighborMass ||
               smoothMass_[TankGrid::at(gx + 1, gy)] < kAirNeighborMass;
      }

      if (edge) {
        level -= 1.0f;
        if (level < 1.0f) level = 1.0f;
      }

      int r, g, b;
      colorFromLevel(level, r, g, b);
      if (edge) {
        int sparkle = kSurfaceLight + sparkleJitter(x, y, sparkleBucket);
        r += sparkle;
        g += sparkle;
        b += sparkle;
      }

      rect(kPaintLeft + x * kPixelW, kPaintTop + y * kPixelH,
           kPixelW, kPixelH, pack(r, g, b));
    }
  }

  for (int y = 0; y < kWaterRows; ++y) {
    for (int x = 0; x < kWaterCols; ++x) {
      int gx = x + 1;
      int gy = y + 1;
      int cell = TankGrid::at(gx, gy);
      if (smoothMass_[cell] < kSurfaceMass) continue;

      int airX = 0;
      int airY = 0;
      if (smoothMass_[TankGrid::at(gx, gy - 1)] < kAirNeighborMass) {
        airY = -1;
      } else if (smoothMass_[TankGrid::at(gx, gy + 1)] < kAirNeighborMass) {
        airY = 1;
      } else if (smoothMass_[TankGrid::at(gx - 1, gy)] < kAirNeighborMass) {
        airX = -1;
      } else if (smoothMass_[TankGrid::at(gx + 1, gy)] < kAirNeighborMass) {
        airX = 1;
      }
      if (airX == 0 && airY == 0) continue;

      float localMotion = fabsf(mass[cell] - lastMass_[cell]);
      float energy = localMotion * 0.70f + sceneMotion_ * 0.30f;
      if (energy < kGlintMotionThreshold) continue;

      int chance = kGlintBaseChance + kGlintEdgeChance + static_cast<int>(energy * 32.0f);
      if (chance > 38) chance = 38;

      uint8_t h = static_cast<uint8_t>(mixHash(x, y, glintBucket) >> 24);
      if (h < chance) drawGlint(x, y, airX, airY, h, energy);
    }
  }

  display_->drawRGBBitmap(kViewLeft, kViewTop, frame_, kViewWidth, kViewHeight);
  for (size_t i = 0; i < TankGrid::kCellCount; ++i) lastMass_[i] = mass[i];
}

}  // namespace sea
