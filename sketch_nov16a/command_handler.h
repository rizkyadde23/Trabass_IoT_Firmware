/*
 * Trabass IoT - Command downlink handler
 * ------------------------------------------------------------------
 * Mengambil payload dari antrean mqtt_link, mem-parsing JSON, lalu
 * mendispatch ke modul outputs. Tidak ada delay() di jalur loop().
 *
 * Aksi yang dikenal:
 *   LOCK, UNLOCK, ALARM_ON, ALARM_OFF, LOCK_AND_ALARM,
 *   WARNING_BEEP, EXTEND_TIME, SEND_SMS (log PENDING_M5).
 */

#ifndef TRABASS_COMMAND_HANDLER_H
#define TRABASS_COMMAND_HANDLER_H

#include <Arduino.h>

namespace command_handler {

void begin();

// Ambil maksimal satu command per iterasi agar loop tetap responsif.
// Return true jika ada command yang diproses (berhasil atau ditolak).
bool loop();

} // namespace command_handler

#endif // TRABASS_COMMAND_HANDLER_H
