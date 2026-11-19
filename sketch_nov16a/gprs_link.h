/*
 * Trabass IoT - Tautan GPRS SIM800L via TinyGSM (M6)
 * ------------------------------------------------------------------
 * Modul ini adalah PEMILIK TUNGGAL UART2. Tidak ada layanan SMS lagi.
 * TinyGSM dipakai untuk attach GPRS (APN internet) dan menyediakan
 * TinyGsmClient yang dipakai PubSubClient untuk MQTT dua arah.
 *
 * Semua operasi attach dipecah per langkah di loop() agar tidak ada
 * delay() panjang di jalur utama. Satu langkah AT memang bisa memblokir
 * beberapa detik (keterbatasan TinyGSM) tetapi hanya saat transisi.
 */

#ifndef TRABASS_GPRS_LINK_H
#define TRABASS_GPRS_LINK_H

#include <Arduino.h>

// TinyGSM mewajibkan pilihan modem SEBELUM include.
#ifndef TINY_GSM_MODEM_SIM800
#define TINY_GSM_MODEM_SIM800
#endif
#ifndef TINY_GSM_RX_BUFFER
#define TINY_GSM_RX_BUFFER 512
#endif
#include <TinyGsmClient.h>

namespace gprs_link {

// Inisialisasi UART2. Tidak memblokir.
void begin();

// Dipanggil tiap iterasi loop(). Menjalankan satu langkah attach per call.
void loop();

// true setelah GPRS aktif dan modul punya IP.
bool attached();

// Minta attach; return true jika sudah attached.
bool requestConnect();

// Lepas sesi TCP tetapi biarkan GPRS ter-attach (failover cepat).
void release();

// Hentikan percobaan attach (dipakai saat WiFi pulih). Tidak memutus
// sesi data bila memang sudah attached.
void cancel();

const char *stateLabel();
const char *ip();
int csq();
uint32_t attempts();
uint32_t failures();

// Client TinyGSM yang dipakai PubSubClient.
TinyGsmClient &client();

// AT mentah untuk menu Serial. Tidak memakai parser TinyGSM.
bool debugAt(const char *cmd, char *out, size_t outSize,
             uint32_t timeoutMs = 1200);

// Menu '9': tukar mapping RX/TX saat runtime (tanpa re-solder).
bool swapPins();
const char *pinLabel();

// Menu '0': scan lengkap 2 mapping x 8 baud x terminator CR/CRLF.
// Dipanggil manual dari Serial Monitor, bukan dari loop().
void scanUart();

} // namespace gprs_link

#endif // TRABASS_GPRS_LINK_H
