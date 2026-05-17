#pragma once

#include <array>
#include <cstdint>

#include "SeaConfig.h"

namespace sea {

enum class CellState : uint8_t {
  Wall = 0,
  Water = 1,
  Empty = 2,
};

class TankGrid {
 public:
  static constexpr int kCols = kTankCols;
  static constexpr int kRows = kTankRows;
  static constexpr int kXStride = kCols + 1;
  static constexpr int kYStride = kCols;
  static constexpr int kXFaceCount = (kCols + 1) * kRows;
  static constexpr int kYFaceCount = kCols * (kRows + 1);
  static constexpr int kCellCount = kCols * kRows;

  std::array<float, kXFaceCount> xVel{};
  std::array<float, kYFaceCount> yVel{};
  std::array<float, kXFaceCount> oldXVel{};
  std::array<float, kYFaceCount> oldYVel{};
  std::array<float, kXFaceCount> xWeight{};
  std::array<float, kYFaceCount> yWeight{};
  std::array<float, kCellCount> open{};
  std::array<float, kCellCount> mass{};
  std::array<CellState, kCellCount> cell{};

  static constexpr int xFace(int x, int y) { return y * kXStride + x; }
  static constexpr int yFace(int x, int y) { return y * kYStride + x; }
  static constexpr int at(int x, int y) { return y * kCols + x; }

  void clearVelocities() {
    xVel.fill(0.0f);
    yVel.fill(0.0f);
    xWeight.fill(0.0f);
    yWeight.fill(0.0f);
  }

  void resetCells() {
    open.fill(1.0f);
    cell.fill(CellState::Empty);

    for (int x = 0; x < kCols; ++x) {
      open[at(x, 0)] = 0.0f;
      open[at(x, kRows - 1)] = 0.0f;
      cell[at(x, 0)] = CellState::Wall;
      cell[at(x, kRows - 1)] = CellState::Wall;
    }

    for (int y = 0; y < kRows; ++y) {
      open[at(0, y)] = 0.0f;
      open[at(kCols - 1, y)] = 0.0f;
      cell[at(0, y)] = CellState::Wall;
      cell[at(kCols - 1, y)] = CellState::Wall;
    }
  }
};

}  // namespace sea
