#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <array>
#include <cstdint>

#include "SeaConfig.h"
#include "TankGrid.h"

namespace sea {

class SurfaceRenderer {
 public:
  SurfaceRenderer();

  void begin(Adafruit_ST7789& display);
  void draw(const float* mass, int dropletCount, float fps);

 private:
  void clearFrame();
  void rect(int x, int y, int w, int h, uint16_t color);
  void dot(int x, int y, uint16_t color);
  void colorFromLevel(float level, int& r, int& g, int& b) const;
  void drawGlint(int cx, int cy, int airX, int airY, uint8_t hash, float energy);

  Adafruit_ST7789* display_;
  uint16_t* frame_;
  bool ready_;
  bool firstSmooth_;
  bool firstMotion_;
  float sceneMotion_;
  std::array<float, TankGrid::kCellCount> smoothMass_;
  std::array<float, TankGrid::kCellCount> lastMass_;
};

}  // namespace sea
