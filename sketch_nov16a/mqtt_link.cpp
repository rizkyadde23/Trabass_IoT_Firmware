/*
 * Trabass IoT - Implementasi WiFi + MQTT
 * ------------------------------------------------------------------
 * Prinsip:
 *   1. Tidak ada delay() panjang; semua berbasis millis().
 *   2. Log hanya saat transisi state atau percobaan ke-N agar tidak
 *      membanjiri Serial.
 *   3. Password WiFi tidak pernah dicetak.
 *   4. Callback MQTT hanya menyalin payload ke antrean.
 */

#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "mqtt_link.h"

namespace {

constexpr size_t kCmdSlotSize = 192;
constexpr size_t kCmdQueueSize = 4;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

LinkStats gStats;
LinkState gState = LinkState::Offline;

bool gWifiWasConnected = false;
bool gMqttWasConnected = false;
bool gConnectEvent = false;

uint32_t gWifiRetryAtMs = 0;
uint32_t gLastMqttAttemptMs = 0;
uint32_t gLastPublishMs = 0;
bool gHasPublished = false;

char gCmdQueue[kCmdQueueSize][kCmdSlotSize];
size_t gCmdHead = 0;
size_t gCmdTail = 0;
size_t gCmdCount = 0;

void enqueueCommand(const char *payload) {
  if (payload == nullptr) return;

  strncpy(gCmdQueue[gCmdHead], payload, kCmdSlotSize - 1);
  gCmdQueue[gCmdHead][kCmdSlotSize - 1] = '\0';

  gCmdHead = (gCmdHead + 1) % kCmdQueueSize;
  if (gCmdCount < kCmdQueueSize) {
    gCmdCount++;
  } else {
    // Antrean penuh: buang yang paling lama.
    gCmdTail = (gCmdTail + 1) % kCmdQueueSize;
  }

  gStats.commandCount++;
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  // Callback tidak boleh blocking. Salin payload lalu selesai.
  char buffer[kCmdSlotSize];
  size_t copyLength = length < (sizeof(buffer) - 1) ? length : (sizeof(buffer) - 1);
  memcpy(buffer, payload, copyLength);
  buffer[copyLength] = '\0';

  enqueueCommand(buffer);

  Serial.printf("[MQTT] Command masuk %u B dari %s\n",
                length,
                topic != nullptr ? topic : "?");
}

void ensureWifi() {
  wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    if (!gWifiWasConnected) {
      gWifiWasConnected = true;
      gStats.wifiConnects++;
      gState = LinkState::WifiConnected;
      Serial.printf("[WIFI] Terhubung SSID=%s IP=%s RSSI=%d dBm\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
    }
    return;
  }

  if (gWifiWasConnected) {
    gWifiWasConnected = false;
    gMqttWasConnected = false;
    gState = LinkState::WifiConnecting;
    Serial.println("[WIFI] Koneksi terputus, mulai reconnect...");

    // Reset state MQTT agar tidak dianggap connected saat socket mati.
    if (mqtt.connected()) {
      mqtt.disconnect();
    }
  }

  uint32_t now = millis();
  if (gWifiRetryAtMs != 0 && (int32_t)(now - gWifiRetryAtMs) < 0) {
    return;
  }

  gWifiRetryAtMs = now + WIFI_CONNECT_TIMEOUT_MS;
  gStats.wifiAttempts++;
  gState = LinkState::WifiConnecting;

  Serial.printf("[WIFI] Percobaan connect #%lu (status=%d)\n",
                (unsigned long)gStats.wifiAttempts,
                (int)status);
  WiFi.reconnect();
}

void ensureMqtt() {
  if (WiFi.status() != WL_CONNECTED) {
    gState = LinkState::WifiConnecting;
    return;
  }

  if (mqtt.connected()) {
    if (!gMqttWasConnected) {
      gMqttWasConnected = true;
      gStats.mqttConnects++;
      gState = LinkState::MqttConnected;
      gConnectEvent = true;
      Serial.printf("[MQTT] Terhubung ke %s:%d, subscribe %s\n",
                    MQTT_BROKER_HOST,
                    MQTT_BROKER_PORT,
                    MQTT_TOPIC_COMMAND);
    }
    return;
  }

  if (gMqttWasConnected) {
    gMqttWasConnected = false;
    gState = LinkState::WifiConnected;
    Serial.println("[MQTT] Koneksi terputus, akan retry.");
  }

  uint32_t now = millis();
  if (gLastMqttAttemptMs != 0 &&
      (now - gLastMqttAttemptMs) < MQTT_RECONNECT_INTERVAL_MS) {
    return;
  }
  gLastMqttAttemptMs = now;
  gStats.mqttAttempts++;

  char clientId[40];
  snprintf(clientId, sizeof(clientId), "%s%s", MQTT_CLIENT_ID_PREFIX, BIKE_ID);

  bool connected = false;
#if MQTT_ENABLE_LWT
  static const char kWillPayload[] = "{\"online\":false}";
  connected = mqtt.connect(clientId, nullptr, nullptr,
                           MQTT_TOPIC_STATUS, 0, true, kWillPayload);
#else
  connected = mqtt.connect(clientId);
#endif

  if (connected) {
    gMqttWasConnected = true;
    gStats.mqttConnects++;
    gState = LinkState::MqttConnected;
    gConnectEvent = true;

    if (!mqtt.subscribe(MQTT_TOPIC_COMMAND, MQTT_SUBSCRIBE_QOS)) {
      Serial.println("[MQTT] PERINGATAN: subscribe command gagal.");
    }

    Serial.printf("[MQTT] Connected clientId=%s\n", clientId);
  } else {
    // Log tiap 5 percobaan agar tidak spam.
    if ((gStats.mqttAttempts % 5) == 1) {
      Serial.printf("[MQTT] Gagal connect (state=%d), retry %d ms\n",
                    mqtt.state(),
                    MQTT_RECONNECT_INTERVAL_MS);
    }
  }
}

bool publishTelemetryInternal(const char *payload, bool force) {
  if (payload == nullptr || payload[0] == '\0') {
    return false;
  }
  if (!mqtt.connected()) {
    return false;
  }

  uint32_t now = millis();
  if (!force && gHasPublished &&
      (now - gLastPublishMs) < (uint32_t)TELEMETRY_INTERVAL_MS) {
    return false;
  }

  const size_t length = strlen(payload);
  if (length >= MQTT_BUFFER_SIZE) {
    Serial.printf("[MQTT] Payload %u B melebihi buffer %d B.\n",
                  (unsigned)length,
                  MQTT_BUFFER_SIZE);
    return false;
  }

  if (mqtt.publish(MQTT_TOPIC_TELEMETRY, payload)) {
    gStats.publishCount++;
    gStats.lastPublishMs = now;
    gLastPublishMs = now;
    gHasPublished = true;
    return true;
  }

  gStats.publishFailures++;
  if ((gStats.publishFailures % 10) == 1) {
    Serial.printf("[MQTT] Publish gagal (total=%lu). Buffer=%d B\n",
                  (unsigned long)gStats.publishFailures,
                  MQTT_BUFFER_SIZE);
  }
  return false;
}

} // namespace

