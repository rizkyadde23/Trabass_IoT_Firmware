/*
 * Trabass IoT - Builder & pengirim telemetri
 * ------------------------------------------------------------------
 * Kontrak payload (konteks.md bagian 6), topic bike/telemetry/BIKE_ID:
 *
 *   {"lat":-7.7,"lng":110.4,"speed":1.2,"mpu_motion":false,
 *    "lock_state":true,"gps_fix":true,"satellites":9,
 *    "uptime_ms":123456,"fw_version":"1.0.0-m3"}
 *
 * Tanpa fix: "lat":null,"lng":null,"speed":0.0,"satellites":0,
 * "gps_fix":false. IMU dan lock_state tetap dikirim agar dashboard
 * tetap hidup saat demo indoor.
 *
 * Modul ini hanya menyusun payload dan memanggil mqtt_link; tidak
 * membaca sensor secara langsung selain lewat API modul sensor.
 */

#ifndef TRABASS_TELEMETRY_H
#define TRABASS_TELEMETRY_H

#include <Arduino.h>

namespace telemetry {

// Panggil sekali dari setup().
void begin();

// Panggil tiap iterasi loop(). Mengirim saat interval habis atau
// segera setelah MQTT reconnect (publish on connect).
void loop();

// Kirim sekarang (menu 'm'). Bypass interval.
bool publishNow();

// Cetak payload yang akan dikirim ke Serial (menu 't').
void printPayload();

} // namespace telemetry

#endif // TRABASS_TELEMETRY_H
