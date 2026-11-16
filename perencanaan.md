# Perencanaan Integrasi Firmware — Trabass IoT

**Lokasi dokumen:** `d:\Code\ARDUINO\Praktikum IOT\Trabass-IoT-Firmware\perencanaan.md`
**Versi:** 1.0 — **Tanggal:** 2026-11-16
**Status saat ini:** Milestone 1 (M1) sedang dikerjakan
**Bahasa:** teknis Indonesia (tingkat mahasiswa Informatika)

---

## 1. Tujuan dokumen

Dokumen ini menjadi **sumber tunggal rencana integrasi** firmware ESP32 dengan backend dan frontend yang sudah ada. Isinya:

1. Keputusan arsitektur yang sudah dikunci bersama user.
2. Rincian milestone **M1 sampai M7** beserta kriteria selesai.
3. Matriks test case dengan kolom **Actual** dan **Status** yang diisi hanya jika pengujian benar-benar dijalankan.
4. Aturan kerja inkremental: satu milestone selesai → user konfirmasi → milestone berikutnya.

Dokumen pendamping:

- `konteks.md` → fakta lapangan, wiring, kontrak data, temuan, batasan.
- `struktur.md` → struktur folder dan tanggung jawab tiap file.

---

## 2. Prinsip kerja (commit bersama user)

| No | Prinsip | Konsekuensi teknis |
|---|---|---|
| P1 | Jalankan di PC lokal dulu | MQTT ke `10.3.24.125:1883`, bukan cloud |
| P2 | Jangan merombak yang sudah jalan | Schema DB, endpoint, dan hardware test existing dipertahankan |
| P3 | Perubahan kecil dan terisolasi | Patch backend no-GPS-fix hanya di `telemetryController.js` |
| P4 | Inkremental per milestone | Tidak ada kode M3 sebelum M2 diterima user |
| P5 | Tidak klaim sukses tanpa bukti | Kolom Actual berisi log/output nyata; jika belum diuji → `NOT RUN` |
| P6 | Modular tapi tidak berlebihan | Pecah per komponen, bukan per fungsi kecil |
| P7 | Konfigurasi terpisah dari logika | Semua pin, IP, threshold, dan flag ada di `config.h`/`config.example.h` |
| P8 | Logging + error handling | Tiap modul mencetak prefix `[MODUL]`, gagal → log + state aman, tidak reset |
| P9 | Keamanan minimum | Kredensial tidak di chat, topic dibatasi per bike ID, SMS sanitasi ASCII |

---

## 3. Keputusan arsitektur final

| Topik | Keputusan | Alasan |
|---|---|---|
| Jaringan | ESP32 + PC satu router Wi-Fi 2.4 GHz | Backend dan broker sudah bind `0.0.0.0`, firewall diizinkan |
| Protokol device | MQTT over Wi-Fi | Backend sudah subscribe `bike/telemetry/+` dan publish command |
| Protokol UI | REST + polling 5 detik | Implementasi frontend sudah seperti ini; tidak perlu WebSocket |
| SIM800L | SMS saja | Tidak ada stack GPRS/MQTT; dipakai untuk notifikasi/fallback |
| GPS tanpa fix | Patch backend kecil | Baterai/IMU/lock tetap masuk dashboard saat demo indoor |
| Detail IMU | Boolean `mpu_motion` saja | Tidak perlu migration schema, tidak perlu grafik raw |
| Baterai | Ditunda (`N/A`) | Hardware divider/INA219 belum tersedia; modul dibuat stub |
| Reliability | Reconnect + publish state terakhir + stale 30 s | Sesuai scope lokal; ACK/retry/NVS ditunda |
| Struktur firmware | Modular per komponen | Memudahkan perawatan dan pengujian |
| Alarm default | 60 detik, configurable | Mencegah buzzer menyala terus |
| Reboot saat sewa | Default **terkunci** (fail-safe) | Menghindari sepeda terbuka tanpa sesi aktif |
| Relay | Aktif LOW; `LOCK` = relay OFF (pin HIGH) | Sesuai `RELAY_ON LOW` / `RELAY_OFF HIGH` di baseline |
| Bike ID | `BK-001` | Konvensi yang dipakai kontrak MQTT |

