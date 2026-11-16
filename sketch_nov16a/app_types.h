/*
 * Trabass IoT - Tipe data bersama
 * ------------------------------------------------------------------
 * Header ini hanya berisi enum/struct yang dipakai lintas modul.
 * Tidak ada definisi variabel di sini agar tidak terjadi multiple
 * definition saat di-include oleh beberapa .cpp.
 */

#ifndef TRABASS_APP_TYPES_H
#define TRABASS_APP_TYPES_H

#include <Arduino.h>

// Catatan: nama anggota sengaja CamelCase karena PubSubClient
// mendefinisikan macro MQTT_CONNECTED/MQTT_DISCONNECTED, dll.
enum class LinkState : uint8_t {
  Offline = 0,        // WiFi belum terhubung
  WifiConnecting,     // sedang mencoba/menunggu WiFi
  WifiConnected,      // WiFi siap, MQTT belum
  MqttConnected,      // siap publish/subscribe
};

struct LinkStats {
  uint32_t wifiAttempts = 0;
  uint32_t wifiConnects = 0;
  uint32_t mqttAttempts = 0;
  uint32_t mqttConnects = 0;
  uint32_t publishCount = 0;
  uint32_t publishFailures = 0;
  uint32_t commandCount = 0;
  uint32_t lastPublishMs = 0;
};

#endif // TRABASS_APP_TYPES_H
