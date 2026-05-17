#include "WaterModel.h"

#include <algorithm>
#include <cmath>

namespace sea {

namespace {

struct QuadWeights {
  int x;
  int y;
  float w00;
  float w10;
  float w01;
  float w11;
};

inline float clampf(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

inline int clampi(int value, int lo, int hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

QuadWeights quadAt(float x, float y, float offsetX, float offsetY, int maxBaseX, int maxBaseY) {
  float gx = x - offsetX;
  float gy = y - offsetY;
  int ix = clampi(static_cast<int>(std::floor(gx)), 0, maxBaseX);
  int iy = clampi(static_cast<int>(std::floor(gy)), 0, maxBaseY);
  float fx = clampf(gx - static_cast<float>(ix), 0.0f, 1.0f);
  float fy = clampf(gy - static_cast<float>(iy), 0.0f, 1.0f);
  return {ix, iy,
          (1.0f - fx) * (1.0f - fy), fx * (1.0f - fy),
          (1.0f - fx) * fy, fx * fy};
}

}  // namespace

WaterModel::WaterModel() = default;

void WaterModel::reset(int count) {
  drops_.clear();
  drops_.reserve(kMaxDroplets);
  grid_.resetCells();
  grid_.clearVelocities();
  grid_.oldXVel.fill(0.0f);
  grid_.oldYVel.fill(0.0f);
  grid_.mass.fill(0.0f);
  seedPool(count);
  rebuildRenderMass();
}

void WaterModel::seedPool(int count) {
  const float gap = 0.45f;
  const float left = 1.0f + gap * 0.5f;
  const float right = static_cast<float>(kTankCols - 1) - gap * 0.5f;
  const float top = 1.0f + gap * 0.5f;
  const float bottom = 1.0f + static_cast<float>(kWaterRows) * 0.70f;

  for (float y = top; y <= bottom; y += gap) {
    for (float x = left; x <= right; x += gap) {
      if (static_cast<int>(drops_.size()) >= count) return;
      drops_.push_back({x, y, 0.0f, 0.0f});
    }
  }
}

void WaterModel::advance(float seconds, float gx, float gy) {
  integrate(seconds, gx, gy);
  for (int i = 0; i < kSeparationPasses; ++i) separateNeighbors();
  collideWithTank();
  splatVelocityToGrid();
  makeGridDivergenceFree();
  pullVelocityFromGrid();
}

void WaterModel::integrate(float seconds, float gx, float gy) {
  for (Droplet& d : drops_) {
    d.velX += gx * seconds;
    d.velY += gy * seconds;
    d.posX += d.velX * seconds;
    d.posY += d.velY * seconds;
  }
}

void WaterModel::separateNeighbors() {
  if (drops_.empty()) return;

  const float minGap = kDropletRadius * 2.0f;
  const float minGap2 = minGap * minGap;
  const float bucketScale = 1.0f / minGap;
  const int bucketsX = static_cast<int>(std::ceil(kTankCols * bucketScale)) + 1;
  const int bucketsY = static_cast<int>(std::ceil(kTankRows * bucketScale)) + 1;

  static std::vector<int> head;
  static std::vector<int> link;
  head.assign(bucketsX * bucketsY, -1);
  link.assign(drops_.size(), -1);

  auto bucketOf = [&](float x, float y) {
    int bx = clampi(static_cast<int>(x * bucketScale), 0, bucketsX - 1);
    int by = clampi(static_cast<int>(y * bucketScale), 0, bucketsY - 1);
    return by * bucketsX + bx;
  };

  for (int i = 0; i < static_cast<int>(drops_.size()); ++i) {
    int b = bucketOf(drops_[i].posX, drops_[i].posY);
    link[i] = head[b];
    head[b] = i;
  }

  for (int i = 0; i < static_cast<int>(drops_.size()); ++i) {
    Droplet& a = drops_[i];
    int bx = clampi(static_cast<int>(a.posX * bucketScale), 0, bucketsX - 1);
    int by = clampi(static_cast<int>(a.posY * bucketScale), 0, bucketsY - 1);

    for (int oy = -1; oy <= 1; ++oy) {
      for (int ox = -1; ox <= 1; ++ox) {
        int nx = bx + ox;
        int ny = by + oy;
        if (nx < 0 || ny < 0 || nx >= bucketsX || ny >= bucketsY) continue;

        for (int j = head[ny * bucketsX + nx]; j >= 0; j = link[j]) {
          if (j <= i) continue;
          Droplet& b = drops_[j];
          float dx = b.posX - a.posX;
          float dy = b.posY - a.posY;
          float dist2 = dx * dx + dy * dy;
          if (dist2 <= 0.0f || dist2 >= minGap2) continue;

          float dist = std::sqrt(dist2);
          float move = (minGap - dist) * 0.5f / dist;
          a.posX -= dx * move;
          a.posY -= dy * move;
          b.posX += dx * move;
          b.posY += dy * move;
        }
      }
    }
  }
}

void WaterModel::collideWithTank() {
  const float left = 1.0f + kDropletRadius;
  const float right = static_cast<float>(kTankCols - 1) - kDropletRadius;
  const float top = 1.0f + kDropletRadius;
  const float bottom = static_cast<float>(kTankRows - 1) - kDropletRadius;

  for (Droplet& d : drops_) {
    if (d.posX < left) {
      d.posX = left;
      if (d.velX < 0.0f) d.velX = -d.velX * kWallBounce;
    } else if (d.posX > right) {
      d.posX = right;
      if (d.velX > 0.0f) d.velX = -d.velX * kWallBounce;
    }

    if (d.posY < top) {
      d.posY = top;
      if (d.velY < 0.0f) d.velY = -d.velY * kWallBounce;
    } else if (d.posY > bottom) {
      d.posY = bottom;
      if (d.velY > 0.0f) d.velY = -d.velY * kWallBounce;
    }
  }
}

void WaterModel::splatVelocityToGrid() {
  grid_.clearVelocities();
  for (int i = 0; i < TankGrid::kCellCount; ++i) {
    if (grid_.cell[i] != CellState::Wall) grid_.cell[i] = CellState::Empty;
  }

  for (const Droplet& d : drops_) {
    int cx = static_cast<int>(d.posX);
    int cy = static_cast<int>(d.posY);
    if (cx >= 0 && cx < kTankCols && cy >= 0 && cy < kTankRows) {
      int cell = TankGrid::at(cx, cy);
      if (grid_.cell[cell] == CellState::Empty) grid_.cell[cell] = CellState::Water;
    }

    QuadWeights xs = quadAt(d.posX, d.posY, 0.0f, 0.5f, kTankCols - 1, kTankRows - 2);
    grid_.xVel[TankGrid::xFace(xs.x, xs.y)] += d.velX * xs.w00;
    grid_.xVel[TankGrid::xFace(xs.x + 1, xs.y)] += d.velX * xs.w10;
    grid_.xVel[TankGrid::xFace(xs.x, xs.y + 1)] += d.velX * xs.w01;
    grid_.xVel[TankGrid::xFace(xs.x + 1, xs.y + 1)] += d.velX * xs.w11;
    grid_.xWeight[TankGrid::xFace(xs.x, xs.y)] += xs.w00;
    grid_.xWeight[TankGrid::xFace(xs.x + 1, xs.y)] += xs.w10;
    grid_.xWeight[TankGrid::xFace(xs.x, xs.y + 1)] += xs.w01;
    grid_.xWeight[TankGrid::xFace(xs.x + 1, xs.y + 1)] += xs.w11;

    QuadWeights ys = quadAt(d.posX, d.posY, 0.5f, 0.0f, kTankCols - 2, kTankRows - 1);
    grid_.yVel[TankGrid::yFace(ys.x, ys.y)] += d.velY * ys.w00;
    grid_.yVel[TankGrid::yFace(ys.x + 1, ys.y)] += d.velY * ys.w10;
    grid_.yVel[TankGrid::yFace(ys.x, ys.y + 1)] += d.velY * ys.w01;
    grid_.yVel[TankGrid::yFace(ys.x + 1, ys.y + 1)] += d.velY * ys.w11;
    grid_.yWeight[TankGrid::yFace(ys.x, ys.y)] += ys.w00;
    grid_.yWeight[TankGrid::yFace(ys.x + 1, ys.y)] += ys.w10;
    grid_.yWeight[TankGrid::yFace(ys.x, ys.y + 1)] += ys.w01;
    grid_.yWeight[TankGrid::yFace(ys.x + 1, ys.y + 1)] += ys.w11;
  }

  for (int i = 0; i < TankGrid::kXFaceCount; ++i) {
    grid_.xVel[i] = grid_.xWeight[i] > 0.0f ? grid_.xVel[i] / grid_.xWeight[i] : 0.0f;
  }
  for (int i = 0; i < TankGrid::kYFaceCount; ++i) {
    grid_.yVel[i] = grid_.yWeight[i] > 0.0f ? grid_.yVel[i] / grid_.yWeight[i] : 0.0f;
  }

  grid_.oldXVel = grid_.xVel;
  grid_.oldYVel = grid_.yVel;
}

void WaterModel::makeGridDivergenceFree() {
  for (int pass = 0; pass < kPressurePasses; ++pass) {
    for (int y = 1; y < kTankRows - 1; ++y) {
      for (int x = 1; x < kTankCols - 1; ++x) {
        if (grid_.cell[TankGrid::at(x, y)] != CellState::Water) continue;

        float left = grid_.open[TankGrid::at(x - 1, y)];
        float right = grid_.open[TankGrid::at(x + 1, y)];
        float top = grid_.open[TankGrid::at(x, y - 1)];
        float bottom = grid_.open[TankGrid::at(x, y + 1)];
        float openSum = left + right + top + bottom;
        if (openSum <= 0.0f) continue;

        float divergence = grid_.xVel[TankGrid::xFace(x + 1, y)] -
                           grid_.xVel[TankGrid::xFace(x, y)] +
                           grid_.yVel[TankGrid::yFace(x, y + 1)] -
                           grid_.yVel[TankGrid::yFace(x, y)];
        float correction = -divergence * kPressurePush / openSum;

        grid_.xVel[TankGrid::xFace(x, y)] -= left * correction;
        grid_.xVel[TankGrid::xFace(x + 1, y)] += right * correction;
        grid_.yVel[TankGrid::yFace(x, y)] -= top * correction;
        grid_.yVel[TankGrid::yFace(x, y + 1)] += bottom * correction;
      }
    }
  }
}

void WaterModel::pullVelocityFromGrid() {
  const float picPart = 1.0f - kGridMix;

  for (Droplet& d : drops_) {
    QuadWeights xs = quadAt(d.posX, d.posY, 0.0f, 0.5f, kTankCols - 1, kTankRows - 2);
    int xi00 = TankGrid::xFace(xs.x, xs.y);
    int xi10 = TankGrid::xFace(xs.x + 1, xs.y);
    int xi01 = TankGrid::xFace(xs.x, xs.y + 1);
    int xi11 = TankGrid::xFace(xs.x + 1, xs.y + 1);
    float xPic = grid_.xVel[xi00] * xs.w00 + grid_.xVel[xi10] * xs.w10 +
                 grid_.xVel[xi01] * xs.w01 + grid_.xVel[xi11] * xs.w11;
    float xDelta = (grid_.xVel[xi00] - grid_.oldXVel[xi00]) * xs.w00 +
                   (grid_.xVel[xi10] - grid_.oldXVel[xi10]) * xs.w10 +
                   (grid_.xVel[xi01] - grid_.oldXVel[xi01]) * xs.w01 +
                   (grid_.xVel[xi11] - grid_.oldXVel[xi11]) * xs.w11;
    d.velX = picPart * xPic + kGridMix * (d.velX + xDelta);

    QuadWeights ys = quadAt(d.posX, d.posY, 0.5f, 0.0f, kTankCols - 2, kTankRows - 1);
    int yi00 = TankGrid::yFace(ys.x, ys.y);
    int yi10 = TankGrid::yFace(ys.x + 1, ys.y);
    int yi01 = TankGrid::yFace(ys.x, ys.y + 1);
    int yi11 = TankGrid::yFace(ys.x + 1, ys.y + 1);
    float yPic = grid_.yVel[yi00] * ys.w00 + grid_.yVel[yi10] * ys.w10 +
                 grid_.yVel[yi01] * ys.w01 + grid_.yVel[yi11] * ys.w11;
    float yDelta = (grid_.yVel[yi00] - grid_.oldYVel[yi00]) * ys.w00 +
                   (grid_.yVel[yi10] - grid_.oldYVel[yi10]) * ys.w10 +
                   (grid_.yVel[yi01] - grid_.oldYVel[yi01]) * ys.w01 +
                   (grid_.yVel[yi11] - grid_.oldYVel[yi11]) * ys.w11;
    d.velY = picPart * yPic + kGridMix * (d.velY + yDelta);
  }
}

void WaterModel::splatMass(float x, float y, float weight) {
  QuadWeights q = quadAt(x, y, 0.0f, 0.0f, kTankCols - 2, kTankRows - 2);
  grid_.mass[TankGrid::at(q.x, q.y)] += q.w00 * weight;
  grid_.mass[TankGrid::at(q.x + 1, q.y)] += q.w10 * weight;
  grid_.mass[TankGrid::at(q.x, q.y + 1)] += q.w01 * weight;
  grid_.mass[TankGrid::at(q.x + 1, q.y + 1)] += q.w11 * weight;
}

void WaterModel::rebuildRenderMass() {
  grid_.mass.fill(0.0f);

  constexpr float left = 1.0f;
  constexpr float top = 1.0f;
  const float right = static_cast<float>(kTankCols - 1);
  const float bottom = static_cast<float>(kTankRows - 1);

  for (const Droplet& d : drops_) {
    splatMass(d.posX, d.posY, 1.0f);

    bool mirrorLeft = d.posX < left + 1.0f;
    bool mirrorRight = d.posX > right - 1.0f;
    bool mirrorTop = d.posY < top + 1.0f;
    bool mirrorBottom = d.posY > bottom - 1.0f;

    if (mirrorLeft) splatMass(left * 2.0f - d.posX, d.posY, 1.0f);
    if (mirrorRight) splatMass(right * 2.0f - d.posX, d.posY, 1.0f);
    if (mirrorTop) splatMass(d.posX, top * 2.0f - d.posY, 1.0f);
    if (mirrorBottom) splatMass(d.posX, bottom * 2.0f - d.posY, 1.0f);

    if (mirrorLeft && mirrorTop) splatMass(left * 2.0f - d.posX, top * 2.0f - d.posY, 1.0f);
    if (mirrorRight && mirrorTop) splatMass(right * 2.0f - d.posX, top * 2.0f - d.posY, 1.0f);
    if (mirrorLeft && mirrorBottom) splatMass(left * 2.0f - d.posX, bottom * 2.0f - d.posY, 1.0f);
    if (mirrorRight && mirrorBottom) splatMass(right * 2.0f - d.posX, bottom * 2.0f - d.posY, 1.0f);
  }
}

}  // namespace sea