### 3.1 Diagram alur milestone

```mermaid
gantt
    title Roadmap Integrasi Trabass IoT (lokal)
    dateFormat  YYYY-MM-DD
    section Fondasi
    M1 Baseline & lingkungan   :m1, 2026-11-16, 2d
    M2 WiFi + MQTT skeleton    :m2, after m1, 3d
    section Data
    M3 Telemetri + patch GPS   :m3, after m2, 4d
    M4 Command + aktuator      :m4, after m3, 4d
    section Notifikasi
    M5 SMS SIM800L             :m5, after m4, 3d
    section Validasi
    M6 End-to-end bisnis       :m6, after m5, 4d
    M7 Reliability & final     :m7, after m6, 3d
```

> Tanggal di diagram adalah **estimasi relatif**, bukan komitmen waktu. Tiap milestone menunggu konfirmasi user.

### 3.2 Alur kerja satu milestone

```mermaid
flowchart TD
    A[Analisis milestone] --> B[Implementasi inkremental]
    B --> C[Kompilasi & upload]
    C --> D[Uji sesuai matriks]
    D --> E{Bukti lengkap?}
    E -- Tidak --> B
    E -- Ya --> F[Isi Actual & Status di perencanaan.md]
    F --> G[Lapor ke user + minta konfirmasi]
    G --> H[Milestone berikutnya]
```

---

## 4. Rincian milestone

### M1 — Baseline, konfigurasi, dan verifikasi lingkungan

**Tujuan:** memastikan kode tes existing aman, library tersedia, jaringan lokal terjangkau, dan dokumentasi fondasi terbentuk. **Belum ada logika integrasi yang ditulis.**

**File yang dibuat/diubah:**

- `baseline/sketch_nov16a_baseline/sketch_nov16a_baseline.ino` + `README.md`
- `sketch_nov16a/config.example.h`
- `sketch_nov16a/config.h`
- `perencanaan.md`, `konteks.md`, `struktur.md`

**Langkah:**

1. Salin `sketch_nov16a.ino` ke folder baseline dan rekam SHA-256.
2. Instal library: TinyGPSPlus `v1.0.3a`, PubSubClient `v2.8`, ArduinoJson `v7.4.3`.
3. Verifikasi board ESP32 (paket `esp32` 3.3.12) dan library existing `LiquidCrystal_I2C`.
4. Verifikasi `GET /health` backend dan port yang listen.
5. Verifikasi IP LAN `10.3.24.125` untuk port 1883 dan 3000.
6. Verifikasi firewall inbound Node.js dan mosquitto.
7. Tulis kontrak telemetri, wiring, dan checklist baseline ke `konteks.md`.
8. Tulis struktur folder ke `struktur.md`.

**Kriteria selesai M1:**

- [x] Backup baseline memiliki SHA-256 terekam
- [x] Ketiga library terinstal di sketchbook library
- [x] `/health` sukses, MySQL/Redis/MQTT OK
- [x] Port 1883/3000/3306/6379 listen dan firewall mengizinkan
- [x] `10.3.24.125:1883` dan `:3000` reachable dari PC
- [x] `config.example.h` + `config.h` terbentuk
- [x] Tiga dokumen perencanaan/konteks/struktur terbentuk
- [ ] User mengisi `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_BROKER_HOST` di `config.h`
- [ ] User menjalankan menu tes baseline `a`, `1`–`7`, `s` dan melaporkan hasilnya

**Status M1:** dokumentasi dan verifikasi lingkungan selesai; menunggu pengisian kredensial + hasil tes hardware dari user.

---

### M2 — Kerangka komunikasi firmware

