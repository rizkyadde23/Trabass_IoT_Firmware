# Perencanaan Integrasi Firmware — Trabass IoT

**Lokasi dokumen:** `d:\Code\ARDUINO\Praktikum IOT\Trabass-IoT-Firmware\perencanaan.md`
**Versi:** 1.7 — **Tanggal:** 2026-11-19
**Status saat ini:** M1–M4 selesai. **M5 diimplementasikan lalu disupersede**: layanan SMS dihapus karena user memprioritaskan internet GPRS. **M6A (WiFi + GPRS via TinyGSM, broker publik HiveMQ) selesai secara fungsional**: flash 966.008 B / RAM 52.400 B ter-upload; `AT→OK`, `CREG 0,1`, `CIFSR` IP, MQTT/telemetri/command via GPRS terbukti; failover otomatis 30 dtk + pemulihan WiFi PASS (T37–T40, T42). T41 `PARTIAL` (endurance 30 menit belum). T19/T31–T36 tidak dijalankan lagi. M6B (alur bisnis) dan M7 masih terbuka.
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
| P1 | Jalankan di PC lokal dulu | MQTT ke `10.20.226.246:1883`, bukan cloud |
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
5. Verifikasi IP LAN (M1: `10.3.24.125`, M2 hotspot: `10.20.226.246`) untuk port 1883 dan 3000.
6. Verifikasi firewall inbound Node.js dan mosquitto.
7. Tulis kontrak telemetri, wiring, dan checklist baseline ke `konteks.md`.
8. Tulis struktur folder ke `struktur.md`.

**Kriteria selesai M1:**

- [x] Backup baseline memiliki SHA-256 terekam
- [x] Ketiga library terinstal di sketchbook library
- [x] `/health` sukses, MySQL/Redis/MQTT OK
- [x] Port 1883/3000/3306/6379 listen dan firewall mengizinkan
- [x] `10.20.226.246:1883` dan `:3000` reachable dari PC (M1 sempat `10.3.24.125`)
- [x] `config.example.h` + `config.h` terbentuk
- [x] Tiga dokumen perencanaan/konteks/struktur terbentuk
- [x] User mengisi `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_BROKER_HOST` di `config.h` (diverifikasi M2: ESP32 connect)
- [x] User menjalankan menu tes baseline `a`, `1`–`7`, `s` (dilaporkan berhasil; agent memverifikasi ulang `1`–`6`, `s`, `h` pada M2)

**Status M1:** dokumentasi dan verifikasi lingkungan selesai; kredensial sudah diisi dan konektivitas terbukti pada M2 (WiFi + MQTT).

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

**Hasil M2 (2026-11-16):**

- Kompilasi `arduino-cli` exit 0; 936.060 byte flash (71%), RAM global 49.776 byte (15%). Tidak ada error/warning pada kode sketch (hanya peringatan deprecated library `LiquidCrystal_I2C`).
- Boot: `POWERON_RESET` → `[BOOT] Trabass fw=1.0.0-m2 bike=BK-001` → relay OFF / buzzer LOW.
- WiFi connect **1,09 dtk**, MQTT connect **1,88 dtk** (target < 15 dtk).
- Heartbeat tiap 5 dtk: `type=heartbeat`, `fw_version=1.0.0-m2`, RSSI -50…-60 dBm, `lock_state=true`.
- Command `bike/command/BK-001` diterima callback dan masuk antrean: `[MQTT] Command masuk 15 B`, `[CMD] Diterima`.
- Menu regresi: `n`, `m`, `h`, `s` OK; `1` LCD OK, `2` buzzer jalan, `3` relay ON→OFF (state akhir terkunci), `4` MPU OK, `5` GPS FIX 21 satelit, `6` SIM800L **tidak membalas**.
- Reconnect terbukti (2026-11-16): hotspot dimatikan ±30 dtk → `[WIFI] Koneksi terputus`, retry tiap 10 dtk, WiFi pulih ±1 dtk setelah hotspot tersedia, MQTT connect 5 dtk kemudian, heartbeat publish-on-connect muncul (jeda heartbeat 38,3 dtk). Command JSON valid diterima setelah reconnect → subscribe terpasang ulang.
- Catatan kosmetik: ESP-IDF mencetak `E wifi:sta is connecting, return error` saat `WiFi.reconnect()` dipanggil ketika stack masih connecting; koneksi tetap berhasil. Tidak mengubah perilaku.
- SIM800L **tidak membalas AT** (5 percobaan) → wiring/power perlu dicek sebelum M5; T19 tetap BLOCKED.

**Status M2:** selesai dan terverifikasi (T03 PASS, T04 PARTIAL karena restart service tidak dilakukan; jalur subscribe-on-connect terbukti). Menunggu konfirmasi user sebelum M3.

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

**Status M3:** implementasi selesai dan diuji. Firmware produksi `1.0.0-m3` terpasang terakhir di COM14; backend lokal berjalan dengan `npm.cmd run dev` (nodemon). Telemetri fix, no-fix, Redis, MySQL, dan dashboard terbukti; dua uji memakai instrumentasi karena threshold gerak dan koordinat ruang uji tidak memungkinkan reproduksi kondisi normal.

**Bukti utama:**

- `m3_compile_final.log`: exit 0, 939.744 B (71%), RAM 49.824 B (15%).
- `m3_upload_final.log`: hash verified, COM14.
- `m3_final_mqtt.log`: payload fix 156–157 B, 18–22 satelit, interval ~5 dtk.
- `m3_nofix.log`: payload firmware `lat/lng:null`; `telemetry_logs` 116→116; koordinat `bikes` tidak berubah; Redis `HEXISTS lat/lng=0`; recovery `=1`.
- `m3_tamper.log`, `m3_unlocked.log`: TAMPER id 9 dan 10, alert `notified=true`.
- `m3_backend.log`/`m3_backend.err.log`: tidak ada `[TELEMETRY ERROR]` atau `NOT NULL`.
- Dashboard API: `has_live_location=true`, `lock_state=true`, `mpu_motion=false`, `battery_percent=null`.

**Keterbatasan yang dicatat:**

