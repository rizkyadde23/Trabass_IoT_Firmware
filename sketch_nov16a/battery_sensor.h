/*
 * Trabass IoT - Modul baterai (STUB untuk M3)
 * ------------------------------------------------------------------
 * Hardware divider/INA219 belum terpasang. Selama
 * ENABLE_BATTERY_SENSOR = 0, read() selalu false dan telemetri tidak
 * mengirim field battery. Dashboard menampilkan "N/A".
 *
 * Saat hardware siap: set ENABLE_BATTERY_SENSOR 1 dan sesuaikan
 * BATTERY_ADC_PIN / BATTERY_DIVIDER_RATIO di config.h.
 */

#ifndef TRABASS_BATTERY_SENSOR_H
#define TRABASS_BATTERY_SENSOR_H

#include <Arduino.h>

namespace battery_sensor {

// Panggil sekali dari setup().
void begin();

// True jika pembacaan valid; percent diisi 0..100.
// Saat ENABLE_BATTERY_SENSOR 0 selalu false (percent tidak diubah).
bool read(float &percent);

bool enabled();

} // namespace battery_sensor

#endif // TRABASS_BATTERY_SENSOR_H
