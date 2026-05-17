#pragma once

#include <array>
#include <vector>

#include "Droplet.h"
#include "SeaConfig.h"
#include "TankGrid.h"

namespace sea {

class WaterModel {
 public:
  WaterModel();

  void reset(int count = kStartDroplets);
  void advance(float seconds, float gx, float gy);
  void rebuildRenderMass();

  const std::array<float, TankGrid::kCellCount>& renderMass() const { return grid_.mass; }
  int dropletCount() const { return static_cast<int>(drops_.size()); }

 private:
  void seedPool(int count);
  void integrate(float seconds, float gx, float gy);
  void separateNeighbors();
  void collideWithTank();
  void splatVelocityToGrid();
  void makeGridDivergenceFree();
  void pullVelocityFromGrid();
  void splatMass(float x, float y, float weight);

  std::vector<Droplet> drops_;
  TankGrid grid_;
};

}  // namespace sea
