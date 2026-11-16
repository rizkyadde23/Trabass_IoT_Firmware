/*
 * Trabass IoT - Tes komponen baseline
 * ------------------------------------------------------------------
 * Modul ini memindahkan fungsi tes dari sketch_nov16a.ino tanpa
 * mengubah perilakunya. Menu serial tetap memanggil fungsi-fungsi ini
 * hanya jika ENABLE_SERIAL_TEST_MENU = 1 dan hanya saat user mengetik
 * perintah, bukan otomatis saat boot.
 *
 * Objek hardware (lcd, gps, gpsSerial, sim) dimiliki oleh .ino dan
 * diakses lewat extern agar tidak terjadi duplikasi definisi.
 */

#ifndef TRABASS_SELF_TEST_H
#define TRABASS_SELF_TEST_H

#include <Arduino.h>

namespace self_test {

bool tesLCD();
void tesBuzzer();
void toggleRelay();
bool tesMPU();
bool tesGPS();
bool tesSIM();
void kirimSMS();
void i2cScan();
void tesSemua();

// true = relay sedang ON (solenoid aktif / terbuka).
bool relayState();

} // namespace self_test

#endif // TRABASS_SELF_TEST_H
