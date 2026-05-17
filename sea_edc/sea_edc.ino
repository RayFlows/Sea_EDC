#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <array>
#include <atomic>

#include "MotionInput.h"
#include "SeaConfig.h"
#include "SurfaceRenderer.h"
#include "TankGrid.h"
#include "WaterModel.h"

#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

#define TFT_SCLK 18
#define TFT_MOSI 23
#define TFT_CS 5
#define TFT_DC 27
#define TFT_RST 26
#define TFT_SPI_HZ 27000000
#define TFT_SPI_MODE SPI_MODE0
#define TFT_INVERT true

Adafruit_ST7789 display(TFT_CS, TFT_DC, TFT_RST);

sea::WaterModel ocean;
sea::SurfaceRenderer renderer;
sea::MotionInput motion;

SemaphoreHandle_t densityMutex = nullptr;
std::atomic<float> gravityX{0.0f};
std::atomic<float> gravityY{sea::kGravityScale};
std::atomic<float> renderFps{0.0f};

uint32_t fpsLastMs = 0;
uint32_t fpsFrames = 0;

void pulseDisplayReset() {
  pinMode(TFT_CS, OUTPUT);
  digitalWrite(TFT_CS, HIGH);
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(20);
  digitalWrite(TFT_RST, LOW);
  delay(80);
  digitalWrite(TFT_RST, HIGH);
  delay(150);
}

void physicsTask(void*) {
  uint32_t lastUs = micros();
  float accumulator = 0.0f;

  for (;;) {
    uint32_t nowUs = micros();
    float dt = (nowUs - lastUs) * 1e-6f;
    lastUs = nowUs;
    if (dt > sea::kMaxElapsedSeconds) dt = sea::kMaxElapsedSeconds;
    accumulator += dt;

    bool stepped = false;
    while (accumulator >= sea::kStepSeconds) {
      float gx = gravityX.load(std::memory_order_relaxed);
      float gy = gravityY.load(std::memory_order_relaxed);
      ocean.advance(sea::kStepSeconds, gx, gy);
      accumulator -= sea::kStepSeconds;
      stepped = true;
    }

    if (stepped) {
      xSemaphoreTake(densityMutex, portMAX_DELAY);
      ocean.rebuildRenderMass();
      xSemaphoreGive(densityMutex);
    }

    vTaskDelay(1);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("[boot] Sea EDC");

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(400000);
  bool imuOk = motion.begin(Wire);
  Serial.println(imuOk ? "[imu] MPU6050 online" : "[imu] MPU6050 not found, using fallback gravity");

  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  pulseDisplayReset();
  display.init(240, 240, TFT_SPI_MODE);
  display.setSPISpeed(TFT_SPI_HZ);
  display.setRotation(1);
  display.invertDisplay(TFT_INVERT);
  display.fillScreen(sea::kEmptyColor);

  renderer.begin(display);
  ocean.reset(sea::kStartDroplets);
  ocean.rebuildRenderMass();

  densityMutex = xSemaphoreCreateMutex();
  xTaskCreatePinnedToCore(physicsTask, "physics", sea::kPhysicsStack,
                          nullptr, sea::kPhysicsPriority, nullptr,
                          sea::kPhysicsCore);

  fpsLastMs = millis();
}

void loop() {
  float gx = 0.0f;
  float gy = sea::kGravityScale;
  if (motion.readGravity(gx, gy)) {
    gravityX.store(gx, std::memory_order_relaxed);
    gravityY.store(gy, std::memory_order_relaxed);
  }

  static std::array<float, sea::TankGrid::kCellCount> massSnapshot;
  int particleCount = 0;

  xSemaphoreTake(densityMutex, portMAX_DELAY);
  massSnapshot = ocean.renderMass();
  particleCount = ocean.dropletCount();
  xSemaphoreGive(densityMutex);

  renderer.draw(massSnapshot.data(), particleCount, renderFps.load(std::memory_order_relaxed));

  fpsFrames++;
  uint32_t nowMs = millis();
  if (nowMs - fpsLastMs >= 1000) {
    float fps = fpsFrames * 1000.0f / (float)(nowMs - fpsLastMs);
    renderFps.store(fps, std::memory_order_relaxed);
    Serial.print("[fps] ");
    Serial.print(fps, 1);
    Serial.print(" particles=");
    Serial.print(particleCount);
    Serial.print(" gx=");
    Serial.print(gx, 2);
    Serial.print(" gy=");
    Serial.println(gy, 2);
    fpsFrames = 0;
    fpsLastMs = nowMs;
  }
}
