#include "utils.h"
#include <math.h>

#define BUZZER_PIN 17
// returns TRUE if phase is stable
bool isPhaseStable(float phi) {

    static float last_phi = 0.0f;
    const float threshold = 0.1f;  // internal stability limit
    float phase_error = phi - last_phi;

    // wrap to [-PI, PI]
    if (phase_error > M_PI)  phase_error -= 2.0f * M_PI;
    if (phase_error < -M_PI) phase_error += 2.0f * M_PI;

    float phase_jitter = fabsf(phase_error);

    last_phi = phi;

    return (phase_jitter < threshold);
}

bool isMagStable(float R)
{
  static float ema = 0.0f;
  static bool init = false;
  const float minR  = 10.0f;  // hard cutoff
  const float alpha = 0.1f;

// hard failure condition
  if (R < minR) {
    return false;
  }


  if (!init) {
    ema = R;
    init = true;
  } else {
    ema = alpha * R + (1.0f - alpha) * ema;
  }

  return (R > 0.5f * ema);
}

bool isMagStable2(float R)
{
  static float ema = 0.0f;
  static bool init = false;
  static bool prev_stable = false;

  const float minR  = 10.0f;
  const float alpha = 0.1f;

  // ---- HARD FAILURE ----
  if (R < minR) {
    // transition: STABLE -> NOT STABLE
    if (prev_stable) {
        ledcWriteTone(0, 2000);         // 1 kHz tone
        delay(150);
        ledcWriteTone(0, 1000);         // 1 kHz tone
        delay(150);
        ledcWrite(0, 0);
    }

    prev_stable = false;
    return false;
  }

  // ---- EMA UPDATE ----
  if (!init) {
    ema = R;
    init = true;
  } else {
    ema = alpha * R + (1.0f - alpha) * ema;
  }

  bool stable = (R > 0.5f * ema);

  // ---- STATE TRANSITION DETECTION ----
  if (stable && !prev_stable) {
    // NOT STABLE -> STABLE
    ledcWriteTone(0, 1000);         // 1 kHz tone
    delay(150);
    ledcWriteTone(0, 2000);         // 1 kHz tone
    delay(150);
    ledcWrite(0, 0);
  }
  else if (!stable && prev_stable) {
    // STABLE -> NOT STABLE
    ledcWriteTone(0, 2000);         // 1 kHz tone
    delay(150);
    ledcWriteTone(0, 1000);         // 1 kHz tone
    delay(150);
    ledcWrite(0, 0);
  }

  prev_stable = stable;
  return stable;
}



static ModulePacket packet;
static uint8_t broadcastAddress[] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

void onDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  // optional debug
}

void initESPNow(uint32_t moduleID) {
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  esp_now_register_send_cb(onDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;

  esp_now_add_peer(&peerInfo);

  packet.moduleID = moduleID;
  packet.status = STATUS_OK;
}

void sendStatus(ModuleStatus status) {

  packet.status = status;

  esp_now_send(broadcastAddress,
               (uint8_t*)&packet,
               sizeof(packet));
}


/*
static void onDataRecv(const uint8_t *mac,
                       const uint8_t *incomingData,
                       int len) {

  if (len != sizeof(ModulePacket)) return;

  ModulePacket packet;
  memcpy(&packet, incomingData, sizeof(packet));

  int8_t id = packet.moduleID;

    // Only accept IDs 1–4
  if (id < 1 || id > 4) return;
    
  detCheckFlag[id] = true;
  detAlarmFlag[id] = (packet.status == STATUS_ALARM);


  Serial.print("Module ID: ");
  Serial.print(packet.moduleID);

  Serial.print(" | Status: ");

Serial.print("Detector ");
    Serial.print(id);

    Serial.print(" | Check: ");
    Serial.print(detCheckFlag[id]);

    Serial.print(" | Alarm: ");
    Serial.println(detAlarmFlag[id]);
}
*/



volatile bool detCheckFlag[5] = {false};
volatile bool detAlarmFlag[5] = {false};
unsigned long lastSeen[5] = {0};

static void onDataRecv(const uint8_t *mac,
                       const uint8_t *incomingData,
                       int len)
{
    if (len != sizeof(ModulePacket))
        return;

    ModulePacket packet;
    memcpy(&packet, incomingData, sizeof(packet));

    uint8_t id = packet.moduleID;

    if (id < 1 || id > 4)
        return;

    // Mark detector alive
    detCheckFlag[id] = true;

    // Store time we last heard from it
    lastSeen[id] = millis();

    // Update alarm state
    detAlarmFlag[id] =
        (packet.status == STATUS_ALARM);

    Serial.print("Detector ");
    Serial.print(id);

    Serial.print(" | Alive=");
    Serial.print(detCheckFlag[id]);

    Serial.print(" | Alarm=");
    Serial.println(detAlarmFlag[id]);
}

void updateDetectorTimeouts()
{
    unsigned long now = millis();

    for (int id = 1; id <= 4; id++)
    {
        if (detCheckFlag[id] &&
            (now - lastSeen[id] > 60000))
        {
            detCheckFlag[id] = false;

            Serial.print("Detector ");
            Serial.print(id);
            Serial.println(" timed out");
        }
    }
}

void initESPNowReceiver() {
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  esp_now_register_recv_cb(onDataRecv);

  Serial.println("ESP-NOW Receiver Ready");
}