- T08/T23 memakai build override `-DIMU_MOTION_ACCEL_THRESHOLD_G=0.001f -DIMU_MOTION_GYRO_THRESHOLD_DPS=9999.0f`. Gerakan fisik dengan threshold produksi (0.35 g / 40 dps) belum diuji.
- T09 memakai payload synthetic `lock_state:false,mpu_motion:true` tanpa koordinat, bukan gerakan fisik.
- Resolusi TAMPER otomatis setelah 120 dtk **tidak berjalan selama geofence masih violated** karena `handleTelemetry` sudah `return` di blok alert. Insiden 10 ditutup lewat payload unlock (19:02:08). Ini perilaku backend lama, bukan regresi patch M3; perlu keputusan user bila ingin diperbaiki.
- GPS ruang uji `-7.78243,110.41583` berjarak ~3,49 km dari pangkalan lama `STATION_LAT=-7.773512`, `STATION_LNG=110.385411` (radius 500 m), sehingga semua telemetri fix memicu `GEOFENCE_VIOLATION`.
- **Keputusan M4:** pangkalan diubah ke Kampus 1 UPN Veteran Yogyakarta `STATION_LAT=-7.7626945`, `STATION_LNG=110.4095241` dan `GEOFENCE_RADIUS_M=5000`. Lokasi uji ~2,30 km dari titik baru → di dalam batas aman. Alert Redis `bike:BK-001:alert` otomatis `active=false` pada restart backend 08:08:09 dengan `resolved_status=OVERDUE`; tidak ada log `GEOFENCE_VIOLATION` baru setelahnya.
- Baterai `DEFERRED` (hardware belum ada); SMS T19 `BLOCKED` (SIM800L belum membalas `AT`).

---

### M4 — Command downlink dan aktuator

**Tujuan:** perintah backend benar-benar menggerakkan relay, buzzer, dan LCD.

**File:**

- Baru: `command_handler.h/.cpp`, `outputs.h/.cpp`
- Diubah: `mqtt_link.cpp` (antrean diserahkan ke handler), `sketch_nov16a.ino`, `self_test.cpp` (`toggleRelay()` delegate ke `outputs`), `telemetry.cpp` tidak diubah (pin relay tetap sumber `lock_state`), `config.example.h`, backend `.env` (geofence)

**Command yang didukung:** `UNLOCK`, `LOCK`, `ALARM_ON`, `ALARM_OFF`, `LOCK_AND_ALARM`, `WARNING_BEEP`, `EXTEND_TIME`, `SEND_SMS` (log `PENDING_M5`), unknown → log + abaikan.

**Langkah yang diterapkan:**

1. Parse JSON ArduinoJson v7; validasi `action`; baca opsional `timestamp`, `message`, `duration_ms`, `added_minutes`, `end_time`, `target_phone`.
2. Dedupe ringan 4 slot berdasarkan `action|timestamp` dalam `COMMAND_DEDUPE_WINDOW_MS` (default 3000 ms).
3. `outputs` menjadi pemilik tunggal relay/buzzer/LCD. Setter absolut (idempoten), alarm auto-off, beep 3 pulse non-blocking, pesan LCD otomatis revert.
4. Alarm tidak diperpanjang oleh `ALARM_ON` berulang selama masih aktif.
5. Setiap command yang dieksekusi segera memanggil `telemetry::publishNow()` (force publish QoS 0).
6. Menu tes serial `3` kini memanggil `outputs::setLock()` sehingga state tidak desinkron.

**Pengujian:** T12–T18, T24, T30 + regresi T01/T05/T10/T11 dan menu serial.

**Kriteria selesai:** respons < 2 detik; `ALARM_OFF` mematikan sirene; command ganda tidak merusak state.

**Actual ringkas (2026-11-19):**

- Build override `ALARM_DURATION_MS=3000` → upload COM14 sukses; build produksi 60 dtk → upload ulang COM14 sukses.
- `fw_version=1.0.0-m4`; flash 950.420 B (72%), RAM 50.328 B (15%).
- T12/T13: `UNLOCK` → relay ON/pin LOW/`lock_state:false`; `LOCK` → relay OFF/pin HIGH/`lock_state:true`; telemetri force publish langsung muncul.
- T14/T15/T16: `ALARM_ON` 3000 ms → auto-off; `ALARM_OFF` mematikan sebelum auto-off; `LOCK_AND_ALARM` menyisakan keadaan terkunci setelah alarm mati. Build produksi diverifikasi `ALARM ON selama 60000 ms`.
- T17/T18: `[LCD] Pesan ... 'PERINGATAN' | 'Ada gerakan menc'`, `3 pulse` beep; `PERPANJANG | Ditambah 15 mnt`, `added_minutes=15 end_time=...` terbaca.
- T24: dua `LOCK` identik berturut-turut → yang kedua `[CMD] Duplikat dalam 3000 ms, diabaikan`.
- T30: admin dashboard `FORCE_UNLOCK`/`FORCE_LOCK` → `admin_command_logs` id 17/18 `SENT`, firmware mengeksekusi keduanya, telemetri `lock_state` mengikuti.
- Regresi menu serial `o`, `3`, `n`: state relay sinkron (`pin=0` saat terbuka, `pin=1` saat terkunci), WiFi/MQTT `MQTT_CONNECTED`.

**Status M4:** SELESAI untuk logika/state; bunyi buzzer fisik dan tampilan LCD masih perlu konfirmasi manual (agent hanya membuktikan pin, state machine, dan log).

---

### M5 — SMS fallback SIM800L

**Tujuan:** `SEND_SMS` benar-benar mengantre dan mengirim SMS lewat SIM800L tanpa memblokir loop.

**File:**

- Baru: `sms_service.h/.cpp`
- Diubah: `command_handler.cpp` (`SEND_SMS` → `queueSms()`), `mqtt_link.cpp` (slot command 192→512 B), `sketch_nov16a.ino` (begin/loop + mode manual AT), `self_test.h/.cpp` (menu `8` diagnostik baud), `config.example.h` (`FW_VERSION=1.0.0-m5`, konstanta M5). Backend dan frontend **tidak diubah**.

**Langkah yang diterapkan:**

