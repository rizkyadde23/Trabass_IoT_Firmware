/*
 * Trabass IoT - Modul konektivitas WiFi + MQTT
 * ------------------------------------------------------------------
 * Tanggung jawab:
 *   - Menjaga koneksi WiFi STA dan MQTT (non-blocking).
 *   - Subscribe topic command dan menaruh payload ke antrean.
 *   - Publish heartbeat periodik + langsung setelah reconnect.
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

// Publish heartbeat. force=true mengabaikan interval.
void publishHeartbeat(bool force = false);

// Ambil satu payload command dari antrean (dipakai M4).
// Return true jika ada payload yang disalin ke out.
bool takeCommand(char *out, size_t outSize);

const LinkStats &stats();

} // namespace mqtt_link

#endif // TRABASS_MQTT_LINK_H
