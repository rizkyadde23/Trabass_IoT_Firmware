/*
 * Trabass IoT - WiFi + GPRS + MQTT (M6)
 * ------------------------------------------------------------------
 * WiFi adalah transport utama. Jika WiFi putus lebih lama dari
 * WIFI_FAILOVER_AFTER_MS, MQTT dialihkan ke TinyGsmClient (GPRS).
 * Setelah WiFi stabil kembali, transport dialihkan balik.
 *
 * PubSubClient menyimpan Client& saat konstruksi, jadi dipakai dua
 * instance dengan callback dan antrean command yang sama.
 */

#include <WiFi.h>
#include <PubSubClient.h>

#include "config.h"
#include "gprs_link.h"
#include "mqtt_link.h"

namespace {

constexpr size_t kCmdSlotSize = 512;
constexpr size_t kCmdQueueSize = 4;

WiFiClient wifiClient;
PubSubClient mqttWifi(wifiClient);
PubSubClient mqttGsm(gprs_link::client());

enum class Transport : uint8_t { Wifi, Gprs };

LinkStats gStats;
LinkState gState = LinkState::Offline;
Transport gTransport = Transport::Wifi;

bool gWifiWasConnected = false;
bool gMqttWasConnected = false;
bool gConnectEvent = false;
bool gGprsRequested = false;
bool gWifiOffForGprs = false;
bool gWifiProbing = false;
uint32_t gWifiProbeAtMs = 0;

uint32_t gWifiRetryAtMs = 0;
uint32_t gWifiLostSinceMs = 0;
uint32_t gWifiBackSinceMs = 0;
uint32_t gLastMqttAttemptMs = 0;
uint32_t gLastPublishMs = 0;
bool gHasPublished = false;

char gCmdQueue[kCmdQueueSize][kCmdSlotSize];
size_t gCmdHead = 0;
size_t gCmdTail = 0;
size_t gCmdCount = 0;

PubSubClient &activeMqtt() {
  return gTransport == Transport::Gprs ? mqttGsm : mqttWifi;
}

void enqueueCommand(const char *payload) {
  if (payload == nullptr) return;

  strncpy(gCmdQueue[gCmdHead], payload, kCmdSlotSize - 1);
  gCmdQueue[gCmdHead][kCmdSlotSize - 1] = '\0';

  gCmdHead = (gCmdHead + 1) % kCmdQueueSize;
  if (gCmdCount < kCmdQueueSize) {
    gCmdCount++;
  } else {
    gCmdTail = (gCmdTail + 1) % kCmdQueueSize;
  }

  gStats.commandCount++;
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  char buffer[kCmdSlotSize];
  size_t copyLength = length < (sizeof(buffer) - 1) ? length : (sizeof(buffer) - 1);
  memcpy(buffer, payload, copyLength);
  buffer[copyLength] = '\0';

  enqueueCommand(buffer);

  Serial.printf("[MQTT] Command masuk %u B dari %s via %s\n",
                length, topic != nullptr ? topic : "?",
                gTransport == Transport::Gprs ? "GPRS" : "WIFI");
}

void switchTransport(Transport to, const char *reason) {
  if (gTransport == to) return;

  activeMqtt().disconnect();
  gTransport = to;
  gMqttWasConnected = false;
  gLastMqttAttemptMs = 0;

  if (to == Transport::Wifi) {
    gprs_link::release();
    gprs_link::cancel();
    gGprsRequested = false;
    gState = LinkState::WifiConnected;
  } else {
    gState = gprs_link::attached() ? LinkState::GprsConnected
                                  : LinkState::GprsAttaching;
  }

  Serial.printf("[NET] Transport -> %s (%s)\n",
                to == Transport::Wifi ? "WIFI" : "GPRS", reason);
}

void parkWifiForGprs() {
  if (gWifiOffForGprs) return;
  gWifiOffForGprs = true;
  gWifiProbing = false;
  gWifiWasConnected = false;
  gWifiLostSinceMs = millis();
  gWifiProbeAtMs = millis() + (uint32_t)GPRS_WIFI_PROBE_MS;
  WiFi.mode(WIFI_OFF);
  Serial.println("[WIFI] Radio dimatikan sementara agar UART2 SIM800L stabil.");
}

void wakeWifiForProbe() {
  gWifiProbing = true;
  gWifiRetryAtMs = millis() + WIFI_CONNECT_TIMEOUT_MS;
  gStats.wifiAttempts++;
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.println("[WIFI] Probe pemulihan WiFi dimulai.");
}

void ensureWifi() {
  const uint32_t now = millis();

  if (gWifiOffForGprs && !gWifiProbing && gWifiProbeAtMs != 0 &&
      (int32_t)(now - gWifiProbeAtMs) >= 0) {
    wakeWifiForProbe();
  }

  const wl_status_t status = WiFi.status();

  if (status == WL_CONNECTED) {
    gWifiOffForGprs = false;
    gWifiProbing = false;
    gWifiProbeAtMs = 0;
    if (!gWifiWasConnected) {
      gWifiWasConnected = true;
      gWifiBackSinceMs = now;
      gWifiLostSinceMs = 0;
      gStats.wifiConnects++;
      Serial.printf("[WIFI] Terhubung SSID=%s IP=%s RSSI=%d dBm\n",
                    WiFi.SSID().c_str(),
                    WiFi.localIP().toString().c_str(),
                    WiFi.RSSI());
    }

    if (gTransport == Transport::Gprs && gWifiBackSinceMs != 0 &&
        (now - gWifiBackSinceMs) >= (uint32_t)WIFI_RESTORE_STABLE_MS) {
      switchTransport(Transport::Wifi, "WiFi pulih stabil");
    }
    return;
  }

  if (gWifiProbing && (int32_t)(now - gWifiRetryAtMs) >= 0) {
    // Probe gagal: matikan lagi radio agar UART2 tetap bersih.
    gWifiProbing = false;
    gWifiProbeAtMs = now + (uint32_t)GPRS_WIFI_PROBE_MS;
    gWifiRetryAtMs = 0;
    WiFi.mode(WIFI_OFF);
    Serial.println("[WIFI] Probe gagal, radio dimatikan lagi.");
    return;
  }

  if (gWifiOffForGprs) return;

  if (gWifiWasConnected) {
    gWifiWasConnected = false;
    gWifiLostSinceMs = now;
    gWifiBackSinceMs = 0;
    gState = LinkState::WifiConnecting;
    Serial.println("[WIFI] Koneksi terputus, mulai reconnect...");
    if (gTransport == Transport::Wifi && mqttWifi.connected()) {
      mqttWifi.disconnect();
    }
  }

  if (gWifiLostSinceMs == 0) gWifiLostSinceMs = now;

  if (gWifiRetryAtMs != 0 && (int32_t)(now - gWifiRetryAtMs) < 0) return;

  gWifiRetryAtMs = now + WIFI_CONNECT_TIMEOUT_MS;
  gStats.wifiAttempts++;
  gState = gTransport == Transport::Gprs ? gState : LinkState::WifiConnecting;

  Serial.printf("[WIFI] Percobaan connect #%lu (status=%d)\n",
                (unsigned long)gStats.wifiAttempts, (int)status);
  WiFi.reconnect();
}

void requestGprs() {
  parkWifiForGprs();
  if (!gGprsRequested) {
    gGprsRequested = true;
    gStats.gprsAttempts++;
  }

  if (gprs_link::requestConnect()) {
    gStats.gprsConnects++;
    switchTransport(Transport::Gprs, "GPRS aktif");
  } else if (gState != LinkState::GprsAttaching) {
    gState = LinkState::GprsAttaching;
  }
}

void ensureGprs() {
  if (WiFi.status() == WL_CONNECTED) {
    gWifiLostSinceMs = 0;
    return;
  }

  const uint32_t now = millis();
  if (gWifiLostSinceMs == 0) gWifiLostSinceMs = now;

  if (gTransport == Transport::Wifi &&
      (now - gWifiLostSinceMs) >= (uint32_t)WIFI_FAILOVER_AFTER_MS) {
    if (!gGprsRequested) {
      Serial.printf("[NET] WiFi putus %lu ms, failover ke GPRS.\n",
                    (unsigned long)(now - gWifiLostSinceMs));
    }
    requestGprs();
  }
}

void configureClient(PubSubClient &client, uint16_t socketTimeoutS) {
  client.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
  client.setCallback(onMqttMessage);
  client.setBufferSize(MQTT_BUFFER_SIZE);
  client.setKeepAlive(MQTT_KEEPALIVE_S);
  client.setSocketTimeout(socketTimeoutS);
}

void ensureMqtt() {
  PubSubClient &mqtt = activeMqtt();

  if (gTransport == Transport::Gprs && !gprs_link::attached()) {
    gState = LinkState::GprsAttaching;
    return;
  }

  if (mqtt.connected()) {
    if (!gMqttWasConnected) {
      gMqttWasConnected = true;
      gStats.mqttConnects++;
      gState = gTransport == Transport::Gprs ? LinkState::MqttGprsConnected
                                             : LinkState::MqttConnected;
      gConnectEvent = true;
      Serial.printf("[MQTT] Terhubung via %s ke %s:%d, subscribe %s\n",
                    gTransport == Transport::Gprs ? "GPRS" : "WIFI",
                    MQTT_BROKER_HOST, MQTT_BROKER_PORT, MQTT_TOPIC_COMMAND);
    }
    return;
  }

  if (gMqttWasConnected) {
    gMqttWasConnected = false;
    gState = gTransport == Transport::Gprs ? LinkState::GprsConnected
                                           : LinkState::WifiConnected;
    Serial.println("[MQTT] Koneksi terputus, akan retry.");
  }

  const uint32_t now = millis();
  if (gLastMqttAttemptMs != 0 &&
      (now - gLastMqttAttemptMs) < MQTT_RECONNECT_INTERVAL_MS) {
    return;
  }
  gLastMqttAttemptMs = now;
  gStats.mqttAttempts++;

  char clientId[48];
  snprintf(clientId, sizeof(clientId), "%s%s-%s",
           MQTT_CLIENT_ID_PREFIX, BIKE_ID,
           gTransport == Transport::Gprs ? "g" : "w");

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
    gState = gTransport == Transport::Gprs ? LinkState::MqttGprsConnected
                                           : LinkState::MqttConnected;
    gConnectEvent = true;

    if (!mqtt.subscribe(MQTT_TOPIC_COMMAND, MQTT_SUBSCRIBE_QOS)) {
      Serial.println("[MQTT] PERINGATAN: subscribe command gagal.");
    }
    Serial.printf("[MQTT] Connected clientId=%s via %s\n", clientId,
                  gTransport == Transport::Gprs ? "GPRS" : "WIFI");
  } else if ((gStats.mqttAttempts % 5) == 1) {
    Serial.printf("[MQTT] Gagal connect via %s (state=%d), retry %d ms\n",
                  gTransport == Transport::Gprs ? "GPRS" : "WIFI",
                  mqtt.state(), MQTT_RECONNECT_INTERVAL_MS);
  }
}