1. Antrean `SMS_QUEUE_SIZE=4`; command handler memanggil `sms_service::queueSms(target_phone, message)` (fallback field `phone` untuk kompatibilitas). Payload ditolak bila nomor/pesan invalid atau antrean penuh, tetapi telemetri tetap dipublish.
2. State machine non-blocking: `AT` → `ATE0` → `AT+CPIN?` → `AT+CREG?` → `AT+CMGF=1` → `AT+CMGS="nomor"` → prompt `>` → teks + `Ctrl+Z` → tunggu `OK/ERROR`. Tidak ada `delay()` di jalur otomatis.
3. Sanitasi: nomor hanya `+`/digit, `+62…`/`62…` dinormalisasi ke `08…`, 9–15 digit; teks ASCII printable maks. 140 char (non-ASCII/CR/LF/Ctrl+Z → `?`) sehingga injeksi AT tidak mungkin.
4. Retry `SMS_MAX_ATTEMPTS=2` per pesan dengan jeda `SMS_RETRY_INTERVAL_MS`; counter direset tiap pesan (bug ini ditemukan dan diperbaiki saat uji antrean).
5. Konflik UART diatasi: `sms_service` pemilik `sim` saat mode auto; perintah `AT...` dan menu `6`/`7`/`a` menangguhkan service dan meneruskan balasan ke Serial hanya selama jendela `SIM_MANUAL_WINDOW_MS`.
6. `ENABLE_LOCAL_TAMPER_SMS=0` (default): SMS lokal saat tamper + offline **tidak** diaktifkan agar tidak duplikat dengan WA/SMS fallback backend.
7. Menu `8` = diagnostik baud SIM (9600/19200/38400/57600/115200) untuk membedakan baud mismatch vs modul mati.

**Pengujian:** T19, T31–T36 + regresi T12/T13/T28.

**Kriteria selesai:** SMS diterima < 30 detik; loop tidak stall; kegagalan tercatat di log. Jika SIM belum aktif → status `BLOCKED`, bukan `PASS`.

**Actual ringkas (2026-11-19):**

- Build produksi `1.0.0-m5` sukses: flash 954.280 B (72%), RAM 52.464 B (16%), upload COM14 sukses.
- `m5_queue_tests.log`: payload valid masuk antrean (`pending=1`), 2 attempt gagal (`AT tidak merespons`), antrean dibuang, telemetri tetap tiap 5 dtk.
- `m5_sanitize.log`: payload 350 B terbaca utuh setelah buffer diperbesar ke 512 B; `+62 878-6688-7578` → `087866887578`; non-ASCII → `?`; teks dipotong 140 char; string `AT+CMGS=dummy` di dalam pesan tidak dieksekusi.
- `m5_queue_full_final.log`: 5 command berturut-turut → 4 diterima (`pending=1..4`), ke-5 `[SMS] Antrean penuh, pesan ditolak`; tiap pesan mendapat `Attempt 1/2` dan `2/2` (reset terverifikasi); telemetri 5 dtk tidak berhenti.
- `m5_boot_sim.log` + `m5_baud_diag.log`: menu `6` gagal setelah 5× `AT`; diagnostik menu `8` tidak mendapat balasan di kelima baud.
- `m5_regression.log`: menu `n` menampilkan `[SMS] state=IDLE pending=0 busy=tidak`; raw `AT` tidak mengganggu loop; `UNLOCK`/`LOCK`, LCD, dan publish 0 gagal tetap normal.

**Status M5:** **DISUPERSEDE oleh M6A (2026-11-19).** Bukti software T33–T36 tetap disimpan sebagai history, tetapi tidak lagi dipakai sebagai kriteria selesai. `sms_service.h/.cpp`, `NOMOR_TUJUAN`, menu `7`/`8`, dan seluruh jalur `SEND_SMS` dihapus. T19/T31–T36 dinyatakan `RETIRED`, bukan `PASS`/`FAIL` baru. Alasan: UART2 dipakai penuh oleh TinyGSM untuk GPRS, dan pengiriman SMS nyata tidak pernah terbukti (modem tidak merespons `AT`).

---

### M6A — Internet GPRS SIM800L via TinyGSM (menggantikan M5)

**Tujuan:** WiFi tetap utama; saat WiFi putus, MQTT (telemetri + command) otomatis memakai GPRS SIM800L. SMS tidak ada lagi.

**Keputusan kunci:**

- Modem: SIM800L v2 EVB tanpa PWRKEY, self-power. APN by.u/Telkomsel `internet`, user/password kosong.
- Wiring final yang dipakai firmware (sudah ditukar user di hardware): **TXD modul → GPIO16 (RX ESP32)**, **RXD modul ← GPIO17 (TX ESP32)**, **VDD → 3V3 ESP32**, GND bersama. GPS NEO-6M memakai **GPIO26/27**. Tidak ada `SIM_PWRKEY_PIN`. Suplai 5 V modul **wajib adaptor terpisah ≥ 2 A**; adaptor 5 V/3 A sudah dipasang user pada uji final (sebelumnya dari VIN ESP32 dan memicu `UNDER-VOLTAGE WARNING`).
- Broker: `broker.hivemq.com:1883` (IP LAN tidak reachable dari GPRS). Topik namespaced: `trabass/<BIKE_ID>/telemetry`, `trabass/<BIKE_ID>/command`, `trabass/<BIKE_ID>/ack`, `trabass/<BIKE_ID>/status`.
- Failover: `WIFI_FAILOVER_AFTER_MS=30000`, kembali ke WiFi setelah `WIFI_RESTORE_STABLE_MS=15000`. Radio WiFi dimatikan sementara saat GPRS dicoba (`GPRS_WIFI_PROBE_MS=30000`), lalu dipole-ulang untuk mendeteksi pemulihan; ini hasil diagnosis karena aktivitas WiFi mengganggu UART2 bila 5 V masih bersama VIN ESP32.
- MQTT: publish QoS 0 (batasan `PubSubClient`), subscribe QoS 1, buffer 512 B, LWT masih mati.
- Dua instance `PubSubClient` (`mqttWifi(wifiClient)`, `mqttGsm(gprs_link::client())`) karena `PubSubClient` menyimpan `Client&` saat konstruksi. Callback dan antrean command sama.

**File:**