namespace mqtt_link {

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // kurangi latensi downlink command
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t now = millis();
  gWifiRetryAtMs = now + WIFI_CONNECT_TIMEOUT_MS;
  gStats.wifiAttempts++;

  mqtt.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  mqtt.setCallback(onMqttMessage);
  mqtt.setBufferSize(MQTT_BUFFER_SIZE);
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setSocketTimeout(2);

  Serial.printf("[MQTT] Broker=%s:%d clientId=%s%s\n",
                MQTT_BROKER_HOST,
                MQTT_BROKER_PORT,
                MQTT_CLIENT_ID_PREFIX,
                BIKE_ID);
}

void loop() {
  ensureWifi();
  ensureMqtt();

  if (mqtt.connected()) {
    mqtt.loop();
  }

  // M2/M3: command hanya dicatat. M4 akan melakukan dispatch.
  char command[kCmdSlotSize];
  while (takeCommand(command, sizeof(command))) {
    Serial.printf("[CMD] Diterima: %s\n", command);
  }
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool mqttConnected() {
  return mqtt.connected();
}

LinkState state() {
  return gState;
}

const char *stateLabel() {
  switch (gState) {
    case LinkState::Offline: return "OFFLINE";
    case LinkState::WifiConnecting: return "WIFI_CONNECTING";
    case LinkState::WifiConnected: return "WIFI_CONNECTED";
    case LinkState::MqttConnected: return "MQTT_CONNECTED";
  }
  return "UNKNOWN";
}

String ssid() {
  if (WiFi.status() != WL_CONNECTED) return String("-");
  return WiFi.SSID();
}

String localIp() {
  if (WiFi.status() != WL_CONNECTED) return String("0.0.0.0");
  return WiFi.localIP().toString();
}

int32_t rssi() {
  if (WiFi.status() != WL_CONNECTED) return 0;
  return WiFi.RSSI();
}

bool publishTelemetry(const char *payload, bool force) {
  return publishTelemetryInternal(payload, force);
}

bool takeConnectEvent() {
  const bool event = gConnectEvent;
  gConnectEvent = false;
  return event;
}

bool takeCommand(char *out, size_t outSize) {
  if (out == nullptr || outSize == 0 || gCmdCount == 0) {
    return false;
  }

  strncpy(out, gCmdQueue[gCmdTail], outSize - 1);
  out[outSize - 1] = '\0';

  gCmdTail = (gCmdTail + 1) % kCmdQueueSize;
  gCmdCount--;
  return true;
}

const LinkStats &stats() {
  return gStats;
}

} // namespace mqtt_link
