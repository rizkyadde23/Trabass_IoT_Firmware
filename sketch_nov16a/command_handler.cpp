/*
 * Trabass IoT - Implementasi command downlink handler
 * ------------------------------------------------------------------
 * Payload contoh dari backend:
 *   {"action":"LOCK_AND_ALARM","reason":"RENTAL_TIME_EXPIRED",
 *    "message":"Waktu sewa habis.","timestamp":"2026-11-19T...Z"}
 *
 * Setelah aksi dijalankan, telemetri dipublish paksa (force=true) agar
 * dashboard langsung melihat perubahan lock_state.
 */

#include <ArduinoJson.h>

#include "config.h"
#include "command_handler.h"
#include "mqtt_link.h"
#include "outputs.h"
#include "telemetry.h"

namespace {

constexpr size_t kCommandBufferSize = 192;
constexpr size_t kKeySize = 96;
constexpr size_t kDedupeSlots = 4;

struct DedupeEntry {
  char key[kKeySize];
  uint32_t ms;
};

DedupeEntry gDedupe[kDedupeSlots];
size_t gDedupeIndex = 0;

bool sudahDiproses(const char *key, uint32_t now) {
  for (size_t i = 0; i < kDedupeSlots; i++) {
    if (gDedupe[i].key[0] == '\0') continue;
    if (strcmp(gDedupe[i].key, key) != 0) continue;

    if ((now - gDedupe[i].ms) < (uint32_t)COMMAND_DEDUPE_WINDOW_MS) {
      Serial.printf("[CMD] Duplikat dalam %d ms, diabaikan: %s\n",
                    COMMAND_DEDUPE_WINDOW_MS, key);
      return true;
    }
    break;
  }

  strncpy(gDedupe[gDedupeIndex].key, key, kKeySize - 1);
  gDedupe[gDedupeIndex].key[kKeySize - 1] = '\0';
  gDedupe[gDedupeIndex].ms = now;
  gDedupeIndex = (gDedupeIndex + 1) % kDedupeSlots;
  return false;
}

uint32_t durasiAlarm(JsonVariantConst value) {
  unsigned long ms = value | 0UL;
  if (ms == 0) return 0;
  return (uint32_t)ms;
}

void ringkas(const char *teks, char *out, size_t outSize) {
  if (outSize == 0) return;
  strncpy(out, teks != nullptr ? teks : "", outSize - 1);
  out[outSize - 1] = '\0';
}

} // namespace

namespace command_handler {

void begin() {
  for (size_t i = 0; i < kDedupeSlots; i++) {
    gDedupe[i].key[0] = '\0';
    gDedupe[i].ms = 0;
  }
  gDedupeIndex = 0;
  Serial.printf("[CMD] Handler siap. Dedupe window=%d ms.\n",
                COMMAND_DEDUPE_WINDOW_MS);
}

bool loop() {
  char payload[kCommandBufferSize];
  if (!mqtt_link::takeCommand(payload, sizeof(payload))) return false;

  Serial.printf("[CMD] Diterima %u B: %s\n",
                (unsigned)strlen(payload), payload);

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[CMD] JSON tidak valid (%s), diabaikan.\n", err.c_str());
    return true;
  }

  const char *action = doc["action"] | "";
  if (action[0] == '\0') {
    Serial.println("[CMD] Field 'action' kosong, diabaikan.");
    return true;
  }

  const char *timestamp = doc["timestamp"] | "";
  char key[kKeySize];
  snprintf(key, sizeof(key), "%s|%s", action, timestamp);

  const uint32_t now = millis();
  if (sudahDiproses(key, now)) return true;

  char pesan[33];
  ringkas(doc["message"] | "", pesan, sizeof(pesan));

  bool dieksekusi = true;

  if (strcmp(action, "LOCK") == 0) {
    outputs::setLock(true);
    outputs::showMessage("TERKUNCI", "Perintah diterima");

  } else if (strcmp(action, "UNLOCK") == 0) {
    outputs::alarmOff();
    outputs::setLock(false);
    outputs::showMessage("TERBUKA", "Perintah diterima");

  } else if (strcmp(action, "ALARM_ON") == 0) {
    if (outputs::isAlarmActive()) {
      Serial.println("[CMD] ALARM_ON diterima, alarm sudah aktif.");
      dieksekusi = false;
    } else {
      outputs::alarmOn(durasiAlarm(doc["duration_ms"]));
    }

  } else if (strcmp(action, "ALARM_OFF") == 0) {
    outputs::alarmOff();

  } else if (strcmp(action, "LOCK_AND_ALARM") == 0) {
    outputs::setLock(true);
    if (!outputs::isAlarmActive()) {
      outputs::alarmOn(durasiAlarm(doc["duration_ms"]));
    }

  } else if (strcmp(action, "WARNING_BEEP") == 0) {
    outputs::warningBeep();
    outputs::showMessage("PERINGATAN",
                         pesan[0] != '\0' ? pesan : "Periksa sepeda");

  } else if (strcmp(action, "EXTEND_TIME") == 0) {
    int added = doc["added_minutes"] | 0;
    char detail[17];
    snprintf(detail, sizeof(detail), "Ditambah %d mnt", added);
    outputs::showMessage("PERPANJANG", detail);
    Serial.printf("[CMD] EXTEND_TIME added_minutes=%d end_time=%s\n",
                  added, (const char *)(doc["end_time"] | "-"));

  } else if (strcmp(action, "SEND_SMS") == 0) {
    // M4 sengaja tidak menyentuh SIM800L. Eksekusi nyata di M5.
    Serial.println("[CMD] SEND_SMS -> PENDING_M5 (SMS tidak dikirim).");

  } else {
    Serial.printf("[CMD] Aksi '%s' tidak dikenal, diabaikan.\n", action);
    dieksekusi = false;
  }

  if (dieksekusi) {
    Serial.printf("[CMD] Eksekusi '%s' selesai. state=%s alarm=%s\n",
                  action,
                  outputs::stateLabel(),
                  outputs::isAlarmActive() ? "ON" : "OFF");
    telemetry::publishNow();
  }

  return true;
}

} // namespace command_handler
