/*
 * Trabass IoT - Implementasi aktuator (relay, buzzer, LCD)
 * ------------------------------------------------------------------
 * Tidak ada delay() di jalur loop(). Pembangkit nada ~2 kHz memakai
 * microseconds() dan toggle pin; LCD hanya ditulis saat berubah atau
 * untuk menghitung mundur alarm (1 detik).
 */

#include <LiquidCrystal_I2C.h>

#include "config.h"
#include "outputs.h"

// Objek LCD dimiliki oleh sketch_nov16a.ino (sama seperti self_test.cpp).
extern LiquidCrystal_I2C lcd;

namespace {

enum class LcdMode : uint8_t {
  Kosong = 0,
  Terkunci,
  Terbuka,
  Alarm,
  Pesan,
};

enum class BeepPhase : uint8_t {
  On = 0,
  Off,
};

bool gLocked = true;
bool gAlarmActive = false;
uint32_t gAlarmDurationMs = 0;
uint32_t gAlarmUntilMs = 0;

bool gBeepActive = false;
BeepPhase gBeepPhase = BeepPhase::On;
uint32_t gBeepCount = 0;
uint32_t gBeepPhaseEndMs = 0;
uint32_t gNextToggleUs = 0;
bool gBuzzerLevel = false;

uint32_t gMessageUntilMs = 0;
char gMessageLine1[17] = {0};
char gMessageLine2[17] = {0};

// Deadline hitung mundur sewa (0 = tidak ada sewa aktif). RAM-only:
// sengaja tidak disimpan ke NVS agar reboot tetap fail-safe terkunci.
uint32_t gRentalDeadlineMs = 0;

LcdMode gLcdMode = LcdMode::Kosong;
bool gLcdDirty = true;
uint32_t gLcdNextRefreshMs = 0;

void tulisBuzzer(bool high) {
  if (gBuzzerLevel == high) return;
  gBuzzerLevel = high;
  digitalWrite(BUZZER_PIN, high ? HIGH : LOW);
}

// Toggle ~2 kHz. Dipanggil tiap loop(); tidak memblokir.
void perbaruiNada() {
  const bool butuhNada = gAlarmActive ||
                         (gBeepActive && gBeepPhase == BeepPhase::On);
  if (!butuhNada) {
    tulisBuzzer(false);
    return;
  }

  if ((int32_t)(micros() - gNextToggleUs) < 0) return;
  tulisBuzzer(!gBuzzerLevel);
  gNextToggleUs += 250; // 250 us per level -> periode 500 us -> 2 kHz.
}

void perbaruiBeep(uint32_t now) {
  if (!gBeepActive) return;
  if ((int32_t)(now - gBeepPhaseEndMs) < 0) return;

  if (gBeepPhase == BeepPhase::On) {
    gBeepPhase = BeepPhase::Off;
    gBeepPhaseEndMs = now + WARNING_BEEP_OFF_MS;
    return;
  }

  gBeepCount++;
  if (gBeepCount >= WARNING_BEEP_COUNT) {
    gBeepActive = false;
    gBeepPhase = BeepPhase::On;
    tulisBuzzer(false);
    Serial.printf("[OUTPUT] Warning beep selesai (%d pulse).\n",
                  WARNING_BEEP_COUNT);
    return;
  }

  gBeepPhase = BeepPhase::On;
  gBeepPhaseEndMs = now + WARNING_BEEP_ON_MS;
}

LcdMode hitungMode() {
  if (gMessageUntilMs != 0) return LcdMode::Pesan;
  if (gAlarmActive) return LcdMode::Alarm;
  return gLocked ? LcdMode::Terkunci : LcdMode::Terbuka;
}

uint32_t sisaSewaMs() {
  if (gRentalDeadlineMs == 0) return 0;
  int32_t sisa = (int32_t)(gRentalDeadlineMs - millis());
  return sisa > 0 ? (uint32_t)sisa : 0;
}

// Baris kedua LCD saat sewa aktif. Format sengaja dipadatkan agar
// status "AMAN" dan sisa waktu tetap muat dalam 16 karakter.
void formatSisaSewa(char *out, size_t outSize) {
  if (outSize == 0) return;

  if (gRentalDeadlineMs == 0) {
    snprintf(out, outSize, "Status: AMAN");
    return;
  }

  const uint32_t totalDetik = sisaSewaMs() / 1000;
  if (totalDetik >= 6000) { // >= 100 menit: pakai jam agar tidak overflow
    uint32_t jam = totalDetik / 3600;
    uint32_t menit = (totalDetik % 3600) / 60;
    if (jam > 99) {
      jam = 99;
      menit = 59;
    }
    snprintf(out, outSize, "AMAN Sisa:%02uj%02um",
             (unsigned)jam, (unsigned)menit);
    return;
  }

  const uint32_t menit = totalDetik / 60;
  const uint32_t detik = totalDetik % 60;
  snprintf(out, outSize, "AMAN Sisa:%02um%02us",
           (unsigned)menit, (unsigned)detik);
}

void tulisBaris(uint8_t baris, const char *teks) {
  char buffer[17] = {0};
  strncpy(buffer, teks, 16);
  buffer[16] = '\0';
  lcd.setCursor(0, baris);
  for (uint8_t i = 0; i < 16; i++) {
    lcd.write(buffer[i] == '\0' ? ' ' : buffer[i]);
  }
}

void gambarLcd(uint32_t now) {
  const LcdMode mode = hitungMode();

  if (mode != gLcdMode) {
    lcd.clear();
    gLcdMode = mode;
    gLcdDirty = true;
  }

  // Refresh rutin diperlukan alarm dan countdown sewa. Tanpa ini, guard
  // menolak refresh Terbuka sehingga angka hanya terbaca sekali.
  const bool butuhRefreshRutin =
      mode == LcdMode::Alarm ||
      (mode == LcdMode::Terbuka && gRentalDeadlineMs != 0);

  if (!gLcdDirty && !butuhRefreshRutin) {
    gLcdNextRefreshMs = now + LCD_STATUS_REFRESH_MS;
    return;
  }

  switch (mode) {
    case LcdMode::Terkunci:
      tulisBaris(0, "SEPEDA TERKUNCI");
      tulisBaris(1, "Status: AMAN");
      break;
    case LcdMode::Terbuka: {
      char sisa[17];
      formatSisaSewa(sisa, sizeof(sisa));
      tulisBaris(0, "SEPEDA TERBUKA");
      tulisBaris(1, sisa);
      break;
    }
    case LcdMode::Alarm: {
      char sisa[17];
      uint32_t remaining = outputs::alarmRemainingMs() / 1000;
      snprintf(sisa, sizeof(sisa), "Sisa alarm: %lu d",
               (unsigned long)remaining);
      tulisBaris(0, "ALARM BERBUNYI!");
      tulisBaris(1, sisa);
      break;
    }
    case LcdMode::Pesan:
      tulisBaris(0, gMessageLine1);
      tulisBaris(1, gMessageLine2);
      break;
    default:
      break;
  }

  gLcdDirty = false;
  gLcdNextRefreshMs = now + LCD_STATUS_REFRESH_MS;
}

} // namespace

