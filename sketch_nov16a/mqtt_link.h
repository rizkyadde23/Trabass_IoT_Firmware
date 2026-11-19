/*
 * Trabass IoT - Modul konektivitas WiFi + GPRS + MQTT (M6)
 * ------------------------------------------------------------------
 * Tanggung jawab:
 *   - WiFi STA sebagai transport utama, GPRS TinyGSM sebagai cadangan.
 *   - MQTT dua arah (telemetry publish + command subscribe) di kedua
 *     transport dengan callback dan antrean yang sama.
 *   - Menandai event reconnect agar telemetri langsung dikirim ulang.
 *
 * Modul ini TIDAK menggerakkan aktuator. Eksekusi command adalah
 * tanggung jawab command_handler.
 */

#ifndef TRABASS_MQTT_LINK_H
#define TRABASS_MQTT_LINK_H

#include <Arduino.h>
#include "app_types.h"

namespace mqtt_link {

// Panggil sekali dari setup(). Tidak memblokir lama.
void begin();

// Panggil tiap iterasi loop(). Tidak memblokir lama.
void loop();

bool wifiConnected();
bool mqttConnected();
LinkState state();
const char *stateLabel();

// Minta attach GPRS. Mematikan radio WiFi sementara karena aktivitas WiFi
// terbukti mengganggu UART2 SIM800L bila suplai 5 V belum terpisah.
void requestGprs();

// Transport aktif dan status GPRS untuk menu serial.
const char *transportLabel();
bool gprsAttached();
const char *gprsIp();

// Info jaringan untuk menu serial.
String ssid();
String localIp();
int32_t rssi();

// Publish payload telemetri (QoS 0, retain false).
// force=true mengabaikan interval TELEMETRY_INTERVAL_MS.
// Return true hanya jika publish benar-benar terkirim.
bool publishTelemetry(const char *payload, bool force = false);

// Ambil (dan bersihkan) event "baru saja terhubung MQTT". Dipakai
// telemetry untuk publish on connect tanpa duplikasi.
bool takeConnectEvent();

// Ambil satu payload command dari antrean (dipakai M4).
// Return true jika ada payload yang disalin ke out.
bool takeCommand(char *out, size_t outSize);

const LinkStats &stats();

} // namespace mqtt_link

#endif // TRABASS_MQTT_LINK_H
