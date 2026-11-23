# Tabel Pin ESP32 — Trabass IoT

**Versi:** 1.1 — **Tanggal:** 2026-11-22  
**Firmware:** `1.0.2-gprsfix` — **Board:** ESP32 DevKit  
**Sumber:** `konteks.md` §3.1–3.3 dan `sketch_nov16a/config.example.h`

## 1. Hubungan pin ESP32 ↔ komponen (ringkasan)

| Pin / Konektor ESP32 | Fungsi ESP32 | Komponen | Arah kabel | Detail penting | Status |
|---|---|---|---|---|---|
| `GND` | Ground | Semua modul + adaptor SIM800L | GND bersama | **Wajib common ground** ESP32–SIM800L–GPS–LCD–MPU6050–relay–adaptor | Aktif |
| `3V3` | Referensi level | SIM800L pin `VDD` | `3V3 → VDD` | Pin `VDD` **bukan** pin daya 5 V modem | Aktif |
| `5V / VIN` | Suplai opsional modul | LCD 16×2, relay | sesuai kebutuhan modul | **Jangan** dipakai untuk pin `5V` SIM800L | Aktif (dengan catatan) |
| Adaptor eksternal `5 V / 3 A` | Daya modem | SIM800L pin `5V` | `5V → 5V modul` | Kapasitor `1000 µF/16 V` di dekat pin modul. Suplai dari VIN ESP32 dulu memicu `UNDER-VOLTAGE WARNING` | Aktif |
| `GPIO21` | I2C `SDA` | LCD 16×2, MPU6050 | `SDA ↔ SDA` | Bus I2C bersama | Aktif |
| `GPIO22` | I2C `SCL` | LCD 16×2, MPU6050 | `SCL ↔ SCL` | Bus I2C bersama | Aktif |
| `GPIO26` | UART1 `RX` | GPS NEO-6M | `TXD GPS → RX26` | `GPS_BAUD 9600`, `SERIAL_8N1` | Aktif |
| `GPIO27` | UART1 `TX` | GPS NEO-6M | `RXD GPS ← TX27` | `GPS_BAUD 9600`, `SERIAL_8N1` | Aktif |
| `GPIO16` | UART2 `RX` | SIM800L v2 EVB | `TXD modul → RX16` | `SIM_BAUD 9600`. Modem sempat ber-autobaud (`+IPR:0`) sehingga `AT` kadang kosong; sudah dikunci manual ke `AT+IPR=9600` (`+IPR?` → `9600`) | Aktif, terverifikasi GPRS |
| `GPIO17` | UART2 `TX` | SIM800L v2 EVB | `RXD modul ← TX17` | Level logika modul ~2,8 V; GND wajib bersama | Aktif |
| `GPIO4` | Digital output | Relay / solenoid via level converter | `LV1 ← GPIO4`, `HV1 → IN relay` | **Aktif LOW**: `LOCK` = pin HIGH (3,3 V → 5 V) / relay OFF; `UNLOCK` = pin LOW / relay ON | Terbukti T47 |
| `GPIO19` | Digital output | Buzzer | `IN ← GPIO19` | Toggle ~2 kHz | Aktif |
| `GPIO1 / GPIO3` | UART0 USB | PC / Serial Monitor | USB | `SERIAL_BAUD 115200`; menu tes serial | Aktif |
| `EN`, `BOOT` | Reset / boot | Tombol onboard DevKit | — | Standar board | Aktif |

## 2. Tabel per komponen

### 2.1 ESP32 DevKit

| Pin ESP32 | Terhubung ke | Arah | Kegunaan | Status |
|---|---|---|---|---|
| `GPIO21` | LCD `SDA`, MPU6050 `SDA` | out | Bus I2C (data); user menukar SDA/SCL pada extension board | LCD + MPU `0x68` terbukti |
| `GPIO22` | LCD `SCL`, MPU6050 `SCL` | out | Bus I2C (clock); user menukar SDA/SCL pada extension board | LCD + MPU `0x68` terbukti |
| `GPIO26` | GPS `TXD` | in | UART1 RX | Terbukti, 9600 baud |
| `GPIO27` | GPS `RXD` | out | UART1 TX | Terbukti, 9600 baud |
| `GPIO16` | SIM800L `TXD` | in | UART2 RX | Terbukti `AT→OK` |
| `GPIO17` | SIM800L `RXD` | out | UART2 TX | Terbukti |
| `GPIO4` | Level converter `LV1` → `HV1` → Relay `IN` | out | Kontrol aktif-LOW; 3,3 V diterjemahkan ke 5 V | Terbukti T12/T13, T47 |
| `GPIO19` | Buzzer `IN` | out | Alarm / beep | Terbukti via log `[ALARM]` |
| `3V3` | SIM800L `VDD`, level converter `LV` | power | Referensi level logika ESP32 | Terbukti, T47 |
| `USB` | PC `COM14` | — | Upload + Serial 115200 | Terbukti |
| `GND` | Semua modul + adaptor | — | Common ground | Wajib |

