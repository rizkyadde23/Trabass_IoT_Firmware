/*
 * Trabass IoT - Aktuator: relay, buzzer, dan LCD
 * ------------------------------------------------------------------
 * Modul ini adalah SATU-SATUNYA pemilik pin relay/buzzer dan objek LCD.
 * Semua perubahan state bersifat absolut (bukan toggle) dan non-blocking.
 *
 * Aturannya:
 *   - relay aktif LOW: LOCK = RELAY_OFF (HIGH), UNLOCK = RELAY_ON (LOW)
 *   - alarm otomatis mati setelah ALARM_DURATION_MS (atau durasi command)
 *   - WARNING_BEEP memakai 3 pulse non-blocking (tidak mengubah kunci)
 *   - pesan LCD sementara otomatis kembali ke status utama
 */

#ifndef TRABASS_OUTPUTS_H
#define TRABASS_OUTPUTS_H

#include <Arduino.h>

namespace outputs {

// Inisialisasi pin, LCD, dan state aman (terkunci, alarm mati).
void begin();

// Panggil tiap iterasi loop(). Menangani auto-off alarm, beep, dan LCD.
void loop();

// Set absolut. Tidak ada toggle di sini (idempoten).
void setLock(bool locked);
bool isLocked();

// Alarm/sirene. durasi 0 memakai ALARM_DURATION_MS.
// Jika alarm sudah aktif, durasi lama dipertahankan (tidak diperpanjang).
void alarmOn(uint32_t durationMs = 0);
void alarmOff();
bool isAlarmActive();
uint32_t alarmRemainingMs();

// 3 pulse pendek non-blocking. Tidak mengubah state kunci/alarm.
void warningBeep();
bool isBeeping();

// Hitung mundur sewa untuk LCD. Deadline hanya disimpan di RAM dan
// dihapus saat LOCK/boot; bukan sumber kebenaran (backend tetap DB).
void setRentalRemainingMinutes(uint32_t minutes);
void extendRentalMinutes(uint32_t minutes);
void clearRentalTimer();
uint32_t rentalRemainingMs();

// Tampilkan pesan sementara di LCD (maksimal 16 karakter per baris).
void showMessage(const char *line1, const char *line2,
                 uint32_t durationMs = 0);

// Paksa LCD menggambar ulang status utama.
void renderStatus();

const char *stateLabel();

} // namespace outputs

#endif // TRABASS_OUTPUTS_H