**Tujuan:** ESP32 terhubung Wi-Fi dan MQTT, subscribe command, publish heartbeat, dan pulih otomatis.

**File:** `sketch_nov16a.ino`, `app_types.h`, `mqtt_link.h/.cpp`, `config.h`

**Langkah:**

1. Init serial, `Wire`, dan pin dengan state aman (relay OFF, buzzer LOW).
2. `wifiEnsureConnected()` non-blocking dengan timeout dan retry dari config.
3. `mqttEnsureConnected()` dengan client ID unik `trabass-BK-001`, subscribe `bike/command/BK-001`.
4. Heartbeat/status tiap 5 detik (boleh memakai payload telemetri minimum).
5. Reconnect backoff; setelah reconnect, publish state terakhir jika `MQTT_PUBLISH_ON_RECONNECT`.
6. Callback MQTT hanya menaruh command ke antrean, tidak mengubah GPIO langsung.

**Pengujian:** B02–B04, T02–T04, T25.

**Kriteria selesai:** reconnect < 15 detik, tidak ada watchdog reset, heartbeat terlihat di `mosquitto_sub`, menu tes serial masih bisa dipakai.

---

### M3 — Telemetri GPS, IMU, lock state, dan patch backend

**Tujuan:** data sensor masuk Redis/MySQL/dashboard, termasuk saat GPS belum fix.

**File:** `gps_sensor.h/.cpp`, `imu_sensor.h/.cpp`, `battery_sensor.h/.cpp` (stub), `telemetry.h/.cpp`, `outputs.h/.cpp`, `src/controllers/telemetryController.js`

**Langkah:**

1. GPS dibaca non-blocking lewat `gps.encode()` di tiap loop; simpan `lat`, `lng`, `speed`, `satellites`, `gps_fix`.
2. MPU6050 sample tiap 100 ms; klasifikasi `mpu_motion` dengan threshold + hold/idle dari config.
3. Modul baterai mengembalikan `false` karena `ENABLE_BATTERY_SENSOR 0`; field `battery` tidak dikirim.
4. Builder JSON menghasilkan payload kontrak (lihat `konteks.md` bagian 6).
5. Publish tiap `TELEMETRY_INTERVAL_MS`.
6. Patch backend: telemetri tanpa `lat/lng` tetap update Redis (lock/IMU/speed/`gps_fix=false`/`updated_at`), jalankan deteksi tamper, **skip** geofence dan insert `telemetry_logs`.

**Pengujian:** T05, T06, T08, T09, T10, T11, T23, T26 (T26 deferred).

**Kriteria selesai:** 10/10 payload valid; dashboard update ≤ 10 detik; tanpa fix tetap ada data IMU/lock; tidak ada error SQL `NOT NULL`.

---

### M4 — Command downlink dan aktuator

**Tujuan:** perintah backend benar-benar menggerakkan relay, buzzer, dan LCD.

**File:** `command_handler.h/.cpp`, `outputs.h/.cpp`, `mqtt_link.h/.cpp`

**Command yang wajib didukung:** `UNLOCK`, `LOCK`, `ALARM_ON`, `ALARM_OFF`, `LOCK_AND_ALARM`, `WARNING_BEEP`, `EXTEND_TIME`, `SEND_SMS` (masuk antrean).

**Langkah:**

1. Parse JSON dengan ArduinoJson; validasi `action` dan opsional `duration`/`message`/`phone`.
2. Dispatch ke handler; abaikan duplikat berdasarkan `(action, counter)` bila tersedia.
3. Alarm state machine: nyala → auto-off setelah `ALARM_DURATION_MS` atau `ALARM_OFF`.
4. Buzzer non-blocking: gunakan `millis()`, bukan `delay()` seperti `beep()` baseline.
5. Update LCD: `TERKUNCI`, `TERBUKA`, `ALARM`, `PERINGATAN`, `PERPANJANG`.
6. Setelah eksekusi, segera publish telemetri agar backendverifikasi.

**Pengujian:** T12–T18, T24.

