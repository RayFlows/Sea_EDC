#pragma once

#include <Arduino.h>
#include <Wire.h>

#include "SeaConfig.h"

namespace sea {

class MotionInput {
 public:
  MotionInput();

  bool begin(TwoWire& bus);
  bool readGravity(float& gx, float& gy);

 private:
  bool readAccel();
  void writeRegister(uint8_t reg, uint8_t value);

  TwoWire* bus_;
  bool online_;
  float ax_;
  float ay_;
  float az_;
};

}  // namespace sea
