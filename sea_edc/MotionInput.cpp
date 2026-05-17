#include "MotionInput.h"

#include <cmath>

namespace sea {

namespace {
constexpr uint8_t kAddress = 0x68;
constexpr uint8_t kPowerMgmt1 = 0x6B;
constexpr uint8_t kAccelConfig = 0x1C;
constexpr uint8_t kGyroConfig = 0x1B;
constexpr uint8_t kFilterConfig = 0x1A;
constexpr uint8_t kAccelData = 0x3B;
constexpr float kAccelScale = 16384.0f;
}  // namespace

MotionInput::MotionInput() : bus_(nullptr), online_(false), ax_(0.0f), ay_(0.0f), az_(1.0f) {}

bool MotionInput::begin(TwoWire& bus) {
  bus_ = &bus;
  writeRegister(kPowerMgmt1, 0x00);
  writeRegister(kAccelConfig, 0x00);
  writeRegister(kGyroConfig, 0x00);
  writeRegister(kFilterConfig, 0x03);
  delay(50);
  online_ = readAccel();
  return online_;
}

void MotionInput::writeRegister(uint8_t reg, uint8_t value) {
  if (bus_ == nullptr) return;
  bus_->beginTransmission(kAddress);
  bus_->write(reg);
  bus_->write(value);
  bus_->endTransmission();
}

bool MotionInput::readAccel() {
  if (bus_ == nullptr) return false;

  bus_->beginTransmission(kAddress);
  bus_->write(kAccelData);
  if (bus_->endTransmission(false) != 0) return false;

  if (bus_->requestFrom(kAddress, static_cast<uint8_t>(14), static_cast<uint8_t>(true)) != 14) {
    return false;
  }

  int16_t rawX = static_cast<int16_t>((bus_->read() << 8) | bus_->read());
  int16_t rawY = static_cast<int16_t>((bus_->read() << 8) | bus_->read());
  int16_t rawZ = static_cast<int16_t>((bus_->read() << 8) | bus_->read());
  for (int i = 0; i < 8; ++i) bus_->read();

  ax_ = static_cast<float>(rawX) / kAccelScale;
  ay_ = static_cast<float>(rawY) / kAccelScale;
  az_ = static_cast<float>(rawZ) / kAccelScale;
  online_ = true;
  return true;
}

bool MotionInput::readGravity(float& gx, float& gy) {
  readAccel();

  if (!online_) {
    gx = 0.0f;
    gy = kGravityScale;
    return false;
  }

  float screenX = -ax_;
  float screenY = ay_;
  if (std::sqrt(screenX * screenX + screenY * screenY) < 0.08f) {
    screenX = 0.0f;
    screenY = 1.0f;
  }

  gx = screenX * kGravityScale;
  gy = screenY * kGravityScale;
  return true;
}

}  // namespace sea