**Kriteria selesai:** respons < 2 detik; `ALARM_OFF` mematikan sirene; command ganda tidak merusak state.

---

### M5 — SMS fallback SIM800L

**Tujuan:** `SEND_SMS` berjalan tanpa memblokir loop.

**File:** `sms_service.h/.cpp`, `command_handler.h/.cpp`

**Langkah:**

1. Antrean SMS berukuran `SMS_QUEUE_SIZE`.
2. State machine AT: `AT` → `ATE0` → `AT+CPIN?` → `AT+CMGF=1` → `AT+CMGS` → prompt `>` → teks → `Ctrl+Z` → tunggu `OK/ERROR`.
3. Sanitasi nomor (digit `+`/`0–9`) dan teks ASCII; panjang pesan dibatasi.
4. Retry terbatas, log ke Serial dengan prefix `[SMS]`.
5. Opsional: SMS lokal saat tamper dan MQTT putus (**tidak** menggantikan jalur utama).

**Pengujian:** T19, T23.

**Kriteria selesai:** SMS diterima < 30 detik; loop tidak stall; kegagalan tercatat di log. Jika SIM belum aktif → status `BLOCKED`, bukan `PASS`.

---

### M6 — Integrasi alur bisnis end-to-end

**Tujuan:** alur aplikasi berjalan dengan hardware.

**Skenario:**

1. Midtrans sandbox `settlement` → `UNLOCK`.
2. Timer sisa ≤ 5 menit → `WARNING_BEEP`.
3. Waktu habis → `LOCK_AND_ALARM` + insiden `OVERDUE`.
4. Selesai sewa → `LOCK`.
5. Gerakan saat terkunci → `TAMPER` + `ALARM_ON` + notifikasi.
6. Keluar geofence → `GEOFENCE` + `ALARM_ON`.
7. Admin force lock/unlock/alarm off via dashboard.

**Pengujian:** T20–T23.

**Kriteria selesai:** setiap skenario punya bukti: log Serial, log backend, screenshot dashboard, dan query DB. Jika Midtrans sandbox tidak valid, pakai admin force unlock dan nyatakan sebagai substitusi, bukan pembayaran sukses.

---

### M7 — Reliability, konsistensi, dan validasi akhir

**Tujuan:** sistem pulih dari gangguan dan keterbatasan terdokumentasi.

**Skenario:**

1. Putus Wi-Fi → pulih.
2. Restart Mosquitto → subscribe ulang.
3. Restart backend → data Redis/MySQL konsisten.
4. Kirim command duplikat QoS 1.
5. Data stale > 30 detik → UI menandai.
6. Baterai lemah (jika hardware sudah ada).
7. Reboot device saat sewa → default terkunci.

**Pengujian:** T03, T04, T24, T25, T26, T27.

**Kriteria selesai:** reconnect pulih; tidak ada aktuator "nyangkut"; tidak ada data live palsu; semua keterbatasan (battery N/A, tanpa ACK) dicatat.

---

## 5. Matriks test case

Aturan pengisian:

- `Actual` → output/log/screenshot nyata. **Dilarang** mengisi "berhasil" tanpa bukti.
- `Status`: `PASS`, `FAIL`, `NOT RUN`, `BLOCKED`, `DEFERRED`, atau `REPORTED` (dilaporkan user, belum diverifikasi agent).

### 5.1 Baseline & lingkungan (M1)