### 2.2 LCD 16×2 I2C

| Pin modul | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `SDA` | ESP32 `GPIO21` | in | I2C data | Terbukti |
| `SCL` | ESP32 `GPIO22` | in | I2C clock | Terbukti |
| `VCC` | `3V3` / `5V` ESP32 | power | Sesuai label modul | Sudah dijumper user (LCD terbaca normal, dilaporkan) |
| `GND` | GND bersama | — | — | Wajib |
| Alamat | `0x27` (alternatif `0x3F`) | — | Cek dengan menu serial `s` | Terbukti `0x27` |

### 2.3 MPU6050

| Pin modul | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `SDA` | ESP32 `GPIO21` | in | Bus I2C bersama LCD | Terbukti setelah SDA/SCL ditukar user |
| `SCL` | ESP32 `GPIO22` | in | Bus I2C bersama LCD | Terbukti setelah SDA/SCL ditukar user |
| `VCC` | ESP32 `3V3` / `5V` | power | Breakout GY-521 sering butuh `5V` karena regulator onboard; ukur tegangan di pin modul | Modul merespons; tegangan pasti belum diukur agent |
| `GND` | GND bersama | — | — | Wajib |
| `AD0` | — | — | Default `0x68`; HIGH → `0x69` | Firmware auto-probe `0x68`/`0x69` |

### 2.4 GPS NEO-6M

| Pin modul | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `TXD` | ESP32 `GPIO26` | out | UART1 RX ESP32 | Terbukti, 9600 baud |
| `RXD` | ESP32 `GPIO27` | in | UART1 TX ESP32 | Terbukti, 9600 baud |
| `VCC` | `3V3` / `5V` ESP32 | power | Sesuai label modul | Perlu cek fisik |
| `GND` | GND bersama | — | — | Wajib |

### 2.5 SIM800L v2 EVB

| Pin modul | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `TXD` | ESP32 `GPIO16` | out | UART2 RX ESP32 | Terbukti `AT→OK` |
| `RXD` | ESP32 `GPIO17` | in | UART2 TX ESP32 | Level logika ~2,8 V |
| `VDD` | ESP32 `3V3` | power | Referensi level, bukan daya 5 V | Terbukti |
| `5V` | Adaptor eksternal `5 V / 3 A` | power | **Jangan** dari `VIN`/`5V` ESP32 | Terbukti |
| `GND` | GND bersama | — | Termasuk GND adaptor | Wajib |
| `PWRKEY` | — | — | Tidak ada; modul self-power | Sesuai EVB |

### 2.6 Modul Relay 1-channel

| Pin modul | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `IN` | ESP32 `GPIO4` via level converter (`LV1 → HV1`) | in | **Aktif LOW di sisi relay**; input 3,3 V dinaikkan ke 5 V | Terbukti T12/T13, T47 |
| `VCC` | ESP32 `5V` | power | Modul relay umum; dikonfirmasi user | Terbukti T47 |
| `GND` | GND bersama | — | — | Wajib |
| `COM` | Suplai **+** solenoid | power | Sumber terpisah, bukan dari ESP32 | Sesuai konfirmasi wiring |
| `NO` | Solenoid terminal **+** | out | Kontak tertutup saat relay ON | Sesuai konfirmasi wiring |
| `NC` | **Tidak dipakai** | — | Dibiarkan kosong | Sesuai konfirmasi wiring |

### 2.6a Modul Logic Level Converter 3,3V ↔ 5V