namespace outputs {

void begin() {
  gLocked = true;
  gAlarmActive = false;
  gAlarmDurationMs = 0;
  gAlarmUntilMs = 0;
  gBeepActive = false;
  gBeepCount = 0;
  gBuzzerLevel = false;
  gMessageUntilMs = 0;
  gMessageLine1[0] = '\0';
  gMessageLine2[0] = '\0';
  gRentalDeadlineMs = 0;
  gLcdMode = LcdMode::Kosong;
  gLcdDirty = true;

  // Tulis level dulu sebelum pinMode untuk meminimalkan jendela aktif-LOW
  // saat pin mulai jadi output. GPIO4 adalah strapping pin, jadi ini hanya
  // mengurangi (tidak menghapus) risiko klik sesaat saat boot.
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  lcd.init();
  lcd.backlight();
  gambarLcd(millis());

  Serial.printf("[OUTPUT] Init OK. Relay=OFF (terkunci), buzzer=LOW, LCD=0x%02X\n",
                LCD_ADDR);
}

void loop() {
  const uint32_t now = millis();

  if (gAlarmActive && (int32_t)(now - gAlarmUntilMs) >= 0) {
    Serial.printf("[ALARM] Auto-off setelah %lu ms.\n",
                  (unsigned long)gAlarmDurationMs);
    gAlarmActive = false;
    gAlarmDurationMs = 0;
    gLcdDirty = true;
  }

  perbaruiBeep(now);
  perbaruiNada();

  if (gMessageUntilMs != 0 && (int32_t)(now - gMessageUntilMs) >= 0) {
    gMessageUntilMs = 0;
    gMessageLine1[0] = '\0';
    gMessageLine2[0] = '\0';
    gLcdDirty = true;
  }

  if (gLcdDirty || (int32_t)(now - gLcdNextRefreshMs) >= 0) {
    gambarLcd(now);
  }
}

void setLock(bool locked) {
  if (gLocked == locked) {
    Serial.printf("[OUTPUT] Lock already %s (idempoten).\n",
                  locked ? "TERKUNCI" : "TERBUKA");
    return;
  }

  gLocked = locked;
  if (locked) gRentalDeadlineMs = 0;
  digitalWrite(RELAY_PIN, locked ? RELAY_OFF : RELAY_ON);
  gLcdDirty = true;
  Serial.printf("[OUTPUT] Relay=%s pin=%s -> %s\n",
                locked ? "OFF" : "ON",
                locked ? "HIGH" : "LOW",
                locked ? "TERKUNCI" : "TERBUKA");
}

bool isLocked() {
  return gLocked;
}

void alarmOn(uint32_t durationMs) {
  uint32_t durasi = durationMs == 0 ? ALARM_DURATION_MS : durationMs;
  if (durasi < 100) durasi = 100;
  if (durasi > ALARM_MAX_DURATION_MS) durasi = ALARM_MAX_DURATION_MS;

  if (gAlarmActive) {
    Serial.printf("[ALARM] Sudah aktif, sisanya %lu ms (tidak diperpanjang).\n",
                  (unsigned long)outputs::alarmRemainingMs());
    return;
  }

  gAlarmActive = true;
  gAlarmDurationMs = durasi;
  gAlarmUntilMs = millis() + durasi;
  gNextToggleUs = micros();
  gLcdDirty = true;
  Serial.printf("[ALARM] ON selama %lu ms.\n", (unsigned long)durasi);
}

void alarmOff() {
  if (!gAlarmActive) {
    Serial.println("[ALARM] Sudah mati (idempoten).");
    return;
  }

  gAlarmActive = false;
  gAlarmDurationMs = 0;
  gLcdDirty = true;
  tulisBuzzer(false);
  Serial.println("[ALARM] OFF (command/admin).");
}

bool isAlarmActive() {
  return gAlarmActive;
}

uint32_t alarmRemainingMs() {
  if (!gAlarmActive) return 0;
  int32_t sisa = (int32_t)(gAlarmUntilMs - millis());
  return sisa > 0 ? (uint32_t)sisa : 0;
}

void warningBeep() {
  gBeepActive = true;
  gBeepPhase = BeepPhase::On;
  gBeepCount = 0;
  gBeepPhaseEndMs = millis() + WARNING_BEEP_ON_MS;
  gNextToggleUs = micros();
  Serial.printf("[OUTPUT] Warning beep mulai (%d pulse).\n",
                WARNING_BEEP_COUNT);
}

bool isBeeping() {
  return gBeepActive;
}

void setRentalRemainingMinutes(uint32_t minutes) {
  if (minutes == 0) {
    clearRentalTimer();
    return;
  }

  gRentalDeadlineMs = millis() + minutes * 60000UL;
  gLcdDirty = true;
  Serial.printf("[SEWA] Deadline LCD di-set %lu menit.\n",
                (unsigned long)minutes);
}

void extendRentalMinutes(uint32_t minutes) {
  if (minutes == 0) return;

  const uint32_t sekarang = millis();
  const uint32_t sisa = sisaSewaMs();
  gRentalDeadlineMs = sekarang + sisa + minutes * 60000UL;
  gLcdDirty = true;
  Serial.printf("[SEWA] EXTEND +%lu menit, sisa ~%lu dtk.\n",
                (unsigned long)minutes, (unsigned long)(sisa / 1000));
}

void clearRentalTimer() {
  if (gRentalDeadlineMs == 0) return;
  gRentalDeadlineMs = 0;
  gLcdDirty = true;
  Serial.println("[SEWA] Timer LCD dihapus.");
}

uint32_t rentalRemainingMs() {
  return sisaSewaMs();
}

void showMessage(const char *line1, const char *line2, uint32_t durationMs) {
  uint32_t durasi = durationMs == 0 ? LCD_MESSAGE_DEFAULT_MS : durationMs;
  strncpy(gMessageLine1, line1 != nullptr ? line1 : "", 16);
  gMessageLine1[16] = '\0';
  strncpy(gMessageLine2, line2 != nullptr ? line2 : "", 16);
  gMessageLine2[16] = '\0';
  gMessageUntilMs = millis() + durasi;
  gLcdDirty = true;
  Serial.printf("[LCD] Pesan %lu ms: '%s' | '%s'\n",
                (unsigned long)durasi, gMessageLine1, gMessageLine2);
}

void renderStatus() {
  gLcdDirty = true;
}

const char *stateLabel() {
  if (gAlarmActive) return "ALARM";
  return gLocked ? "LOCKED" : "UNLOCKED";
}

} // namespace outputs
