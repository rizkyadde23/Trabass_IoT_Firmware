/*
 * Trabass IoT - Implementasi tautan GPRS SIM800L (M6)
 * ------------------------------------------------------------------
 * Urutan attach: AT -> modem.begin() -> CPIN? -> CSQ -> CREG?
 *                -> CSTT/CIICR (gprsConnect) -> CIFSR (IP)
 * Jika modem tidak balas AT, state machine menunggu GPRS_ATTACH_INTERVAL_MS
 * sebelum mencoba lagi, sehingga loop utama tetap hidup.
 */

#include "config.h"
#include "gprs_link.h"

namespace {

HardwareSerial simSerial(2);

TinyGsm &modem() {
  static TinyGsm m(simSerial);
  return m;
}

TinyGsmClient &gsmClient() {
  static TinyGsmClient c(modem());
  return c;
}

enum class Step : uint8_t {
  Idle,
  Probe,
  Init,
  SimReady,
  Signal,
  Register,
  Attach,
  Address,
  Online,
  Retry,
};

Step gStep = Step::Idle;
uint32_t gNextStepMs = 0;
uint32_t gNextCheckMs = 0;
uint32_t gAttempts = 0;
uint32_t gFailures = 0;
uint32_t gAttachedAtMs = 0;
bool gSwapped = false;
bool gRequested = false;
bool gAttached = false;
String gIp = "-";
int gCsq = 0;

void setStep(Step next, uint32_t delayMs = 0) {
  gStep = next;
  gNextStepMs = millis() + delayMs;
}

void applyPins() {
  const int rx = gSwapped ? SIM_TX_PIN : SIM_RX_PIN;
  const int tx = gSwapped ? SIM_RX_PIN : SIM_TX_PIN;
  simSerial.begin(SIM_BAUD, SERIAL_8N1, rx, tx);
  simSerial.setTimeout(1000);
}

// AT mentah, dipakai sebelum TinyGSM mengambil alih parser dan oleh menu.
bool rawAtTerm(const char *cmd, char *out, size_t outSize, uint32_t timeoutMs,
               const char *term) {
  if (cmd == nullptr || out == nullptr || outSize < 2) return false;

  // Hindari flush agresif: URC modem boleh dibuang setelah balasan dibaca.
  // Kirim per byte seperti tester passthrough yang terbukti stabil.
  for (const char *p = cmd; *p != '\0'; ++p) simSerial.write((uint8_t)*p);
  for (const char *p = term; *p != '\0'; ++p) simSerial.write((uint8_t)*p);

  out[0] = '\0';
  size_t n = 0;
  const uint32_t start = millis();
  while ((int32_t)(millis() - start) < (int32_t)timeoutMs &&
         n < outSize - 1) {
    while (simSerial.available()) {
      const char c = (char)simSerial.read();
      if (c == '\r' || c == '\n') continue;
      out[n++] = c;
      out[n] = '\0';
    }
    if (strstr(out, "OK") != nullptr || strstr(out, "ERROR") != nullptr) {
      break;
    }
    yield();
  }
  return strstr(out, "OK") != nullptr;
}

bool rawAt(const char *cmd, char *out, size_t outSize, uint32_t timeoutMs) {
  // Sama seperti tester .ino yang terbukti: Serial Monitor mengirim CRLF.
  return rawAtTerm(cmd, out, outSize, timeoutMs, "\r\n");
}

void gagal(const char *reason) {
  gFailures++;
  if ((gFailures % 3) == 1) {
    Serial.printf("[GPRS] GAGAL: %s (total=%lu)\n",
                  reason, (unsigned long)gFailures);
  }
  setStep(Step::Retry, GPRS_ATTACH_INTERVAL_MS);
}

} // namespace