| Pin converter | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `LV` | ESP32 `3V3` | power | Sisi logika ESP32 | Terbukti T47 |
| `HV` | ESP32 `5V` / suplai 5V modul | power | Sisi logika relay | Terbukti T47 |
| `GND` | GND bersama | — | ESP32 + relay + suplai solenoid | Wajib |
| `LV1` | ESP32 `GPIO4` | in | Output 3,3 V dari firmware | Terbukti T47 |
| `HV1` | Relay `IN` | out | Level 5 V saat `LOCK`, 0 V saat `UNLOCK` | Terbukti T47 |

Converter bersifat **non-inverting**. Firmware tidak diubah: `RELAY_OFF=HIGH`,
`RELAY_ON=LOW`, dan boot default tetap terkunci. Sebelum T47, GPIO4 3,3 V disambung
langsung ke `IN` relay 5V sehingga relay tetap ON saat `LOCK` (root cause).

**Logika kontak:**

| Kondisi | `GPIO4` | `IN relay` setelah converter | Relay | Kontak `COM–NO` | Solenoid |
|---|---|---|---|---|---|
| Terkunci (`LOCK`) | HIGH (3,3 V) | HIGH (5 V) | OFF | Terbuka | Nonaktif |
| Terbuka (`UNLOCK`) | LOW (0 V) | LOW (0 V) | ON | Tertutup | Aktif |

### 2.7 Solenoid / kunci elektronik

| Terminal | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `+` | Relay `NO` | in | Mendapat daya saat `UNLOCK` | Sesuai konfirmasi wiring |
| `−` | GND suplai solenoid | — | GND disatukan dengan ESP32 | Wajib |
| Sumber daya | Adaptor/PSU terpisah | power | **Jangan** disuap dari ESP32 | Perlu cek fisik |

### 2.8 Buzzer

| Pin modul | Terhubung ke | Arah | Detail | Status |
|---|---|---|---|---|
| `IN / +` | ESP32 `GPIO19` | in | Toggle ~2 kHz | Terbukti via log `[ALARM]` |
| `GND / −` | GND bersama | — | — | Wajib |
| `VCC` | `3V3` / `5V` | power | Hanya jika modul 3 pin | Perlu cek tipe modul |

### 2.9 Adaptor 5 V/3 A + kapasitor 1000 µF/16 V

| Elemen | Terhubung ke | Detail | Status |
|---|---|---|---|
| `+5 V` adaptor | SIM800L `5V` | Tidak ke `VIN`/`5V` ESP32 saat USB tercolok | Terbukti |
| `GND` adaptor | GND bersama ESP32 | Common ground wajib | Terbukti |
| Kapasitor `1000 µF/16 V` | Paralel `5V–GND` di dekat modem | Peredam burst arus TX | Terpasang |

### 2.10 PC / Serial Monitor

| Konektor | Terhubung ke | Detail | Status |
|---|---|---|---|
| USB PC | ESP32 USB-UART | Driver **CP210x**, port `COM14` | Terbukti |
| Serial internal | `GPIO1` / `GPIO3` | `115200 baud` | Terbukti |
| GND USB | GND ESP32 | Lewat kabel USB | Terbukti |

## 3. Alamat I2C

| Perangkat | Alamat default | Alternatif | Keterangan |
|---|---:|---:|---|
| LCD 16×2 (I2C) | `0x27` | `0x3F` | Cek dengan menu serial `s` |
| MPU6050 | `0x68` | `0x69` | `0x69` jika pin AD0 HIGH |

Keduanya berbagi bus yang sama pada `GPIO21` (SDA) dan `GPIO22` (SCL).

**Hasil diagnostik 2026-11-22 (arsip):** menu serial `s` dulu hanya menemukan `0x27` (LCD). Setelah user menukar SDA/SCL pada extension board, `bukti/M7/T43_imu_detected.log` menunjukkan `0x27` **dan `0x68`**, dengan pembacaan Acc/Gyro/Suhu yang valid. Keterbatasan: classifier `mpu_motion` produksi belum terpicu oleh goyangan fisik karena ambang 40 d/s harus bertahan 1000 ms (lihat `T43_imu_raw_samples.log`); jalur insiden TAMPER tetap diverifikasi lewat injeksi MQTT di T23.

## 4. Catatan wiring kritikal

