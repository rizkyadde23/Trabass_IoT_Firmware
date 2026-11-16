/*
 * Trabass IoT - Implementasi tes komponen baseline
 * ------------------------------------------------------------------
 * Perilaku sengaja dipertahankan sama dengan sketch_nov16a.ino asli:
 *   - tesGPS()  : blocking 5 detik (dipanggil manual).
 *   - tesSIM()  : beberapa AT dengan timeout pendek (manual).
 *   - kirimSMS(): hingga ~20 detik (manual).
 * Fungsi-fungsi ini TIDAK dipanggil dari loop() otomatis, sehingga
 * tidak mengganggu koneksi MQTT pada operasi normal.
 */

#include <Wire.h>
#include <HardwareSerial.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPSPlus.h>

#include "config.h"
#include "self_test.h"

// Objek hardware dimiliki oleh sketch_nov16a.ino.
extern LiquidCrystal_I2C lcd;
extern TinyGPSPlus gps;
extern HardwareSerial gpsSerial;
extern HardwareSerial sim;
extern void feedGPS();

namespace {

bool relayOn = false;

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

bool mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B); // PWR_MGMT_1
  Wire.write(0x00); // bangunkan sensor
  return Wire.endTransmission() == 0;
}

bool mpuRead(float &ax, float &ay, float &az,
             float &gx, float &gy, float &gz, float &temp) {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14) != 14) return false;

  int16_t v[7];
  for (int i = 0; i < 7; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    v[i] = (hi << 8) | lo;
  }

  ax = v[0] / 16384.0;
  ay = v[1] / 16384.0;
  az = v[2] / 16384.0;
  temp = v[3] / 340.0 + 36.53;
  gx = v[4] / 131.0;
  gy = v[5] / 131.0;
  gz = v[6] / 131.0;
  return true;
}

String kirimAT(const String &cmd, uint32_t timeout = 2000) {
  while (sim.available()) sim.read();
  sim.println(cmd);

  String resp = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (sim.available()) resp += (char)sim.read();
    if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
  }

  resp.trim();
  Serial.print("  >> ");
  Serial.println(cmd);
  Serial.println(resp.length() ? resp : "  (tidak ada balasan)");
  return resp;
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
  relayOn = !relayOn;
  digitalWrite(RELAY_PIN, relayOn ? RELAY_ON : RELAY_OFF);
  Serial.printf("[RELAY] %s (solenoid %s)\n",
                relayOn ? "ON" : "OFF",
                relayOn ? "teraktivasi" : "nonaktif");
  Serial.println("  Dengarkan bunyi 'klik' relay dan cek solenoid.");
}

bool tesMPU() {
  Serial.println("[MPU6050] Tes...");
  if (!i2cAda(MPU_ADDR)) {
    Serial.println("  GAGAL: MPU6050 tidak terdeteksi (cek wiring/alamat)");
    return false;
  }
  if (!mpuInit()) {
    Serial.println("  GAGAL: inisialisasi");
    return false;
  }

  for (int i = 0; i < 5; i++) {
    float ax, ay, az, gx, gy, gz, t;
    if (mpuRead(ax, ay, az, gx, gy, gz, t)) {
      Serial.printf("  Acc[g] %.2f %.2f %.2f | Gyro[d/s] %.1f %.1f %.1f | Suhu %.1f C\n",
                    ax, ay, az, gx, gy, gz, t);
    } else {
      Serial.println("  GAGAL baca data");
      return false;
    }
    delay(500);
  }

  Serial.println("  OK. Goyangkan sensor, nilai harus berubah (diam datar: az ~1.0).");
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

bool tesSIM() {
  Serial.println("[SIM800L] Tes...");
  bool hidup = false;
  for (int i = 0; i < 5; i++) {
    if (kirimAT("AT", 1000).indexOf("OK") >= 0) {
      hidup = true;
      break;
    }
    delay(500);
  }

  if (!hidup) {
    Serial.println("  GAGAL: modul tidak membalas (cek TX/RX, GND, 5V)");
    return false;
  }

  kirimAT("ATE0");
  bool simOk = kirimAT("AT+CPIN?").indexOf("READY") >= 0;
  kirimAT("AT+CSQ");
  String reg = kirimAT("AT+CREG?");
  bool net = reg.indexOf(",1") >= 0 || reg.indexOf(",5") >= 0;
  kirimAT("AT+CBC");

  Serial.printf("  Kartu SIM: %s | Jaringan: %s\n",
                simOk ? "OK" : "BERMASALAH",
                net ? "TERDAFTAR" : "BELUM TERDAFTAR");
  return simOk && net;
}

void kirimSMS() {
  Serial.println("[SMS] Mengirim SMS tes...");
  kirimAT("AT+CMGF=1");
  kirimAT("AT+CSCA?");

  while (sim.available()) sim.read();
  sim.print("AT+CMGS=\"");
  sim.print(NOMOR_TUJUAN);
  sim.println("\"");

  String resp = "";
  unsigned long start = millis();
  while (millis() - start < 5000) {
    while (sim.available()) resp += (char)sim.read();
    if (resp.indexOf('>') >= 0 || resp.indexOf("ERROR") >= 0) break;
  }

  Serial.println("  Balasan CMGS: " + resp);
  if (resp.indexOf('>') < 0) {
    Serial.println("  GAGAL: tidak ada prompt '>' (cek nomor/SIM/jaringan)");
    return;
  }

  sim.print("Tes SIM800L dari ESP32 berhasil");
  sim.write(26); // Ctrl+Z

  resp = "";
  start = millis();
  while (millis() - start < SMS_SEND_TIMEOUT_MS) {
    while (sim.available()) resp += (char)sim.read();
    if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
  }
  Serial.println(resp.length() ? resp : "  (tidak ada balasan)");
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
  bool rSIM = tesSIM();

  Serial.println("\n========== RINGKASAN ==========");
  Serial.printf("LCD I2C    : %s\n", rLCD ? "OK" : "GAGAL");
  Serial.println("Buzzer     : dijalankan (cek bunyi)");
  Serial.printf("MPU6050    : %s\n", rMPU ? "OK" : "GAGAL");
  Serial.printf("GPS        : %s\n", rGPS ? "data masuk" : "GAGAL");
  Serial.printf("SIM800L    : %s\n", rSIM ? "OK (SIM + jaringan)" : "GAGAL/BELUM SIAP");
  Serial.println("Relay      : tes manual, ketik 3");
  Serial.println("===============================\n");

  if (rLCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(rMPU ? "MPU:OK " : "MPU:ERR ");
    lcd.print(rGPS ? "GPS:OK" : "GPS:ERR");
    lcd.setCursor(0, 1);
    lcd.print(rSIM ? "SIM:OK " : "SIM:ERR ");
    lcd.print("Relay:cmd3");
  }
}

bool relayState() {
  return relayOn;
}

} // namespace self_test