namespace gprs_link {

void begin() {
  applyPins();
  Serial.printf("[GPRS] Siap APN=%s baud=%d RX=%d TX=%d (idle saat WiFi aktif)\n",
                GPRS_APN, SIM_BAUD,
                gSwapped ? SIM_TX_PIN : SIM_RX_PIN,
                gSwapped ? SIM_RX_PIN : SIM_TX_PIN);
  setStep(Step::Idle, 0);
}

void loop() {
  const uint32_t now = millis();

  if (gAttached) {
    if (GPRS_CHECK_INTERVAL_MS > 0 && now >= gNextCheckMs) {
      gNextCheckMs = now + GPRS_CHECK_INTERVAL_MS;
      if (!modem().isGprsConnected()) {
        Serial.println("[GPRS] Sesi data putus, attach ulang.");
        gAttached = false;
        gRequested = true;
        setStep(Step::Probe, 0);
      }
    }
    return;
  }

  // Safety net: permintaan attach boleh saja sudah ditandai (mis. dari
  // failover) sementara step kembali Idle setelah menu diagnostik 9/0.
  // Tanpa ini state machine diam walaupun modem sehat.
  if (gRequested && gStep == Step::Idle) setStep(Step::Probe, 0);

  if (gStep == Step::Idle) return;
  if ((int32_t)(now - gNextStepMs) < 0) return;

  switch (gStep) {
    case Step::Probe: {
#if GPRS_PROBE_BEFORE_ATTACH
      char resp[64];
      if (!rawAt("AT", resp, sizeof(resp), 1000)) {
        gAttempts++;
        gFailures++;
        if ((gFailures % 3) == 1) {
          Serial.printf("[GPRS] AT tidak merespons [%s], retry %d ms\n",
                        resp, GPRS_ATTACH_INTERVAL_MS);
        }
        setStep(Step::Retry, GPRS_ATTACH_INTERVAL_MS);
        return;
      }
#endif
      Serial.println("[GPRS] Modul merespons AT, mulai attach.");
      setStep(Step::Init, 0);
      break;
    }

    case Step::Init: {
      if (!modem().begin()) {
        gagal("modem.begin() gagal");
        return;
      }
      setStep(Step::SimReady, 0);
      break;
    }

    case Step::SimReady: {
      if (modem().getSimStatus() != SIM_READY) {
        gagal("SIM belum READY");
        return;
      }
      setStep(Step::Signal, 0);
      break;
    }

    case Step::Signal: {
      gCsq = modem().getSignalQuality();
      Serial.printf("[GPRS] SIM READY, CSQ=%d\n", gCsq);
      setStep(Step::Register, 0);
      break;
    }

    case Step::Register: {
      const RegStatus reg = modem().getRegistrationStatus();
      if (reg != RegStatus::REG_OK_HOME && reg != RegStatus::REG_OK_ROAMING) {
        Serial.printf("[GPRS] Belum terdaftar jaringan (reg=%d), retry.\n",
                      (int)reg);
        gagal("tidak terdaftar di jaringan");
        return;
      }
      const String op = modem().getOperator();
      Serial.printf("[GPRS] Terdaftar, operator=%s\n",
                    op.length() ? op.c_str() : "?");
      setStep(Step::Attach, 0);
      break;
    }

    case Step::Attach: {
      const char *user = GPRS_USER[0] != '\0' ? GPRS_USER : nullptr;
      const char *pass = GPRS_PASSWORD[0] != '\0' ? GPRS_PASSWORD : nullptr;
      if (!modem().gprsConnect(GPRS_APN, user, pass)) {
        gagal("GPRS attach gagal (APN/cakupan 2G)");
        return;
      }
      setStep(Step::Address, 0);
      break;
    }

    case Step::Address: {
      const String ip = modem().getLocalIP();
      if (ip.length() == 0 || ip == "0.0.0.0") {
        gagal("tidak mendapat IP");
        return;
      }
      gIp = ip;
      gAttached = true;
      gRequested = false;
      gAttachedAtMs = now;
      gNextCheckMs = now + GPRS_CHECK_INTERVAL_MS;
      gFailures = 0;
      Serial.printf("[GPRS] AKTIF IP=%s APN=%s CSQ=%d uptime=%lu ms\n",
                    gIp.c_str(), GPRS_APN, gCsq,
                    (unsigned long)gAttachedAtMs);
      setStep(Step::Online, 0);
      break;
    }

    case Step::Retry:
      setStep(Step::Probe, 0);
      break;

    case Step::Online:
    case Step::Idle:
      break;
  }
}

bool attached() { return gAttached; }

bool requestConnect() {
  if (gAttached) return true;
  if (!gRequested) {
    gRequested = true;
    Serial.println("[GPRS] Attach diminta.");
  }
  // Jangan hanya bergantung pada transisi gRequested: bila permintaan
  // sudah aktif tetapi step sedang Idle, tetap mulai ulang dari Probe.
  if (gStep == Step::Idle) setStep(Step::Probe, 0);
  return false;
}

void release() {
  if (!gAttached) return;
  gsmClient().stop();
  Serial.println("[GPRS] Sesi TCP dilepas; GPRS tetap ter-attach.");
}

void cancel() {
  if (gAttached) return;
  gRequested = false;
  setStep(Step::Idle, 0);
  Serial.println("[GPRS] Percobaan attach dihentikan (WiFi pulih).");
}

const char *stateLabel() {
  if (gAttached) return "ATTACHED";
  switch (gStep) {
    case Step::Idle:    return "IDLE";
    case Step::Probe:   return "PROBE";
    case Step::Init:    return "INIT";
    case Step::SimReady:return "SIM_READY";
    case Step::Signal:  return "SIGNAL";
    case Step::Register:return "REGISTER";
    case Step::Attach:  return "ATTACH";
    case Step::Address: return "GET_IP";
    case Step::Online:  return "ONLINE";
    case Step::Retry:   return "RETRY";
  }
  return "UNKNOWN";
}

const char *ip() { return gAttached ? gIp.c_str() : "-"; }
int csq() { return gCsq; }
uint32_t attempts() { return gAttempts; }
uint32_t failures() { return gFailures; }

TinyGsmClient &client() { return gsmClient(); }

bool debugAt(const char *cmd, char *out, size_t outSize, uint32_t timeoutMs) {
  return rawAt(cmd, out, outSize, timeoutMs);
}

bool swapPins() {
  gSwapped = !gSwapped;
  applyPins();
  gAttached = false;
  gRequested = false;
  gIp = "-";
  setStep(Step::Idle, 0);

  char resp[96];
  const bool ok = debugAt("AT", resp, sizeof(resp), 1500);
  Serial.printf("[SIM] Map=%s RX=%d TX=%d -> %s\n",
                pinLabel(),
                gSwapped ? SIM_TX_PIN : SIM_RX_PIN,
                gSwapped ? SIM_RX_PIN : SIM_TX_PIN,
                ok ? "OK" : resp);
  // Biarkan idle; user bisa minta attach lewat menu '6' atau failover.
  setStep(Step::Idle, 0);
  return ok;
}

const char *pinLabel() { return gSwapped ? "SWAPPED" : "NORMAL"; }

void scanUart() {
  if (gAttached) {
    Serial.println("[SIM-SCAN] GPRS aktif, scan dilewati.");
    return;
  }

  static const uint32_t kBauds[] =
      {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};
  Serial.println("[SIM-SCAN] 2 mapping x 8 baud x CR/CRLF (≈26 dtk)...");

  for (int m = 0; m < 2; m++) {
    gSwapped = (m == 1);
    applyPins();
    Serial.printf("  mapping=%s (RX=%d TX=%d)\n", pinLabel(),
                  gSwapped ? SIM_TX_PIN : SIM_RX_PIN,
                  gSwapped ? SIM_RX_PIN : SIM_TX_PIN);

    for (uint32_t baud : kBauds) {
      simSerial.updateBaudRate(baud);

      char r1[48];
      const bool ok1 = rawAtTerm("AT", r1, sizeof(r1), 700, "\r");
      char r2[48];
      const bool ok2 = rawAtTerm("AT", r2, sizeof(r2), 700, "\r\n");

      Serial.printf("    baud=%6lu CR=%s[%s] CRLF=%s[%s]\n",
                    (unsigned long)baud,
                    ok1 ? "OK" : "--", r1,
                    ok2 ? "OK" : "--", r2);
    }
  }

  gSwapped = false;
  gRequested = false;
  applyPins();
  simSerial.updateBaudRate(SIM_BAUD);
  setStep(Step::Idle, 0);
  Serial.printf("[SIM-SCAN] Selesai, kembali NORMAL baud=%d.\n", SIM_BAUD);
}

} // namespace gprs_link
