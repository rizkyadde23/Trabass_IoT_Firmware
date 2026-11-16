/*
 * Trabass IoT - Implementasi modul baterai (stub + jalur ADC siap pakai)
 * ------------------------------------------------------------------
 * Perhitungan saat diaktifkan:
 *   Vpin  = raw / BATTERY_ADC_MAX * BATTERY_ADC_REF_V
 *   Vbatt = Vpin * BATTERY_DIVIDER_RATIO
 *   persen = (Vbatt - BATTERY_MIN_V) / (BATTERY_MAX_V - BATTERY_MIN_V)
 *
 * Tidak ada pembacaan saat ENABLE_BATTERY_SENSOR 0 agar pin 34 tidak
 * mengganggu fungsi lain dan tidak ada klaim data palsu.
 */

#include "config.h"
#include "battery_sensor.h"

namespace battery_sensor {

void begin() {
#if ENABLE_BATTERY_SENSOR
  pinMode(BATTERY_ADC_PIN, INPUT);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
  Serial.printf("[BAT] Sensor aktif di GPIO%d (divider %.2fx).\n",
                BATTERY_ADC_PIN,
                (double)BATTERY_DIVIDER_RATIO);
#else
  Serial.println("[BAT] Sensor nonaktif (stub M3), field battery tidak dikirim.");
#endif
}

bool read(float &percent) {
#if !ENABLE_BATTERY_SENSOR
  (void)percent;
  return false;
#else
  const int raw = analogRead(BATTERY_ADC_PIN);
  const float vpin = ((float)raw / BATTERY_ADC_MAX) * BATTERY_ADC_REF_V;
  const float vbatt = vpin * BATTERY_DIVIDER_RATIO;

  float p = (vbatt - BATTERY_MIN_V) /
            (BATTERY_MAX_V - BATTERY_MIN_V) * 100.0f;
  if (p < 0.0f) p = 0.0f;
  if (p > 100.0f) p = 100.0f;

  percent = p;
  return true;
#endif
}

bool enabled() {
#if ENABLE_BATTERY_SENSOR
  return true;
#else
  return false;
#endif
}

} // namespace battery_sensor