- Baru: `gprs_link.h/.cpp` (pemilik tunggal UART2, `TinyGsm`, `TinyGsmClient`, state machine attach, `debugAt()`, `swapPins()`, `scanUart()`).
- Diubah: `mqtt_link.h/.cpp` (dual transport + failover + `transportLabel()`/`gprsIp()`/`gprsAttached()`), `sketch_nov16a.ino` (menu `6`, `9`, `0`, hapus manual-window SMS), `command_handler.cpp` (`SEND_SMS` → diabaikan), `self_test.h/.cpp` (hapus `tesSIM`/`diagBaudSIM`/`kirimSMS`), `app_types.h` (state/counter GPRS), `config.example.h` + `config.h` (versi `1.0.0-gprs`, konstanta GPRS, broker/topik baru).
- Dihapus: `sms_service.h/.cpp`.
- Backend: `.env` → `MQTT_BROKER_URL=mqtt://broker.hivemq.com:1883`; `src/config/mqtt.js` subscribe `trabass/+/telemetry`; `src/app.js` filter `trabass/…/telemetry`; topik command di `paymentController.js`, `timerService.js`, `bikeCommandService.js`, `telemetryController.js`; `notificationService.js` fallback WA gagal menjadi log-only. Frontend tidak diubah.

**Langkah yang diterapkan:**

1. `gprs_link::begin()` hanya menyiapkan UART2 dan masuk `IDLE`. Attach dimulai saat failover atau menu `6`, sehingga WiFi normal tidak mengganggu modem.
2. State machine per langkah: `PROBE(AT)` → `INIT(modem.begin())` → `SIM_READY(CPIN?)` → `SIGNAL(CSQ)` → `REGISTER(CREG?/COPS?)` → `ATTACH(gprsConnect)` → `GET_IP(CIFSR)` → `ONLINE`. Satu langkah AT bisa memblokir beberapa detik (keterbatasan TinyGSM) tetapi hanya saat transisi; retry tiap `GPRS_ATTACH_INTERVAL_MS=10000`.
3. `swapPins()` (menu `9`) menukar `begin(...,16,17)` ↔ `(...,17,16)` saat runtime lalu mengirim `AT`, tanpa re-solder. Terminator AT default sudah CRLF (kode tester user yang berhasil memakai CRLF); `scanUart()` tetap menguji CR dan CRLF. Baud modem diubah permanen dari autobaud (`+IPR:0`) ke **9600 tetap** lewat passthrough, lalu diverifikasi `+IPR: 9600`.
4. `scanUart()` (menu `0`) menguji 2 mapping × 8 baud (1200–115200) × terminator `CR`/`CRLF` untuk membedakan baud mismatch vs modul mati.
5. `mqtt_link::loop()` menjalankan `gprs_link::loop()`, `ensureWifi()`, `ensureGprs()`, `ensureMqtt()`, lalu `loop()` pada instance aktif. Saat pindah transport, timer reconnect direset dan sesi TCP GPRS dilepas saat kembali ke WiFi.
6. Backend tetap menerima telemetri lama `bike/telemetry/<id>` (toleran), tetapi command hanya dikirim ke topik baru.

**Pengujian:** T37–T42 + regresi T12/T13/T28 via WiFi.

**Kriteria selesai:** `AT→OK`, attach GPRS dapat IP, telemetri publish dan command downlink lewat GPRS, failover WiFi→GPRS→WiFi terbukti, stabil 30 menit. Jika modem tidak merespons → `BLOCKED`, bukan `PASS`.

**Actual ringkas (2026-11-19, setelah swap pin hardware + adaptor 5 V/3 A):**

- Compile produksi final sukses: flash **966.008 B (73%)**, RAM **52.400 B (15%)**; upload COM14 sukses. Versi `1.0.0-gprs`.
- Diagnostik pin lama (26/27) disimpan sebagai history: `m6_uart_scan.log` 32 kombinasi kosong. Setelah user menukar kabel (SIM 16/17, GPS 26/27), `m6_at_newpins.log`: `AT -> ATOK`, `AT+IPR? -> +IPR: 0` (autobaud), `AT+CREG? -> +CREG: 0,2`.
- `m6_sim_diag.log`: `CPIN: READY`, `CCID 8962100833903268361` (Telkomsel), `CSQ: 0,0`, `CREG: 0,2`, `COPS: 0`, `CGATT: 0`, `CIFSR ERROR`. `m6_manual_operator.log`: pemilihan operator manual `51010` → `ERROR`; tetap `CREG 0,2` selama 95 dtk.
- `m6_passthrough_test.log` (sketch passthrough sementara, pin 16/17, 9600): `AT -> OK`, `Call Ready`, `CSQ 0,0`, `CREG 0,2`. `m6_set_fixed_baud.log`: `AT+IPR=9600` lalu `+IPR: 9600` (tersimpan). Jadi wiring/level/UART2 modul **tidak bermasalah**.
- `m6_gprs_only_test.log` (hanya `gprs_link`): `AT -> ATOK`, `CREG 0,2`. `m6_gprs_wifi_test2.log` (ditambah `WiFi.begin()`): balasan `.hex` menjadi `7D 57 AB 7B…` (garbage). `m6_gprs_wifioff_test.log` (WiFi dimatikan): `AT -> ATOK`. **Akar gangguan AT di firmware utama = aktivitas WiFi saat 5 V modem masih di VIN ESP32**, bukan baud/terminator/kabel. Firmware sudah diperbaiki dengan `parkWifiForGprs()` + probe WiFi berkala + `gprs_link::cancel()` saat WiFi pulih.
- `m6_gprs_wifioff_main2.log`: menu `6` mematikan WiFi, `AT -> ATOK`, `CPIN READY`, `CSQ 0,0`, `CREG 0,0`; state machine lanjut ke `SIM_READY` lalu retry karena `reg=2`. WiFi probe berhasil pulih dan transport kembali WiFi; telemetri publish 0 gagal.
- `m6_failover_final.log` (SSID uji sengaja salah): `WiFi putus 30000 ms, failover ke GPRS`, radio WiFi dimatikan, `SIM READY`, `CSQ=16` sempat muncul, tetapi `Belum terdaftar jaringan (reg=2)` terus berulang; probe WiFi gagal lalu radio dimatikan lagi. Failover terpicu tepat 30 dtk, state `GPRS_ATTACHING`, loop tidak stall.
- `m6_long_registration.log` (200 dtk): `CPIN READY`, `CSQ 0,0` dominan, `CREG 0,2` menetap, `CGATT 0`, dan modul mengirim URC **`UNDER-VOLTAGE WARNING`**. Ini buku langsung bahwa suplai 5 V dari VIN ESP32 tidak cukup; cakupan/operator 2G juga belum terbukti.
- `m6_final_cmd3.log` + `GET /health`: regresi final `UNLOCK`/`LOCK` via `trabass/BK-001/command` PASS, `publish=8 gagal=0 cmd=2`, backend `mqtt_connected:true`, subscribe `trabass/+/telemetry`. Tidak ada baris `telemetry_logs` baru karena GPS sedang `gps_fix=false` (backend sengaja tidak insert koordinat null); bukan kegagalan MQTT.
- Alert geofence lama sudah diperbarui di `app_settings` ke `(-7.7626945,110.4095241,5000)`; alert lama `active=false`, `resolved_status=OVERDUE`.

