/*
 * Trabass IoT - Modul sensor IMU (MPU6050)
 * ------------------------------------------------------------------
 * Tanggung jawab:
 *   - Sampling non-blocking tiap IMU_SAMPLE_INTERVAL_MS.
 *   - Klasifikasi gerakan (mpu_motion) memakai threshold + hold/idle
 *     dari config sehingga noise singkat tidak memicu alarm.
 *
 * Output ke backend hanya boolean `motion()`. Nilai mentah disediakan
 * untuk menu tes serial, bukan untuk telemetri.
 */

#ifndef TRABASS_IMU_SENSOR_H
#define TRABASS_IMU_SENSOR_H

#include <Arduino.h>

namespace imu_sensor {

struct Sample {
  float ax, ay, az;  // g
  float gx, gy, gz;  // derajat/detik
  float tempC;
};

// Coba inisialisasi MPU6050 sekarang. Return true jika berhasil.
// Dipanggil setup(); bisa dipanggil ulang dari menu tes.
bool begin();

// Panggil tiap iterasi loop(). Melakukan sampling sesuai interval dan
// retry inisialisasi bila sensor belum siap.
void update();

// True jika sensor terpasang dan siap dibaca.
bool healthy();

// Alamat I2C yang dipakai setelah auto-probe (0 jika belum terdeteksi).
uint8_t detectedAddress();

// True jika gerakan terklasifikasi (sudah melewati hold, belum idle).
bool motion();

// Baca sampel mentah sekarang (blocking singkat ~1 ms). Dipakai tes.
bool readSample(Sample &out);

// Diagnostik.
uint32_t lastSampleAgeMs();

} // namespace imu_sensor

#endif // TRABASS_IMU_SENSOR_H
