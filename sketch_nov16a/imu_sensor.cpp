/*
 * Trabass IoT - Implementasi modul sensor IMU
 * ------------------------------------------------------------------
 * Logika klasifikasi:
 *   1. Sampel dianggap "aktif" jika | |a| - 1 g | > threshold ATAU
 *      besar gyro > threshold dps.
 *   2. motion baru true setelah aktivitas bertahan IMU_MOTION_HOLD_MS.
 *   3. motion kembali false setelah IMU_MOTION_IDLE_MS tanpa aktivitas.
 *
 * Tidak ada delay(); i2cRead hanya beberapa milidetik.
 */

#include <Wire.h>

#include "config.h"
#include "imu_sensor.h"

namespace {

using imu_sensor::Sample;

bool gReady = false;
uint8_t gAddr = MPU_ADDR;
uint32_t gLastInitAttemptMs = 0;
uint32_t gInitAttempts = 0;
uint32_t gLastSampleMs = 0;
bool gHasSampled = false;

bool gMotion = false;
uint32_t gActivitySinceMs = 0;
uint32_t gLastActivityMs = 0;

bool gFirstInitFailureLogged = false;

bool initHardware() {
  // Coba alamat utama lalu alamat cadangan.Kept sederhana: dua kandidat
  // cukup untuk GY-521 dengan AD0 mengambang (0x68) atau HIGH (0x69).
  const uint8_t candidates[2] = {(uint8_t)MPU_ADDR, (uint8_t)MPU_ADDR_ALT};

  for (uint8_t i = 0; i < 2; i++) {
    gAddr = candidates[i];
    Wire.beginTransmission(gAddr);
    Wire.write(0x6B); // PWR_MGMT_1
    Wire.write(0x00); // bangunkan sensor
    if (Wire.endTransmission() == 0) return true;
  }

  gAddr = 0;
  return false;
}

bool readRaw(Sample &out) {
  Wire.beginTransmission(gAddr);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(gAddr, (uint8_t)14) != 14) return false;

  int16_t v[7];
  for (int i = 0; i < 7; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    v[i] = (int16_t)((hi << 8) | lo);
  }

  out.ax = v[0] / 16384.0f;
  out.ay = v[1] / 16384.0f;
  out.az = v[2] / 16384.0f;
  out.tempC = v[3] / 340.0f + 36.53f;
  out.gx = v[4] / 131.0f;
  out.gy = v[5] / 131.0f;
  out.gz = v[6] / 131.0f;
  return true;
}

void classify(const Sample &s, uint32_t now) {
  const float accelMag = sqrtf(s.ax * s.ax + s.ay * s.ay + s.az * s.az);
  const float accelDev = fabsf(accelMag - 1.0f);
  const float gyroMag = sqrtf(s.gx * s.gx + s.gy * s.gy + s.gz * s.gz);

  const bool active = (accelDev > IMU_MOTION_ACCEL_THRESHOLD_G) ||
                      (gyroMag > IMU_MOTION_GYRO_THRESHOLD_DPS);

  if (active) {
    gLastActivityMs = now;

    if (!gMotion) {
      if (gActivitySinceMs == 0) gActivitySinceMs = now;
      if ((now - gActivitySinceMs) >= (uint32_t)IMU_MOTION_HOLD_MS) {
        gMotion = true;
        Serial.printf("[IMU] Gerakan terdeteksi (accelDev=%.2f g, gyro=%.0f d/s)\n",
                      accelDev, gyroMag);
      }
    }
    return;
  }

  gActivitySinceMs = 0;
  if (gMotion &&
      (now - gLastActivityMs) >= (uint32_t)IMU_MOTION_IDLE_MS) {
    gMotion = false;
    Serial.println("[IMU] Kembali diam (mpu_motion=false)");
  }
}

} // namespace

namespace imu_sensor {

bool begin() {
  gInitAttempts++;
  gLastInitAttemptMs = millis();
  gReady = initHardware();

  if (gReady) {
    Serial.printf("[IMU] MPU6050 siap di 0x%02X (sample 100 ms, motion bool).\n",
                  gAddr);
    gFirstInitFailureLogged = false;
  } else if (!gFirstInitFailureLogged || (gInitAttempts % 12) == 1) {
    gFirstInitFailureLogged = true;
    Serial.println("[IMU] MPU6050 belum terdeteksi, retry tiap 5 detik.");
  }

  return gReady;
}

void update() {
  const uint32_t now = millis();

  if (!gReady) {
    if ((now - gLastInitAttemptMs) >= (uint32_t)IMU_RETRY_INTERVAL_MS) {
      begin();
    }
    return;
  }

  if (gHasSampled &&
      (now - gLastSampleMs) < (uint32_t)IMU_SAMPLE_INTERVAL_MS) {
    return;
  }

  Sample s;
  if (!readRaw(s)) {
    gReady = false;
    gLastInitAttemptMs = now;
    Serial.println("[IMU] Baca gagal, sensor dianggap tidak siap (akan retry).");
    return;
  }

  gLastSampleMs = now;
  gHasSampled = true;
  classify(s, now);
}

bool healthy() {
  return gReady;
}

uint8_t detectedAddress() {
  return gReady ? gAddr : 0;
}

bool motion() {
  return gMotion;
}

bool readSample(Sample &out) {
  return readRaw(out);
}

uint32_t lastSampleAgeMs() {
  return gHasSampled ? (millis() - gLastSampleMs) : 0xFFFFFFFFUL;
}

} // namespace imu_sensor