| ID | Tujuan | Langkah | Input | Expected | Actual | Status |
|---|---|---|---|---|---|---|
| B01 | Backup baseline | Salin `.ino`, hitung SHA-256 | File 10737 byte | Backup identik | `4910CDB3...FE48`, 10737 byte | PASS |
| B02 | Kesehatan backend | `GET /health` | `127.0.0.1:3000` | `status success`, MQTT connected | `{"status":"success","redis_status":"OK","mqtt_connected":true}` | PASS |
| B03 | Port service | `netstat -ano` | 1883,3000,3306,6379 | Semua LISTENING | `0.0.0.0:1883/3000/3306`, `0.0.0.0:6379` | PASS |
| B04 | Akses LAN | `Test-NetConnection` | `10.3.24.125:1883`, `:3000` | True | True / True | PASS |
| B05 | Firewall | `Get-NetFirewallRule` | inbound | Node + mosquitto Allow | Keduanya Allow, Public, Enabled | PASS |
| B06 | Library Arduino | Unduh + ekstrak ZIP | 3 repo GitHub | Header tersedia | `TinyGPSPlus.h`, `PubSubClient.h`, `ArduinoJson.h` | PASS |
| B07 | Menu tes existing | Upload baseline, kirim `a`,`1`–`7`,`s`,`h` | Serial 115200 | Semua perintah merespons | Belum dijalankan agent | NOT RUN |
| B08 | Board ESP32 | Cek paket board | Arduino IDE | esp32 3.3.12 terpasang | `esp32/3.3.12` ditemukan | PASS |

> **Catatan B07:** user sebelumnya melaporkan tes komponen berhasil. Karena agent belum melihat log Serial langsung, statusnya sengaja tetap `NOT RUN`, bukan `PASS`.

### 5.2 Integrasi (M2–M7)

| ID | Tujuan | Langkah | Input | Expected | Actual | Status |
|---|---|---|---|---|---|---|
| T01 | Boot aman | Nyalakan device | `config.h` terisi | Relay OFF, buzzer LOW, log boot | — | NOT RUN |
| T02 | WiFi/MQTT connect | Hubungkan AP + broker | SSID benar | Terhubung < 15 dtk | — | NOT RUN |
| T03 | Reconnect WiFi | Matikan AP lalu hidupkan | Gangguan 30 dtk | Pulih otomatis | — | NOT RUN |
| T04 | Reconnect broker | Restart Mosquitto | Restart service | Subscribe ulang | — | NOT RUN |
| T05 | Telemetri GPS fix | Bawa outdoor | Fix ≥ 3 satelit | Redis + MySQL terisi | — | NOT RUN |
| T06 | Telemetri tanpa fix | Demo indoor | `gps_fix=false` | IMU/lock tetap update, tidak error SQL | — | NOT RUN |
| T07 | Akurasi baterai | Bandingkan multimeter | Divider terpasang | Error ≤ 5% | — | DEFERRED |
| T08 | IMU saat locked | Goyang saat terkunci | Gerakan > threshold | `mpu_motion=true`, insiden TAMPER | — | NOT RUN |
| T09 | IMU saat unlocked | Goyang saat terbuka | Gerakan > threshold | `mpu_motion=true`, bukan TAMPER | — | NOT RUN |
| T10 | Redis live hash | Kirim telemetri | Payload valid | Field lengkap + `updated_at` | — | NOT RUN |
| T11 | MySQL log | Kirim telemetri dengan fix | Payload valid | Baris `telemetry_logs` bertambah | — | NOT RUN |
| T12 | `UNLOCK` | Command MQTT/admin | Action UNLOCK | Relay ON, LCD TERBUKA | — | NOT RUN |
| T13 | `LOCK` | Command MQTT/admin | Action LOCK | Relay OFF, LCD TERKUNCI | — | NOT RUN |
| T14 | `ALARM_ON` | Command | Action ALARM_ON | Buzzer nyala | — | NOT RUN |
| T15 | `ALARM_OFF` | Command | Action ALARM_OFF | Buzzer mati | — | NOT RUN |
| T16 | `LOCK_AND_ALARM` | Timer habis | Action LOCK_AND_ALARM | Kunci + sirene 60 dtk | — | NOT RUN |
| T17 | `WARNING_BEEP` | Sisa ≤ 5 menit | Action WARNING_BEEP | 3 beep + LCD | — | NOT RUN |
| T18 | `EXTEND_TIME` | Perpanjangan | Action + deadline | Deadline lokal update, LCD | — | NOT RUN |
| T19 | `SEND_SMS` | Command SMS | Nomor aktif | SMS diterima < 30 dtk | — | BLOCKED |
| T20 | Payment E2E | Midtrans sandbox | Settlement | Unlock otomatis | — | NOT RUN |
| T21 | Timer expiry | Tunggu sewa habis | Sesi aktif | Lock + alarm + insiden | — | NOT RUN |
| T22 | Geofence | Pindah koordinat | Di luar 500 m | Insiden + alarm | — | NOT RUN |
| T23 | Tamper | Gerak saat locked | `mpu_motion=true` | Insiden + notifikasi | — | NOT RUN |
| T24 | Duplicate command | Kirim 2× QoS 1 | Action sama | Tidak ada efek ganda | — | NOT RUN |
| T25 | Backend restart | Restart Node | Service restart | Data tetap konsisten | — | NOT RUN |
| T26 | Low battery | Baterai ≤ 20% | Hardware tersedia | `battery_low=true` | — | DEFERRED |
| T27 | Reboot saat sewa | Reset ESP32 | Sesi aktif | Default terkunci + terdokumentasi | — | NOT RUN |