**Bukti final setelah adaptor 5 V/3 A terpasang (2026-11-19):**

- `m6_extpower_check2.log`: status awal `GPRS=attached IP=10.201.214.162 CSQ=13`; `AT -> OK`, `+CPIN: READY`, `+CSQ: 14,0`, `+CREG: 0,1`, `+COPS: 0,0,"TELKOMSEL"`, `+CGATT: 1`, `AT+CIFSR -> 10.201.214.162`. Menu `6` → `[MQTT] Connected clientId=trabass-BK-001-g via GPRS`, telemetri 160 B dengan GPS fix 22 satelit dikirim lewat GPRS. Tidak ada `UNDER-VOLTAGE WARNING` lagi.
- `m6_gprs_cmd_reactive.log`: command `UNLOCK` diterima **via GPRS** → `[OUTPUT] Relay=ON pin=LOW -> TERBUKA`, `[CMD] Eksekusi 'UNLOCK' selesai. state=UNLOCKED`; telemetri lanjutan `lock_state:false`. LOCK masuk via WiFi setelah probe memulihkan WiFi (transport balik WiFi setelah 15 dtk stabil).
- `m6_failover_auto_extpower.log` (SSID sengaja salah, tanpa menu `6`): tepat 30 dtk `[NET] WiFi putus 30000 ms, failover ke GPRS`, `Radio dimatikan`, `SIM READY CSQ=13`, `Terdaftar, operator=TELKOMSEL`, `GPRS AKTIF IP=10.64.205.139`, `Transport -> GPRS`, `MQTT Connected ... via GPRS`. `UNLOCK` **dan** `LOCK` keduanya diterima via GPRS, relay ON→OFF, telemetri `lock_state:false` lalu `true`. WiFi probe gagal selama SSID salah, radio dimatikan lagi; loop tidak stall.
- `m6_failover_extpower_full.log` (menu `6`, SSID salah): `GPRS AKTIF IP=10.66.68.165 CSQ=13`, MQTT via GPRS, `UNLOCK`/`LOCK` via GPRS, telemetri berurutan hingga uptime 138 dtk tanpa `gagal`.
- `telemetry_logs` id **2286–2295** (20:59:11–20:59:51, koordinat `-7.770888,110.415103/105`) mencatat periode failover otomatis; id 2287–2288 `lock_state=0` sesuai UNLOCK via GPRS. Backend `/health` `mqtt_connected:true`, subscribe `trabass/+/telemetry`.
- `m6_final_regression_ext.log`: firmware final dengan SSID asli, `WiFi=ya MQTT=ya state=MQTT_CONNECTED`, `publish=10 gagal=0`, GPS fix 23 satelit, state `LOCKED relay=OFF pin=1`. Compile/upload final sukses (flash 966.008 B, RAM 52.400 B).

**Status M6A:** **PASS untuk T37–T40 dan T42**. Blocker lama (suplai VIN + tidak ada registrasi 2G) sudah hilang setelah adaptor 5 V/3 A dipasang: `CREG 0,1`, `CIFSR` memberi IP, MQTT/telemetri/command lewat GPRS terbukti, failover otomatis 30 dtk dan pemulihan WiFi (15 dtk stabil) terbukti di dua sesi. **T41 tetap PARTIAL** karena uji stabilitas belum dijalankan penuh 30 menit (baru ~3 menit kumulatif dengan 0 publish gagal). **Milestone 6 dapat dinyatakan selesai secara fungsional** dengan keterbatasan: T41 belum 30 menit, ACK perangkat belum ada, broker publik anonymous/TLS belum aktif, bunyi buzzer/LCD fisik belum dikonfirmasi agent.

---

### M6B — Integrasi alur bisnis end-to-end

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
- `Status`: `PASS`, `FAIL`, `NOT RUN`, `BLOCKED`, `DEFERRED`, `REPORTED` (dilaporkan user, belum diverifikasi agent), atau `RETIRED` (fitur dihapus/disupersede; bukti lama tetap arsip, tidak dijalankan ulang).

### 5.1 Baseline & lingkungan (M1)

| ID | Tujuan | Langkah | Input | Expected | Actual | Status |
|---|---|---|---|---|---|---|
| B01 | Backup baseline | Salin `.ino`, hitung SHA-256 | File 10737 byte | Backup identik | `4910CDB3...FE48`, 10737 byte | PASS |
| B02 | Kesehatan backend | `GET /health` | `127.0.0.1:3000` | `status success`, MQTT connected | `{"status":"success","redis_status":"OK","mqtt_connected":true}` | PASS |
| B03 | Port service | `netstat -ano` | 1883,3000,3306,6379 | Semua LISTENING | `0.0.0.0:1883/3000/3306`, `0.0.0.0:6379` | PASS |
| B04 | Akses LAN | `Test-NetConnection` | `10.20.226.246:1883`, `:3000` | True | True / True (hotspot `Galaxy A04e 5371`; IP lama `10.3.24.125` sudah tidak berlaku) | PASS |
| B05 | Firewall | `Get-NetFirewallRule` | inbound | Node + mosquitto Allow | Keduanya Allow, Public, Enabled | PASS |
| B06 | Library Arduino | Unduh + ekstrak ZIP | 3 repo GitHub | Header tersedia | `TinyGPSPlus.h`, `PubSubClient.h`, `ArduinoJson.h` | PASS |
| B07 | Menu tes existing | Upload baseline, kirim `a`,`1`–`7`,`s`,`h` | Serial 115200 | Semua perintah merespons | M2: `n/m/h/s` + `1`–`6` merespons; `7` SMS tidak dijalankan; SIM800L tidak membalas | PARTIAL |
| B08 | Board ESP32 | Cek paket board | Arduino IDE | esp32 3.3.12 terpasang | `esp32/3.3.12` ditemukan | PASS |

