/*
 * Trabass IoT - Implementasi builder & pengirim telemetri
 * ------------------------------------------------------------------
 * Payload disusun dengan snprintf ke buffer statis TELEMETRY_JSON_BUFFER.
 * Publish tetap QoS 0 (batasan PubSubClient v2.8).
 */

#include "config.h"
#include "telemetry.h"
#include "gps_sensor.h"
#include "imu_sensor.h"
#include "mqtt_link.h"

namespace {

bool gHasAttempted = false;
uint32_t gLastAttemptMs = 0;

bool gFirstPublish = true;

// lock_state diturunkan dari pin relay agar selalu sinkron dengan
// kondisi fisik, termasuk setelah menu tes '3' di M2.
bool locked() {
  return digitalRead(RELAY_PIN) == RELAY_OFF;
}

bool buildPayload(char *out, size_t outSize) {
  if (out == nullptr || outSize == 0) return false;

  const bool fix = gps_sensor::hasFix();
  const char *motion = imu_sensor::motion() ? "true" : "false";
  const char *lock = locked() ? "true" : "false";
  const unsigned long uptime = (unsigned long)millis();

  int length;
  if (fix) {
    length = snprintf(
        out, outSize,
        "{\"lat\":%.6f,\"lng\":%.6f,\"speed\":%.2f,"
        "\"mpu_motion\":%s,\"lock_state\":%s,\"gps_fix\":true,"
        "\"transport\":\"%s\",\"link_state\":\"%s\","
        "\"satellites\":%d,\"uptime_ms\":%lu,\"fw_version\":\"%s\"}",
        gps_sensor::getLat(),
        gps_sensor::getLng(),
        gps_sensor::getSpeed(),
        motion,
        lock,
        mqtt_link::transportLabel(),
        mqtt_link::stateLabel(),
        gps_sensor::getSats(),
        uptime,
        FW_VERSION);
  } else {
    length = snprintf(
        out, outSize,
        "{\"lat\":null,\"lng\":null,\"speed\":0.0,"
        "\"mpu_motion\":%s,\"lock_state\":%s,\"gps_fix\":false,"
        "\"transport\":\"%s\",\"link_state\":\"%s\","
        "\"satellites\":0,\"uptime_ms\":%lu,\"fw_version\":\"%s\"}",
        motion,
        lock,
        mqtt_link::transportLabel(),
        mqtt_link::stateLabel(),
        uptime,
        FW_VERSION);
  }

  if (length <= 0 || (size_t)length >= outSize) {
    Serial.println("[TELEMETRY] Payload terpotong, tidak dikirim.");
    return false;
  }
  return true;
}

void logPayload(const char *payload, size_t length) {
#if ENABLE_TELEMETRY_PAYLOAD_LOG
  Serial.printf("[TELEMETRY] Kirim %u B: %s\n", (unsigned)length, payload);
#else
  (void)payload;
  (void)length;
#endif
}

bool send(bool force) {
  char payload[TELEMETRY_JSON_BUFFER];
  if (!buildPayload(payload, sizeof(payload))) return false;

  if (!mqtt_link::publishTelemetry(payload, force)) {
    return false;
  }

  gHasAttempted = true;
  gLastAttemptMs = millis();
  if (gFirstPublish) {
    gFirstPublish = false;
    Serial.printf("[TELEMETRY] Publish pertama OK (%u B).\n",
                  (unsigned)strlen(payload));
  }
  logPayload(payload, strlen(payload));
  return true;
}

} // namespace

namespace telemetry {

void begin() {
  gHasAttempted = false;
  gFirstPublish = true;
  Serial.printf("[TELEMETRY] Interval %d ms, buffer %d B, versi %s.\n",
                TELEMETRY_INTERVAL_MS,
                TELEMETRY_JSON_BUFFER,
                FW_VERSION);
}

void loop() {
  // Publish segera setelah MQTT tersambung kembali.
  if (mqtt_link::takeConnectEvent()) {
    send(true);
    return;
  }

  const uint32_t now = millis();
  if (gHasAttempted &&
      (now - gLastAttemptMs) < (uint32_t)TELEMETRY_INTERVAL_MS) {
    return;
  }

  send(false);
}

bool publishNow() {
  return send(true);
}

void printPayload() {
  char payload[TELEMETRY_JSON_BUFFER];
  if (!buildPayload(payload, sizeof(payload))) return;
  Serial.printf("[TELEMETRY] Payload (%u B): %s\n",
                (unsigned)strlen(payload),
                payload);
}

} // namespace telemetry