---

## 6. Definition of Done fase lokal

Integrasi dinyatakan selesai hanya jika:

1. ESP32 terhubung broker dan stabil ≥ 30 menit.
2. Telemetri GPS/IMU/lock masuk Redis dan MySQL.
3. Dashboard menampilkan data live dan menandai stale > 30 detik.
4. `UNLOCK`, `LOCK`, `ALARM_ON`, `ALARM_OFF`, `LOCK_AND_ALARM`, `WARNING_BEEP`, `EXTEND_TIME` dieksekusi firmware.
5. Unlock terjadi lewat pembayaran sandbox **atau** admin force unlock (disebut secara eksplisit).
6. Timer habis mengunci + alarm.
7. Geofence dan tamper memicu insiden + alarm.
8. Wi-Fi/MQTT putus lalu pulih otomatis.
9. Fitur existing backend/frontend tidak regressi.
10. Semua test case yang relevan memiliki `Actual` dan `Status`.

**Tidak termasuk DoD:** baterai nyata (`N/A`), ACK/retry command, timer lokal/NVS, TLS/MQTT auth. Semuanya dicatat sebagai keterbatasan.

---

## 7. Risiko dan mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| IP PC berubah (DHCP) | ESP32 gagal connect | DHCP reservation; `MQTT_BROKER_HOST` di `config.h` |
| AP isolation Wi-Fi kampus | Device tidak bisa ke PC | Uji B04; fallback hotspot PC |
| GPS tidak fix indoor | Telemetri dibuang backend | Patch M3 no-fix |
| `delay()` pada `beep()`/`kirimSMS()` | Watchdog reset | State machine berbasis `millis()` |
| Relay salah polaritas | Kunci terbalik | Konfirmasi `RELAY_ON LOW` / `RELAY_OFF HIGH`; uji T12/T13 |
| Alarm menyala terus | Gangguan | Auto-off 60 detik + `ALARM_OFF` |
| `SENT` dianggap eksekusi | Status palsu | Verifikasi lewat telemetri berikutnya; ACK ditunda |
| `telemetry_logs` tumbuh cepat | DB besar | Interval 5 dtk untuk demo; evaluasi throttling setelah M7 |
| Kredensial bocor | Keamanan | `config.h` tidak di-commit; tidak dikirim lewat chat |

---

## 8. Kontrol perubahan

Setiap kali milestone selesai:

1. Perbarui checklist milestone di dokumen ini.
2. Isi `Actual` dan `Status` pada matriks test.
3. Tambahkan baseline baru di `baseline/` jika perubahan besar.
4. Laporkan ke user dan **tunggu konfirmasi** sebelum milestone berikutnya.
5. Jika ada perubahan kontrak data/schema, catat di `konteks.md` dan minta persetujuan eksplisit.
