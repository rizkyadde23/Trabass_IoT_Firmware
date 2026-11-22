/*
 * Trabass IoT - Implementasi tes komponen baseline
 * ------------------------------------------------------------------
 * Perilaku sengaja dipertahankan sama dengan sketch_nov16a.ino asli:
 *   - tesGPS()  : blocking 5 detik (dipanggil manual).
 * Fungsi-fungsi ini TIDAK dipanggil dari loop() otomatis, sehingga
 * tidak mengganggu koneksi MQTT pada operasi normal. Tes SIM/SMS sudah
 * dihapus di M6; diagnostik modem ada di menu '6'/'9'.
 */

#include <Wire.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPSPlus.h>

#include "config.h"
#include "self_test.h"
#include "imu_sensor.h"
#include "outputs.h"

// Objek hardware dimiliki oleh sketch_nov16a.ino.
extern LiquidCrystal_I2C lcd;
extern TinyGPSPlus gps;
extern HardwareSerial gpsSerial;
extern void feedGPS();

namespace {

bool i2cAda(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void beep(int ms) {
  // Toggle ~2 kHz, jalan untuk buzzer aktif maupun pasif.
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(250);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(250);
  }
  digitalWrite(BUZZER_PIN, LOW);
}

} // namespace

namespace self_test {

bool tesLCD() {
  Serial.println("[LCD] Tes...");
  if (!i2cAda(LCD_ADDR)) {
    Serial.println("  GAGAL: LCD tidak terdeteksi di alamat tersebut");
    return false;
  }

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("LCD OK");
  lcd.setCursor(0, 1);
  lcd.print("Sepeda Sewa IoT");
  Serial.println("  OK: cek tulisan di LCD (atur kontras bila kosong)");
  return true;
}

void tesBuzzer() {
  Serial.println("[BUZZER] Bunyi 3x...");
  for (int i = 0; i < 3; i++) {
    beep(200);
    delay(200);
  }
  Serial.println("  Selesai. Apakah terdengar? (dicek manual)");
}

void toggleRelay() {
  // M4: state relay dimiliki outputs, menu tes hanya meminta perubahan.
  // Jika sedang terkunci -> buka (relay ON); sebaliknya kunci lagi.
  const bool akanAktif = outputs::isLocked();
  outputs::setLock(!akanAktif);
  Serial.printf("[RELAY] %s (solenoid %s)\n",
                akanAktif ? "ON" : "OFF",
                akanAktif ? "teraktivasi" : "nonaktif");
  Serial.println("  Dengarkan bunyi 'klik' relay dan cek solenoid.");
}

bool tesMPU() {
  Serial.println("[MPU6050] Tes...");
  // begin() melakukan auto-probe 0x68/0x69, jadi tidak ada pengecekan
  // alamat tunggal lagi di sini.
  if (!imu_sensor::begin()) {
    Serial.println("  GAGAL: tidak ada MPU6050 di 0x68/0x69 (cek VCC/GND/SDA/SCL)");
    return false;
  }
  Serial.printf("  Alamat terdeteksi: 0x%02X\n",
                imu_sensor::detectedAddress());

  for (int i = 0; i < 5; i++) {
    imu_sensor::Sample s;
    if (imu_sensor::readSample(s)) {
      Serial.printf("  Acc[g] %.2f %.2f %.2f | Gyro[d/s] %.1f %.1f %.1f | Suhu %.1f C\n",
                    s.ax, s.ay, s.az, s.gx, s.gy, s.gz, s.tempC);
    } else {
      Serial.println("  GAGAL baca data");
      return false;
    }
    delay(500);
  }

  Serial.printf("  OK. Status motion classifier: %s\n",
                imu_sensor::motion() ? "GERAK" : "DIAM");
  Serial.println("  Goyangkan sensor, nilai harus berubah (diam datar: az ~1.0).");
  return true;
}

bool tesGPS() {
  Serial.println("[GPS] Membaca 5 detik...");
  unsigned long start = millis();
  while (millis() - start < 5000) feedGPS();

  Serial.printf("  Karakter diterima: %lu | Kalimat NMEA valid: %lu\n",
                (unsigned long)gps.charsProcessed(),
                (unsigned long)gps.passedChecksum());

  if (gps.charsProcessed() < 10) {
    Serial.println("  GAGAL: tidak ada data dari GPS (cek TX/RX dan VCC)");
    return false;
  }

  Serial.printf("  Satelit: %d\n",
                gps.satellites.isValid() ? (int)gps.satellites.value() : 0);

  if (gps.location.isValid()) {
    Serial.printf("  FIX OK -> Lat: %.6f  Lng: %.6f\n",
                  gps.location.lat(), gps.location.lng());
  } else {
    Serial.println("  Belum fix. Butuh langit terbuka (cold start 1-15 menit).");
  }
  return true;
}

void i2cScan() {
  Serial.println("[I2C] Scan...");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    if (i2cAda(addr)) {
      Serial.printf("  ditemukan: 0x%02X\n", addr);
      found++;
    }
  }
  if (found == 0) {
    Serial.println("  tidak ada perangkat (cek SDA/SCL, VCC, GND)");
  }
}

void tesSemua() {
  Serial.println("\n========== TES OTOMATIS ==========");
  i2cScan();
  bool rLCD = tesLCD();
  tesBuzzer();
  bool rMPU = tesMPU();
  bool rGPS = tesGPS();

  Serial.println("\n========== RINGKASAN ==========");
  Serial.printf("LCD I2C    : %s\n", rLCD ? "OK" : "GAGAL");
  Serial.println("Buzzer     : dijalankan (cek bunyi)");
  Serial.printf("MPU6050    : %s\n", rMPU ? "OK" : "GAGAL");
  Serial.printf("GPS        : %s\n", rGPS ? "data masuk" : "GAGAL");
  Serial.println("GPRS       : cek menu '6' / status 'n'");
  Serial.println("Relay      : tes manual, ketik 3");
  Serial.println("===============================\n");

  if (rLCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(rMPU ? "MPU:OK " : "MPU:ERR ");
    lcd.print(rGPS ? "GPS:OK" : "GPS:ERR");
    lcd.setCursor(0, 1);
    lcd.print("GPRS:menu6 ");
    lcd.print("Relay:cmd3");
  }
}

bool relayState() {
  // true = relay ON = solenoid aktif = sepeda terbuka.
  return !outputs::isLocked();
}

} // namespace self_test
