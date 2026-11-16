/*
 * Trabass IoT - Modul konektivitas WiFi + MQTT
 * ------------------------------------------------------------------
 * Tanggung jawab:
 *   - Menjaga koneksi WiFi STA dan MQTT (non-blocking).
 *   - Subscribe topic command dan menaruh payload ke antrean.
 *   - Publish telemetri (payload disusun oleh modul telemetry).
 *   - Menandai event reconnect agar telemetri langsung dikirim ulang.
 *
 * Modul ini TIDAK menggerakkan aktuator. Eksekusi command adalah
 * tanggung jawab command_handler (Milestone 4).
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
