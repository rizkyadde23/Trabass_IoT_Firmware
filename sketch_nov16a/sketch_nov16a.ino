/*
  Trabass IoT - Security System Sepeda Sewa (ESP32)
  Milestone 2: WiFi + MQTT skeleton

  Arsitektur file:
    sketch_nov16a.ino  -> orkestrasi setup()/loop() + menu serial
    mqtt_link.*        -> WiFi/MQTT, reconnect, heartbeat, antrean command
    self_test.*        -> tes komponen baseline (LCD, buzzer, relay, MPU, GPS, SIM, SMS)
    config.h           -> konfigurasi lokal (meng-include config.example.h)

  Komponen: LCD I2C, MPU6050, GPS NEO-6M, SIM800L, Relay (solenoid), Buzzer.

  Serial Monitor: 115200 baud, line ending "Both NL & CR".

  Perintah:
    n      = status jaringan (WiFi/MQTT/IP/RSSI)
    m      = publish heartbeat sekarang
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

  Catatan M2:
    - tesSemua() TIDAK dijalankan otomatis saat boot.
    - Command MQTT baru dicatat di Serial; eksekusi aktuator di M4.
    - Heartbeat memakai topik telemetri dengan type "heartbeat".
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TinyGPSPlus.h>

#include "config.h"
#include "mqtt_link.h"
#include "self_test.h"

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
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
}

static void bantuan() {
  Serial.println();
  Serial.println("Perintah:");
  Serial.println("  n=status jaringan | m=publish heartbeat");
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
    case 'm': mqtt_link::publishHeartbeat(true); break;
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

#if ENABLE_SERIAL_TEST_MENU
  bantuan();
#endif

  mqtt_link::begin();
}

void loop() {
  feedGPS();
  mqtt_link::loop();
  pollSerial();

  // Balasan AT manual / SIM800L diteruskan ke Serial.
  while (sim.available()) Serial.write(sim.read());
}