bool publishTelemetryInternal(const char *payload, bool force) {
  if (payload == nullptr || payload[0] == '\0') return false;

  PubSubClient &mqtt = activeMqtt();
  if (!mqtt.connected()) return false;

  const uint32_t now = millis();
  if (!force && gHasPublished &&
      (now - gLastPublishMs) < (uint32_t)TELEMETRY_INTERVAL_MS) {
    return false;
  }

  const size_t length = strlen(payload);
  if (length >= MQTT_BUFFER_SIZE) {
    Serial.printf("[MQTT] Payload %u B melebihi buffer %d B.\n",
                  (unsigned)length, MQTT_BUFFER_SIZE);
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
    Serial.printf("[MQTT] Publish gagal via %s (total=%lu). Buffer=%d B\n",
                  gTransport == Transport::Gprs ? "GPRS" : "WIFI",
                  (unsigned long)gStats.publishFailures, MQTT_BUFFER_SIZE);
  }
  return false;
}

} // namespace

namespace mqtt_link {

void requestGprs() { ::requestGprs(); }

void begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  gWifiRetryAtMs = millis() + WIFI_CONNECT_TIMEOUT_MS;
  gStats.wifiAttempts++;

  configureClient(mqttWifi, 2);
  configureClient(mqttGsm, 5);