- **SIM800L v2 EVB self-power, tanpa PWRKEY.** Modul aktif saat pin `5V` mendapat suplai.
- Pin `5V` SIM800L **wajib** dari adaptor eksternal `5 V / 3 A`, bukan dari `VIN`/`5V` ESP32. `VDD` tetap ke `3V3` ESP32 sebagai referensi level.
- `GND` adaptor, ESP32, SIM800L, GPS, LCD, MPU6050, relay, dan suplai solenoid **harus disatukan**.
- **Relay memakai `COM + NO`**, `NC` tidak dipakai. `COM` menerima suplai **+** solenoid, `NO` menuju solenoid, dan sisi lain solenoid ke GND suplai. Solenoid **jangan** disuap dari ESP32.
- Solenoid bersifat **default terkunci secara mekanis**: tanpa daya = terkunci, saat relay `ON`/berdaya = terbuka. Ini sudah sesuai `LOCK = relay OFF` dan `UNLOCK = relay ON`; jangan di-inversi.
- Kecerahan LCD I2C diatur oleh **hardware** (tegangan suplai, potensio kontras, resistor/jumper backlight). `lcd.backlight()` hanya menyalakan/mematikan LED, bukan mengatur level brightness. Jika redup walau potensio mentok, ukur VCC modul dan periksa resistor backlight.
- Modem sudah dikunci ke baud `9600` permanen (semula autobaud `+IPR:0`). Jangan mengubah `SIM_BAUD` tanpa mengubah konfigurasi modem.
- Mapping final hasil swap hardware: **SIM `16/17`, GPS `26/27`**. Menu serial `9` dapat menukar `16/17 ↔ 17/16` saat runtime tanpa re-solder; menu `0` memindai 32 kombinasi mapping/baud/terminator.
- Relay bersifat **aktif LOW**: pin HIGH = terkunci, pin LOW = terbuka.
- Layanan SMS sudah dihapus; UART2 dipakai penuh oleh TinyGSM untuk GPRS/MQTT. Command `SEND_SMS` diabaikan firmware.

## 5. Riwayat perubahan pin

| Tanggal | Perubahan | Alasan |
|---|---|---|
| 2026-11-19 | SIM `26/27` → `16/17`; GPS `16/17` → `26/27` | Pin SIM tertukar dengan GPS; setelah ditukar `AT→OK` |
| 2026-11-19 | Baud SIM autobaud `+IPR:0` → `+IPR=9600` | Respons AT tidak stabil; saat itu stabil (ternyata tidak permanen — lihat entri 2026-11-22) |
| 2026-11-19 | Suplai `5V` SIM dari VIN → adaptor `5 V / 3 A` | Menghilangkan `UNDER-VOLTAGE WARNING` dan `CREG 0,2` |
| 2026-11-22 | Penambahan tabel per komponen + konfirmasi `COM/NO/NC` relay | Memperjelas koneksi tiap komponen dan jalur solenoid |
| 2026-11-22 | koreksi status MPU6050 + catatan daya LCD | I2C scan hanya menemukan `0x27`; MPU belum terdeteksi. LCD redup bukan bug firmware |
| 2026-11-22 | firmware `1.0.1-hwdiag` | Auto-probe MPU `0x68`/`0x69`, I2C 100 kHz, early-drive relay `RELAY_OFF` sebelum `pinMode(OUTPUT)`; upload COM14 sukses |
| 2026-11-22 | M7: SDA/SCL IMU ditukar, LCD dijumper | I2C scan menemukan `0x27` + `0x68`; LCD dilaporkan normal oleh user |
| 2026-11-22 | M7: simulasi failover SSID salah lalu pulihkan | Config asli dikembalikan (SHA-256 identik), upload ulang sukses, WiFi/MQTT `MQTT_CONNECTED` |
| 2026-11-22 | M7: `AT+IPR?` kembali `0` (autobaud) setelah modem hang | Power-cycle modem lalu `AT+IPR=9600`; `AT` stabil, `CSQ 19`, `CREG 0,1`, `TELKOMSEL` |
| 2026-11-22 | M7: firmware `1.0.2-gprsfix` | Perbaiki `requestConnect()`/safety net saat `gRequested=true` tapi step `IDLE` (menu `9`/`0` bisa memicu); GPRS attach, MQTT via GPRS, telemetri `transport:"GPRS"` dan badge dashboard PASS |
