/*
 * Trabass IoT - Modul sensor GPS (NEO-6M / TinyGPSPlus)
 * ------------------------------------------------------------------
 * Tanggung jawab:
 *   - Mengonsumsi byte UART GPS secara non-blocking tiap loop().
 *   - Menyediakan status fix dan nilai terakhir (lat/lng/speed/satelit).
 *
 * Modul ini TIDAK menyentuh MQTT. Objek `gps` dan `gpsSerial` tetap
 * dimiliki sketch_nov16a.ino dan diakses lewat extern (pola sama
 * dengan self_test.cpp) agar tidak terjadi duplikasi definisi.
 *
 * Fix dianggap segar jika lokasi valid DAN umurnya <= GPS_FIX_MAX_AGE_MS.
 * Saat GPS kehilangan sinyal, nilai lama tidak dipakai untuk telemetri.
 */

#ifndef TRABASS_GPS_SENSOR_H
#define TRABASS_GPS_SENSOR_H

#include <Arduino.h>

namespace gps_sensor {

// Panggil sekali dari setup() untuk mencatat status awal.
void begin();

// Panggil tiap iterasi loop(). Membaca semua byte yang tersedia.
void update();

// True jika ada fix yang masih segar (lihat GPS_FIX_MAX_AGE_MS).
bool hasFix();

// Nilai terakhir; hanya bermakna saat hasFix() true. Tanpa fix
// getSpeed() mengembalikan 0.
double getLat();
double getLng();
double getSpeed();
int getSats();

// Umur data lokasi terakhir dalam ms, atau 0xFFFFFFFF jika belum ada.
uint32_t fixAgeMs();

// Diagnostik untuk menu serial.
uint32_t charsProcessed();
uint32_t passedChecksum();

} // namespace gps_sensor

#endif // TRABASS_GPS_SENSOR_H
