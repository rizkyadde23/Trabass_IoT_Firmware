/*
  TES SEMUA KOMPONEN - Security System Sepeda Sewa (ESP32)
  Komponen: LCD I2C, MPU6050, GPS NEO-6M, SIM800L, Relay (solenoid), Buzzer

  Library (Library Manager):
    - LiquidCrystal I2C   (Frank de Brabander)
    - TinyGPSPlus         (Mikal Hart)
  MPU6050 dibaca langsung lewat Wire, tanpa library tambahan.

  Serial Monitor: 115200 baud, line ending "Both NL & CR"

  Perintah:
    a  = tes otomatis semua (kecuali relay)
    1  = tes LCD
    2  = tes buzzer
    3  = toggle relay (solenoid)
    4  = baca MPU6050 (5x)
    5  = status GPS (tunggu 5 detik)
    6  = tes SIM800L (AT, SIM, sinyal, registrasi)
    7  = kirim SMS tes
    s  = scan I2C
    h  = bantuan
    AT...  = perintah AT langsung ke SIM800L (contoh: AT+CSQ)
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPSPlus.h>

// ================== PIN (sesuaikan dengan wiring) ==================
#define SDA_PIN     21    // LCD + MPU6050
#define SCL_PIN     22
#define GPS_RX_PIN  16    // ESP32 RX <- TX GPS
#define GPS_TX_PIN  17    // ESP32 TX -> RX GPS
#define SIM_RX_PIN  26    // ESP32 RX <- TXD SIM800L
#define SIM_TX_PIN  27    // ESP32 TX -> RXD SIM800L
#define RELAY_PIN   4
#define BUZZER_PIN  19

// Polaritas relay: kebanyakan modul relay aktif LOW. Jika terbalik, tukar nilainya.
#define RELAY_ON    LOW
#define RELAY_OFF   HIGH

#define LCD_ADDR    0x27  // kalau LCD tidak tampil, coba 0x3F (cek hasil scan I2C)
#define MPU_ADDR    0x68

// Isi nomor tujuan format internasional untuk tes SMS
const char* NOMOR_TUJUAN = "+62XXXXXXXXXXX";
// ====================================================================

LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
HardwareSerial sim(2);

bool relayOn = false;

// ---------- Util ----------
void feedGPS() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
}

bool i2cAda(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void i2cScan() {
  Serial.println("[I2C] Scan...");
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (i2cAda(a)) {
      Serial.printf("  ditemukan: 0x%02X\n", a);
      n++;
    }
  }
  if (!n) Serial.println("  tidak ada perangkat (cek SDA/SCL, VCC, GND)");
}

// ---------- LCD ----------
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
  Serial.println("  OK: cek tulisan di LCD (atur kontras dengan potensio di belakang LCD bila kosong)");
  return true;
}

// ---------- Buzzer ----------
void beep(int ms) {
  // toggle ~2kHz, jalan untuk buzzer aktif maupun pasif
  unsigned long start = millis();
  while (millis() - start < (unsigned long)ms) {
    digitalWrite(BUZZER_PIN, HIGH);
    delayMicroseconds(250);
    digitalWrite(BUZZER_PIN, LOW);
    delayMicroseconds(250);
  }
  digitalWrite(BUZZER_PIN, LOW);
}

bool tesBuzzer() {
  Serial.println("[BUZZER] Bunyi 3x...");
  for (int i = 0; i < 3; i++) {
    beep(200);
    delay(200);
  }
  Serial.println("  Selesai. Apakah terdengar? (dicek manual)");
  return true;
}

// ---------- Relay / Solenoid ----------
void toggleRelay() {
  relayOn = !relayOn;
  digitalWrite(RELAY_PIN, relayOn ? RELAY_ON : RELAY_OFF);
  Serial.printf("[RELAY] %s (solenoid %s)\n",
                relayOn ? "ON" : "OFF",
                relayOn ? "teraktivasi" : "nonaktif");
  Serial.println("  Dengarkan bunyi 'klik' relay dan cek solenoid bergerak.");
}

// ---------- MPU6050 ----------
bool mpuInit() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);   // PWR_MGMT_1
  Wire.write(0x00);   // bangunkan sensor
  return Wire.endTransmission() == 0;
}

bool mpuRead(float &ax, float &ay, float &az, float &gx, float &gy, float &gz, float &temp) {
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
  ax = v[0] / 16384.0; ay = v[1] / 16384.0; az = v[2] / 16384.0;   // g
  temp = v[3] / 340.0 + 36.53;                                      // C
  gx = v[4] / 131.0;  gy = v[5] / 131.0;  gz = v[6] / 131.0;       // derajat/detik
  return true;
}

bool tesMPU() {
  Serial.println("[MPU6050] Tes...");
  if (!i2cAda(MPU_ADDR)) {
    Serial.println("  GAGAL: MPU6050 tidak terdeteksi (cek SDA/SCL/VCC/GND, alamat 0x68 atau 0x69)");
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
  Serial.println("  OK. Coba miringkan/goyangkan sensor, nilai harus berubah (diam datar: az ~ 1.0).");
  return true;
}

// ---------- GPS ----------
bool tesGPS() {
  Serial.println("[GPS] Membaca 5 detik...");
  unsigned long start = millis();
  while (millis() - start < 5000) feedGPS();

  Serial.printf("  Karakter diterima: %lu | Kalimat NMEA valid: %lu\n",
                (unsigned long)gps.charsProcessed(), (unsigned long)gps.passedChecksum());
  if (gps.charsProcessed() < 10) {
    Serial.println("  GAGAL: tidak ada data dari GPS (cek TX/RX dan VCC)");
    return false;
  }
  Serial.printf("  Satelit: %d\n", gps.satellites.isValid() ? (int)gps.satellites.value() : 0);
  if (gps.location.isValid()) {
    Serial.printf("  FIX OK -> Lat: %.6f  Lng: %.6f\n", gps.location.lat(), gps.location.lng());
  } else {
    Serial.println("  Belum dapat fix. Modul hidup, tapi butuh langit terbuka (cold start bisa 1-15 menit).");
  }
  return true;
}

// ---------- SIM800L ----------
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

bool tesSIM() {
  Serial.println("[SIM800L] Tes...");
  bool hidup = false;
  for (int i = 0; i < 5; i++) {
    if (kirimAT("AT", 1000).indexOf("OK") >= 0) { hidup = true; break; }
    delay(500);
  }
  if (!hidup) {
    Serial.println("  GAGAL: modul tidak membalas (cek TX/RX, GND, suplai 5V)");
    return false;
  }
  kirimAT("ATE0");
  bool simOk = kirimAT("AT+CPIN?").indexOf("READY") >= 0;
  kirimAT("AT+CSQ");
  String reg = kirimAT("AT+CREG?");
  bool net = reg.indexOf(",1") >= 0 || reg.indexOf(",5") >= 0;
  kirimAT("AT+CBC");
  Serial.printf("  Kartu SIM: %s | Jaringan: %s\n", simOk ? "OK" : "BERMASALAH", net ? "TERDAFTAR" : "BELUM TERDAFTAR");
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

  // tunggu prompt '>' maksimal 5 detik
  String resp = "";
  unsigned long start = millis();
  while (millis() - start < 5000) {
    while (sim.available()) resp += (char)sim.read();
    if (resp.indexOf('>') >= 0 || resp.indexOf("ERROR") >= 0) break;
  }
  Serial.println("  Balasan CMGS: " + resp);
  if (resp.indexOf('>') < 0) {
    Serial.println("  GAGAL: tidak ada prompt '>' (cek nomor tujuan / SIM / jaringan)");
    return;
  }

  sim.print("Tes SIM800L dari ESP32 berhasil");
  sim.write(26);   // Ctrl+Z

  resp = "";
  start = millis();
  while (millis() - start < 20000) {
    while (sim.available()) resp += (char)sim.read();
    if (resp.indexOf("OK") >= 0 || resp.indexOf("ERROR") >= 0) break;
  }
  Serial.println(resp.length() ? resp : "  (tidak ada balasan)");
}

// ---------- Tes otomatis ----------
void tesSemua() {
  Serial.println("\n========== TES OTOMATIS ==========");
  i2cScan();
  bool rLCD = tesLCD();
  bool rBuz = tesBuzzer();
  bool rMPU = tesMPU();
  bool rGPS = tesGPS();
  bool rSIM = tesSIM();

  Serial.println("\n========== RINGKASAN ==========");
  Serial.printf("LCD I2C    : %s\n", rLCD ? "OK" : "GAGAL");
  Serial.printf("Buzzer     : %s\n", rBuz ? "dijalankan (cek bunyi)" : "GAGAL");
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

void bantuan() {
  Serial.println("\nPerintah: a=semua | 1=LCD | 2=buzzer | 3=relay | 4=MPU | 5=GPS | 6=SIM | 7=SMS | s=scan I2C | AT...=langsung ke SIM800L\n");
}

// ---------- Setup & loop ----------
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  sim.begin(9600, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

  delay(3000);   // beri waktu SIM800L boot
  bantuan();
  tesSemua();
}

void loop() {
  feedGPS();

  if (Serial.available()) {
    String in = Serial.readStringUntil('\n');
    in.trim();
    if (in.length() == 0) return;

    if (in.startsWith("AT") || in.startsWith("at")) {
      sim.println(in);
    } else {
      switch (in[0]) {
        case 'a': tesSemua(); break;
        case '1': tesLCD(); break;
        case '2': tesBuzzer(); break;
        case '3': toggleRelay(); break;
        case '4': tesMPU(); break;
        case '5': tesGPS(); break;
        case '6': tesSIM(); break;
        case '7': kirimSMS(); break;
        case 's': i2cScan(); break;
        default:  bantuan(); break;
      }
    }
  }

  // tampilkan balasan SIM800L dari perintah AT manual
  while (sim.available()) Serial.write(sim.read());
}