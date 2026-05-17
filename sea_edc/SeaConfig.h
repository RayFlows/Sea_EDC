#pragma once

#include <cstdint>

namespace sea {

constexpr int kPanelWidth = 240;
constexpr int kPanelHeight = 240;

constexpr int kViewWidth = 240;
constexpr int kViewHeight = 135;
constexpr int kViewLeft = (kPanelWidth - kViewWidth) / 2;
constexpr int kViewTop = (kPanelHeight - kViewHeight) / 2;

constexpr int kWaterCols = 40;
constexpr int kWaterRows = 19;
constexpr int kPixelW = 6;
constexpr int kPixelH = 7;
constexpr int kTankCols = kWaterCols + 2;
constexpr int kTankRows = kWaterRows + 2;
constexpr int kPaintLeft = (kViewWidth - kWaterCols * kPixelW) / 2;
constexpr int kPaintTop = (kViewHeight - kWaterRows * kPixelH) / 2;

constexpr int kMaxDroplets = 1800;
constexpr int kStartDroplets = 1000;

constexpr float kGravityScale = 9.81f * 5.0f;
constexpr float kStepSeconds = 1.0f / 36.0f;
constexpr float kMaxElapsedSeconds = 0.10f;
constexpr float kGridMix = 0.95f;
constexpr int kPressurePasses = 14;
constexpr float kPressurePush = 1.85f;
constexpr int kSeparationPasses = 1;
constexpr float kDropletRadius = 0.27f;
constexpr float kWallBounce = 0.28f;

constexpr uint16_t rgb565(int r, int g, int b) {
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

constexpr uint16_t kSeaPalette[8] = {
  rgb565(0, 10, 28),
  rgb565(220, 246, 255),
  rgb565(160, 226, 255),
  rgb565(92, 206, 255),
  rgb565(42, 182, 248),
  rgb565(20, 152, 232),
  rgb565(10, 118, 210),
  rgb565(8, 80, 176),
};

constexpr uint16_t kEmptyColor = kSeaPalette[0];
constexpr float kFullCellMass = 2.0f;
constexpr float kPaletteSlope = 2.55f;
constexpr float kSurfaceMass = 1.0f;
constexpr float kAirNeighborMass = 0.4f;
constexpr int kSurfaceLight = 14;
constexpr uint32_t kSparkleMs = 500;
constexpr int kSparkleRange = 30;
constexpr float kDensitySmoothing = 0.35f;
constexpr uint32_t kGlintMs = 110;
constexpr float kGlintMotionThreshold = 0.055f;
constexpr int kGlintBaseChance = 3;
constexpr int kGlintEdgeChance = 12;
constexpr int kGlintLight = 42;

constexpr int kPhysicsStack = 8192;
constexpr int kPhysicsCore = 1;
constexpr int kPhysicsPriority = 1;

}  // namespace sea