> **Catatan B07:** user sebelumnya melaporkan tes komponen berhasil. Karena agent belum melihat log Serial langsung, statusnya sengaja tetap `NOT RUN`, bukan `PASS`.

### 5.2 Integrasi (M2–M7)

| ID | Tujuan | Langkah | Input | Expected | Actual | Status |
|---|---|---|---|---|---|---|
| T01 | Boot aman | Nyalakan device | `config.h` terisi | Relay OFF, buzzer LOW, log boot | M2: `POWERON_RESET`; `[BOOT] fw=1.0.0-m2`; `Relay=OFF (terkunci), buzzer=LOW`. M3: `[BOOT] fw=1.0.0-m3 bike=BK-001`, relay tetap OFF | PASS |
| T02 | WiFi/MQTT connect | Hubungkan AP + broker | SSID benar | Terhubung < 15 dtk | WiFi 1,09 dtk; MQTT 1,88 dtk; RSSI -57 dBm | PASS |
| T03 | Reconnect WiFi | Matikan AP lalu hidupkan | Gangguan 30 dtk | Pulih otomatis | Hotspot off ±30 dtk: WiFi putus t=3,6 dtk; retry tiap 10 dtk; pulih t=34,6 dtk; MQTT t=39,6 dtk; jeda heartbeat 38,3 dtk | PASS |
| T04 | Reconnect broker | Restart Mosquitto | Restart service | Subscribe ulang | Jalur reconnect terbukti (WiFi putus → MQTT connect ulang → command `{"action":"LOCK"}` 17 B diterima). Restart service saja belum dilakukan | PARTIAL |
| T05 | Telemetri GPS fix | Bawa outdoor | Fix ≥ 3 satelit | Redis + MySQL terisi | Payload 156 B, 18–22 satelit (`-7.782435,110.415817`); Redis: lat/lng/speed/lock/mpu/`updated_at`; MySQL `telemetry_logs` + `bikes`; dashboard `has_live_location=true` | PASS |
| T06 | Telemetri tanpa fix | Demo indoor | `gps_fix=false` | IMU/lock tetap update, tidak error SQL | Build `-DGPS_FIX_MAX_AGE_MS=1`: payload `lat/lng:null,gps_fix:false,satellites:0`; log 116→116; koordinat `bikes` tetap; Redis lat/lng terhapus, speed/lock/mpu/`updated_at` ada; fix pulih → lat/lng kembali; tanpa error SQL | PASS (instrumented) |
| T07 | Akurasi baterai | Bandingkan multimeter | Divider terpasang | Error ≤ 5% | — | DEFERRED |
| T08 | IMU saat locked | Goyang saat terkunci | Gerakan > threshold | `mpu_motion=true`, insiden TAMPER | Build override threshold `0.001 g`/`9999 dps`: telemetri `mpu_motion=true`; TAMPER id 10 ACTIVE, `bikes.status=TAMPER_ALERT`, alert `notified=true`; ditutup 19:02:08 via payload unlock. Threshold produksi belum diuji gerak fisik | PARTIAL |
| T09 | IMU saat unlocked | Goyang saat terbuka | Gerakan > threshold | `mpu_motion=true`, bukan TAMPER | Payload synthetic `lock_state:false,mpu_motion:true`: TAMPER id 9 RESOLVED, tidak ada TAMPER baru; status akhir `GEOFENCE_VIOLATION` | PARTIAL (synthetic) |
| T10 | Redis live hash | Kirim telemetri | Payload valid | Field lengkap + `updated_at` | `HGETALL`: `lat=-7.782448`, `lng=110.415797`, `speed=0`, `lock_state=true`, `mpu_motion=false`, `updated_at` terisi | PASS |
| T11 | MySQL log | Kirim telemetri dengan fix | Payload valid | Baris `telemetry_logs` bertambah | 85→87 dalam 12 dtk; baris berurutan tiap 5 dtk (contoh log 97–99, 18:41:12–18:41:22) | PASS |
| T12 | `UNLOCK` | Command MQTT/admin | Action UNLOCK | Relay ON, LCD TERBUKA | `mosquitto_pub -q 1`: `[OUTPUT] Relay=ON pin=LOW -> TERBUKA`, `[LCD] Pesan ... 'TERBUKA'`, telemetri `lock_state:false` langsung terbit (uptime 6443 ms) | PASS |
| T13 | `LOCK` | Command MQTT/admin | Action LOCK | Relay OFF, LCD TERKUNCI | `[OUTPUT] Relay=OFF pin=HIGH -> TERKUNCI`, `lock_state:true` langsung terbit (uptime 14670 ms) | PASS |
| T14 | `ALARM_ON` | Command | Action ALARM_ON | Buzzer nyala | Build override: `[ALARM] ON selama 3000 ms`, `[ALARM] Auto-off setelah 3000 ms`. Produksi: `[ALARM] ON selama 60000 ms`. Bunyi fisik belum dikonfirmasi agent | PASS (instrumented) |
| T15 | `ALARM_OFF` | Command | Action ALARM_OFF | Buzzer mati | alarm dinyalakan 2 dtk sebelumnya → `[ALARM] OFF (command/admin)`, state `LOCKED alarm=OFF` sebelum auto-off. Bunyi fisik belum dikonfirmasi agent | PASS (instrumented) |
| T16 | `LOCK_AND_ALARM` | Timer habis | Action LOCK_AND_ALARM | Kunci + sirene 60 dtk | `[OUTPUT] Lock already TERKUNCI (idempoten)`, `[ALARM] ON selama 3000 ms`, auto-off, telemetri tetap `lock_state:true`. Default produksi 60 dtk diverifikasi di T14 | PASS (instrumented) |
| T17 | `WARNING_BEEP` | Sisa ≤ 5 menit | Action WARNING_BEEP | 3 beep + LCD | `[OUTPUT] Warning beep mulai/selesai (3 pulse)`, `[LCD] Pesan 5000 ms: 'PERINGATAN' \| 'Ada gerakan menc'`. Bunyi fisik belum dikonfirmasi agent | PARTIAL |
| T18 | `EXTEND_TIME` | Perpanjangan | Action + deadline | Deadline lokal update, LCD | `[CMD] EXTEND_TIME added_minutes=15 end_time=2026-11-19T09:30:00.000Z`, `[LCD] Pesan ... 'PERPANJANG' \| 'Ditambah 15 mnt'`. **Deadline lokal/NVS tidak diimplementasikan** sesuai keputusan M4 | PARTIAL |
| T19 | `SEND_SMS` | Command MQTT | Nomor aktif | SMS diterima < 30 dtk | Arsip M5: antrean diterima lalu dua attempt gagal (`AT tidak merespons`). M6: layanan SMS dihapus; command `SEND_SMS` diabaikan firmware | RETIRED |
| T20 | Payment E2E | Midtrans sandbox | Settlement | Unlock otomatis | — | NOT RUN |
| T21 | Timer expiry | Tunggu sewa habis | Sesi aktif | Lock + alarm + insiden | — | NOT RUN |
| T22 | Geofence | Pindah koordinat | Di luar 5000 m | Insiden + alarm | Config diubah: pangkalan Kampus 1 `(-7.7626945,110.4095241)`, radius 5000 m. Alert `active=false` setelah restart; tidak ada violation baru. Uji koordinat jauh sintetis sengaja tidak dijalankan agar tidak membuat insiden/WA baru | PARTIAL (config verified) |
| T23 | Tamper | Gerak saat locked | `mpu_motion=true` | Insiden + notifikasi | TAMPER id 10 dibuat; alert Redis `active=true`, `notified=true`, `notify_error` kosong; WhatsApp terkirim. Lihat keterbatasan T08 | PARTIAL |
| T24 | Duplicate command | Kirim 2× QoS 1 | Action sama | Tidak ada efek ganda | Dua payload `LOCK` identical berturut-turut: yang pertama idempoten (`Lock already TERKUNCI`), yang kedua `[CMD] Duplikat dalam 3000 ms, diabaikan: LOCK\|2026-11-19T08:21:20Z`; hanya satu force telemetri | PASS |
| T25 | Backend restart | Restart Node | Service restart | Data tetap konsisten | — | NOT RUN |
| T26 | Low battery | Baterai ≤ 20% | Hardware tersedia | `battery_low=true` | — | DEFERRED |
| T27 | Reboot saat sewa | Reset ESP32 | Sesi aktif | Default terkunci + terdokumentasi | Reboot M3 → telemetri `lock_state=true`; sesi sewa belum ada | PARTIAL |
| T28 | Heartbeat periodik (M2) | Boot normal, amati `mosquitto_sub` | `bike/telemetry/#` | Interval 5 dtk, payload valid | 8 heartbeat berurutan 5,0 dtk; `fw_version=1.0.0-m2`; RSSI -50…-60; `lock_state=true` | PASS |
| T29 | Command diterima (M2) | Publish ke `bike/command/BK-001` | `{"action":"UNLOCK"}`, lalu `{"action":"LOCK"}` via `-f` | Callback mencatat, aktuator belum bergerak | `[MQTT] Command masuk 15 B`; `[CMD] Diterima: {action:UNLOCK}`; setelah reconnect: `17 B` + `{"action":"LOCK"}` (JSON valid) | PASS |
| T30 | Admin command E2E (M4) | Dashboard admin | `FORCE_UNLOCK`, `FORCE_LOCK` | MQTT → eksekusi → telemetri | `POST /api/admin/bike-command` → `status:success`; `admin_command_logs` id 17/18 `SENT`; firmware `[CMD] Eksekusi 'UNLOCK'/'LOCK'`, `lock_state:false` lalu `true` | PASS |
| T31 | Pra-cek SIM (M5) | Menu serial `6` | Modul aktif | `AT→OK`, `CPIN? READY`, `CSQ`, `CREG` terdaftar | Arsip M5: 5× `AT` tanpa balasan. Digantikan T37 | RETIRED |
| T32 | SMS manual (M5) | Menu serial `7` | `NOMOR_TUJUAN` aktif | SMS tes diterima | Menu `7` dihapus; tidak dijalankan ulang | RETIRED |
| T33 | Antrean penuh & non-blocking (M5) | 5× `SEND_SMS` QoS 1 cepat | `SMS_QUEUE_SIZE=4` | 4 diterima, ke-5 ditolak, telemetri 5 dtk | Arsip M5: `pending=1..4`, ke-5 ditolak, telemetri berlanjut. Antrean SMS dihapus | RETIRED |
| T34 | Sanitasi payload (M5) | Payload 350 B, nomor `+62…`, non-ASCII | Teks ASCII maks 140 | Nomor `08…`, tanpa injeksi AT | Arsip M5: terbukti. Tidak relevan lagi tanpa SMS | RETIRED |
| T35 | Failure path (M5) | `SEND_SMS` dengan modem mati | Timeout 2 dtk | Retry 2× lalu dibuang | Arsip M5: terbukti. Failure path GPRS menggantikan ini | RETIRED |
| T36 | Diagnostik baud & regresi (M5) | Menu `8`, `n`, `AT`, `UNLOCK`/`LOCK` | Semua baud + state aman | Tidak ada balasan = modul mati | Arsip M5: terbukti. Menu `8` digantikan menu `0` (scan 32 kombinasi) | RETIRED |
| T37 | Attach GPRS + IP (M6A) | Menu `6`, amati `PROBE→…→GET_IP` | APN `internet`, SIM aktif, cakupan 2G | `AT→OK`, `CPIN READY`, `CREG` terdaftar, `CIFSR` IP | `m6_extpower_check2.log`: `AT -> OK`, `+CPIN: READY`, `+CSQ: 14,0`, `+CREG: 0,1`, `+COPS: 0,0,"TELKOMSEL"`, `+CGATT: 1`, `+CIFSR -> 10.201.214.162`; `m6_failover_auto_extpower.log`: `GPRS AKTIF IP=10.64.205.139`. Riwayat `m6_long_registration.log` (`CREG 0,2`, `UNDER-VOLTAGE WARNING`) terjadi sebelum adaptor 5 V dipasang | PASS |
| T38 | Telemetri via GPRS (M6A) | Putus WiFi > 30 dtk, publish telemetri | GPRS attached | Payload masuk backend | `m6_failover_auto_extpower.log`: telemetri 159–160 B `gps_fix:true`, 22–23 satelit dikirim via GPRS sampai uptime ~139 dtk; `telemetry_logs` id 2286–2295 (20:59:11–20:59:51, `-7.770888,110.415103/105`) termasuk `lock_state=0` saat UNLOCK. `/health` `mqtt_connected:true` | PASS |
| T39 | Command downlink via GPRS (M6A) | Publish `trabass/BK-001/command` saat WiFi putus | GPRS attached | Firmware eksekusi + telemetri | `m6_failover_auto_extpower.log`: `UNLOCK` dan `LOCK` keduanya `Command masuk ... via GPRS` → `[OUTPUT] Relay=ON pin=LOW -> TERBUKA` lalu `Relay=OFF pin=HIGH -> TERKUNCI`, telemetri `lock_state:false` lalu `true`. `m6_gprs_cmd_reactive.log`: UNLOCK via GPRS PASS | PASS |
| T40 | Failover WiFi→GPRS→WiFi (M6A) | SSID uji sengaja salah, lalu SSID asli | `WIFI_FAILOVER_AFTER_MS=30000` | Log transisi transport, kembali WiFi setelah 15 dtk | `m6_failover_auto_extpower.log`: tepat 30 dtk `WiFi putus 30000 ms, failover ke GPRS` → `Radio dimatikan` → `GPRS AKTIF` → `Transport -> GPRS` → MQTT via GPRS; `m6_gprs_cmd_reactive.log` / `m6_extpower_check2.log`: WiFi probe pulih → `[NET] Transport -> WIFI (WiFi pulih stabil)` setelah 15 dtk stabil, telemetri `gagal=0` | PASS |
| T41 | Stabilitas 30 menit (M6A) | Boot normal 30 menit | Telemetri 5 dtk | Tidak stall, publish 0 gagal | `m6_extpower_check2.log` (~80 dtk), `m6_failover_extpower_full.log` (~140 dtk GPRS), `m6_failover_auto_extpower.log` (~110 dtk GPRS), `m6_final_regression_ext.log` (~57 dtk WiFi): seluruhnya `publish 0 gagal`, GPS/IMU tetap dibaca. 30 menit penuh belum dijalankan | PARTIAL |
| T42 | Regresi aktuator/menu (M6A) | `UNLOCK`, `LOCK`, `n`, `o`, `9`, `0`, `a` | Firmware `1.0.0-gprs` | Aktuator/menu aman | `m6_final_regression_ext.log`: `n`/`o` merespons, `WiFi=ya MQTT=ya`, `state=LOCKED relay=OFF pin=1`, `publish=10 gagal=0`; `m6_uart_scan_correctpins.log`: menu `0` selesai tanpa menghentikan telemetri; `m6_cmd_wifi.log` (arsip) REGRESI PASS | PASS |

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
8. Wi-Fi/MQTT putus lalu pulih otomatis, **atau** failover GPRS aktif bila WiFi tidak tersedia (M6A).
9. Fitur existing backend/frontend tidak regressi.
10. Semua test case yang relevan memiliki `Actual` dan `Status`.

