/*
 * Trabass IoT - Implementasi modul sensor GPS
 * ------------------------------------------------------------------
 * Tidak ada delay(). Semua byte yang masuk langsung di-encode ke
 * TinyGPSPlus sehingga loop() tetap responsif terhadap MQTT.
 */

#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

#include "config.h"
#include "gps_sensor.h"

// Objek hardware dimiliki oleh sketch_nov16a.ino.
extern TinyGPSPlus gps;
extern HardwareSerial gpsSerial;

namespace {

bool gWasFix = false;
bool gFirstReport = true;

uint32_t safeAgeMs() {
  return gps.location.isValid() ? gps.location.age() : 0xFFFFFFFFUL;
}

void logTransition(bool fix) {
  if (!gFirstReport && fix == gWasFix) return;
  gFirstReport = false;
  gWasFix = fix;

  if (fix) {
    Serial.printf("[GPS] FIX lat=%.6f lng=%.6f sat=%d speed=%.1f km/jam\n",
                  gps.location.lat(),
                  gps.location.lng(),
                  (int)gps.satellites.value(),
                  gps.speed.kmph());
  } else if (gps.charsProcessed() < 10) {
    Serial.println("[GPS] Menunggu data UART (cek TX/RX/VCC).");
  } else {
    Serial.printf("[GPS] NO-FIX (data terakhir %lu ms lalu, sat=%d)\n",
                  (unsigned long)safeAgeMs(),
                  gps.satellites.isValid() ? (int)gps.satellites.value() : 0);
  }
}

} // namespace

namespace gps_sensor {

void begin() {
  gFirstReport = true;
  gWasFix = false;
  Serial.println("[GPS] Modul siap (UART sudah dibuka oleh setup()).");
}

void update() {
  while (gpsSerial.available()) {
    gps.encode(gpsSerial.read());
  }
  logTransition(hasFix());
}

bool hasFix() {
  return gps.location.isValid() &&
         gps.location.age() <= (unsigned long)GPS_FIX_MAX_AGE_MS;
}

double getLat() {
  return hasFix() ? gps.location.lat() : 0.0;
}

double getLng() {
  return hasFix() ? gps.location.lng() : 0.0;
}

double getSpeed() {
  return hasFix() && gps.speed.isValid() ? gps.speed.kmph() : 0.0;
}

int getSats() {
  return (hasFix() && gps.satellites.isValid())
             ? (int)gps.satellites.value()
             : 0;
}

uint32_t fixAgeMs() {
  return safeAgeMs();
}

uint32_t charsProcessed() {
  return gps.charsProcessed();
}

uint32_t passedChecksum() {
  return gps.passedChecksum();
}

} // namespace gps_sensor