  gprs_link::begin();

  Serial.printf("[MQTT] Broker=%s:%d clientId=%s%s (WiFi utama, GPRS cadangan)\n",
                MQTT_BROKER_HOST, MQTT_BROKER_PORT,
                MQTT_CLIENT_ID_PREFIX, BIKE_ID);
}

void loop() {
  gprs_link::loop();
  ensureWifi();
  ensureGprs();
  ensureMqtt();

  gStats.gprsFailures = gprs_link::failures();

  PubSubClient &mqtt = activeMqtt();
  if (mqtt.connected()) mqtt.loop();
}

bool wifiConnected() { return WiFi.status() == WL_CONNECTED; }
bool mqttConnected() { return activeMqtt().connected(); }

LinkState state() { return gState; }

const char *stateLabel() {
  switch (gState) {
    case LinkState::Offline:          return "OFFLINE";
    case LinkState::WifiConnecting:   return "WIFI_CONNECTING";
    case LinkState::WifiConnected:    return "WIFI_CONNECTED";
    case LinkState::MqttConnected:    return "MQTT_CONNECTED";
    case LinkState::GprsAttaching:    return "GPRS_ATTACHING";
    case LinkState::GprsConnected:    return "GPRS_CONNECTED";
    case LinkState::MqttGprsConnected:return "MQTT_GPRS_CONNECTED";
  }
  return "UNKNOWN";
}

const char *transportLabel() {
  return gTransport == Transport::Gprs ? "GPRS" : "WIFI";
}

bool gprsAttached() { return gprs_link::attached(); }
const char *gprsIp() { return gprs_link::ip(); }

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
  if (out == nullptr || outSize == 0 || gCmdCount == 0) return false;

  strncpy(out, gCmdQueue[gCmdTail], outSize - 1);
  out[outSize - 1] = '\0';

  gCmdTail = (gCmdTail + 1) % kCmdQueueSize;
  gCmdCount--;
  return true;
}

const LinkStats &stats() { return gStats; }

} // namespace mqtt_link