**Tidak termasuk DoD:** baterai nyata (`N/A`), ACK/retry command, timer lokal/NVS, TLS/MQTT auth, dan——sampai T37 `PASS`—pengiriman telemetri/command lewat GPRS. Semuanya dicatat sebagai keterbatasan. Topik `bike/telemetry/+`/`bike/command/+` sudah diganti `trabass/<id>/telemetry`/`trabass/<id>/command`; backend masih toleran membaca format lama.

---

## 7. Risiko dan mitigasi

| Risiko | Dampak | Mitigasi |
|---|---|---|
| IP PC berubah (DHCP) | ESP32 gagal connect | DHCP reservation; `MQTT_BROKER_HOST` di `config.h` |
| AP isolation Wi-Fi kampus | Device tidak bisa ke PC | Uji B04; fallback hotspot PC |
| GPS tidak fix indoor | Telemetri dibuang backend | Patch M3 no-fix |
| `delay()` pada `beep()` / langkah AT TinyGSM | Watchdog reset | Aktuator dan attach GPRS memakai state machine berbasis `millis()`; satu langkah AT TinyGSM tetap bisa memblokir beberapa detik saat transisi (dokumentasikan) |
| Relay salah polaritas | Kunci terbalik | Konfirmasi `RELAY_ON LOW` / `RELAY_OFF HIGH`; uji T12/T13 |
| Alarm menyala terus | Gangguan | Auto-off 60 detik + `ALARM_OFF` |
| `SENT` dianggap eksekusi | Status palsu | Verifikasi lewat telemetri berikutnya; ACK ditunda |
| SIM800L tidak membalas `AT` (M2→M5→M6A) | T37 `BLOCKED` dulu | **Teratasi**: pin SIM ditukar ke 16/17 (GPS 26/27), VDD→3V3, baud dikunci 9600, adaptor 5 V/3 A terpasang. `AT→OK`, `CPIN READY`, `CIFSR` IP terbukti. Firmware tetap mematikan WiFi saat GPRS sebagai mitigasi bila suplai diputus |
| SIM800L 2G-only | Cakupan GPRS dulu meragukan | **Teratasi di lokasi uji**: `+COPS: 0,0,"TELKOMSEL"`, `CREG 0,1`, `CGATT 1`, IP private diperoleh. Risiko tersisa tetap ada bila pindah lokasi tanpa cakupan 2G; modul 4G perlu dipertimbangkan untuk produksi |
| Broker publik HiveMQ | Topik bisa dilihat pihak lain; tanpa auth/TLS | Namespace `trabass/<BIKE_ID>/…`; jangan kirim data sensitif. Untuk produksi: broker privat + TLS/MQTT auth (di luar DoD) |
| Antrean command penuh saat GPRS | Command terbuang | 4 slot × 512 B, telemetri tetap jalan; ACK belum ada (M7) |
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
