/*
  Trabass IoT - Security System Sepeda Sewa (ESP32)
  Milestone 3: telemetri GPS + IMU + lock state

  Arsitektur file:
    sketch_nov16a.ino  -> orkestrasi setup()/loop() + menu serial
    gps_sensor.*       -> parsing NMEA non-blocking, hasFix + lat/lng/speed
    imu_sensor.*       -> sampling MPU6050 + klasifikasi mpu_motion
    battery_sensor.*   -> stub (ENABLE_BATTERY_SENSOR 0)
    telemetry.*        -> builder payload + publish periodik
    mqtt_link.*        -> WiFi/MQTT, reconnect, antrean command
    self_test.*        -> tes komponen baseline (LCD, buzzer, relay, MPU, GPS, SIM, SMS)
    config.h           -> konfigurasi lokal (meng-include config.example.h)

  Komponen: LCD I2C, MPU6050, GPS NEO-6M, SIM800L, Relay (solenoid), Buzzer.

  Serial Monitor: 115200 baud, line ending "Both NL & CR".

  Perintah:
    n      = status jaringan + sensor (GPS/IMU/baterai)
    m      = publish telemetri sekarang
    t      = cetak payload telemetri tanpa publish
    a      = tes otomatis semua (kecuali relay)
    1      = tes LCD
    2      = tes buzzer
    3      = toggle relay (solenoid)
    4      = baca MPU6050 (5x)
    5      = status GPS (tunggu 5 detik)
    6      = tes SIM800L (AT, SIM, sinyal, registrasi)
    7      = kirim SMS tes
    s      = scan I2C
    h      = bantuan
    AT...  = perintah AT langsung ke SIM800L (contoh: AT+CSQ)

  Catatan M3:
    - tesSemua() TIDAK dijalankan otomatis saat boot.
    - Telemetri dikirim tiap TELEMETRY_INTERVAL_MS (default 5 dtk) dan
      segera setelah MQTT reconnect.
    - Saat GPS belum fix, payload memakai lat/lng null; backend tetap
      menyimpan IMU/lock dan melewati log/geofence.
    - Command MQTT baru dicatat di Serial; eksekusi aktuator di M4.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPSPlus.h>

#include "config.h"
#include "battery_sensor.h"
#include "gps_sensor.h"
#include "imu_sensor.h"
#include "mqtt_link.h"
#include "self_test.h"
#include "telemetry.h"

// ------------------------------------------------------------------
// Objek hardware global (diakses juga oleh self_test.cpp via extern)
// ------------------------------------------------------------------
LiquidCrystal_I2C lcd(LCD_ADDR, 16, 2);
TinyGPSPlus gps;
HardwareSerial gpsSerial(1);
HardwareSerial sim(2);

static char serialBuf[96];
static size_t serialLen = 0;

// ------------------------------------------------------------------
// Prototipe
// ------------------------------------------------------------------
void feedGPS();
static void bantuan();
static void printNetworkStatus();
static void handleLine(const char *line);
static void pollSerial();

// ------------------------------------------------------------------
// Util
// ------------------------------------------------------------------
void feedGPS() {
  gps_sensor::update();
}

static void bantuan() {
  Serial.println();
  Serial.println("Perintah:");
  Serial.println("  n=status jaringan+sensor | m=publish telemetri | t=payload");
#if ENABLE_SERIAL_TEST_MENU
  Serial.println("  a=semua | 1=LCD | 2=buzzer | 3=relay | 4=MPU | 5=GPS | 6=SIM | 7=SMS");
  Serial.println("  s=scan I2C | h=bantuan | AT...=langsung ke SIM800L");
#else
  Serial.println("  h=bantuan | AT...=langsung ke SIM800L");
#endif
  Serial.println();
}

static void printNetworkStatus() {
  const LinkStats &s = mqtt_link::stats();

  Serial.println("[NET] ---------- status ----------");
  Serial.printf("[NET] WiFi=%s MQTT=%s state=%s\n",
                mqtt_link::wifiConnected() ? "ya" : "tidak",
                mqtt_link::mqttConnected() ? "ya" : "tidak",
                mqtt_link::stateLabel());
  Serial.printf("[NET] SSID=%s IP=%s RSSI=%d dBm\n",
                mqtt_link::ssid().c_str(),
                mqtt_link::localIp().c_str(),
                (int)mqtt_link::rssi());
  Serial.printf("[NET] Broker=%s:%d\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  Serial.printf("[NET] publish=%lu gagal=%lu cmd=%lu uptime=%lu ms\n",
                (unsigned long)s.publishCount,
                (unsigned long)s.publishFailures,
                (unsigned long)s.commandCount,
                (unsigned long)millis());

  Serial.printf("[SENSOR] GPS fix=%s sat=%d lat/lng=%s\n",
                gps_sensor::hasFix() ? "ya" : "tidak",
                gps_sensor::getSats(),
                gps_sensor::hasFix() ? "tersedia" : "null");
  if (gps_sensor::hasFix()) {
    Serial.printf("[SENSOR]   lat=%.6f lng=%.6f speed=%.1f km/jam\n",
                  gps_sensor::getLat(),
                  gps_sensor::getLng(),
                  gps_sensor::getSpeed());
  }
  Serial.printf("[SENSOR] IMU siap=%s mpu_motion=%s\n",
                imu_sensor::healthy() ? "ya" : "tidak",
                imu_sensor::motion() ? "true" : "false");

  float batteryPercent = 0.0f;
  if (battery_sensor::read(batteryPercent)) {
    Serial.printf("[SENSOR] Baterai=%.0f%%\n", (double)batteryPercent);
  } else {
    Serial.println("[SENSOR] Baterai=N/A (sensor belum terpasang)");
  }
}

static void handleLine(const char *line) {
  if (line == nullptr || line[0] == '\0') return;

  // Perintah AT diteruskan apa adanya ke SIM800L.
  if (strncmp(line, "AT", 2) == 0 || strncmp(line, "at", 2) == 0) {
    sim.println(line);
    return;
  }

  switch (line[0]) {
    case 'n': printNetworkStatus(); break;
    case 'm': telemetry::publishNow(); break;
    case 't': telemetry::printPayload(); break;
#if ENABLE_SERIAL_TEST_MENU
    case 'a': self_test::tesSemua(); break;
    case '1': self_test::tesLCD(); break;
    case '2': self_test::tesBuzzer(); break;
    case '3': self_test::toggleRelay(); break;
    case '4': self_test::tesMPU(); break;
    case '5': self_test::tesGPS(); break;
    case '6': self_test::tesSIM(); break;
    case '7': self_test::kirimSMS(); break;
    case 's': self_test::i2cScan(); break;
#endif
    case 'h': bantuan(); break;
    default: bantuan(); break;
  }
}

static void pollSerial() {
  // Non-blocking: akumulasi karakter, proses saat newline.
  while (Serial.available()) {
    int c = Serial.read();
    if (c < 0) break;

    if (c == '\r') continue;

    if (c == '\n') {
      serialBuf[serialLen] = '\0';
      if (serialLen > 0) handleLine(serialBuf);
      serialLen = 0;
    } else if (serialLen < sizeof(serialBuf) - 1) {
      serialBuf[serialLen++] = (char)c;
    }
  }
}

// ------------------------------------------------------------------
// Setup & loop
// ------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);

  Serial.println();
  Serial.printf("[BOOT] Trabass fw=%s bike=%s\n", FW_VERSION, BIKE_ID);

  // Fail-safe: terkunci dan buzzer diam sejak boot.
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, RELAY_OFF);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Serial.println("[BOOT] Relay=OFF (terkunci), buzzer=LOW");

  Wire.begin(SDA_PIN, SCL_PIN);
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  sim.begin(SIM_BAUD, SERIAL_8N1, SIM_RX_PIN, SIM_TX_PIN);

  gps_sensor::begin();
  imu_sensor::begin();
  battery_sensor::begin();
  telemetry::begin();

#if ENABLE_SERIAL_TEST_MENU
  bantuan();
#endif

  mqtt_link::begin();
}

void loop() {
  feedGPS();
  imu_sensor::update();
  mqtt_link::loop();
  telemetry::loop();
  pollSerial();

  // Balasan AT manual / SIM800L diteruskan ke Serial.
  while (sim.available()) Serial.write(sim.read());
}
