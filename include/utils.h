#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

#ifndef UTILS_H
#define UTILS_H

bool isPhaseStable(float phi);
bool isMagStable(float R);
bool isMagStable2(float R);


//
extern volatile bool detCheckFlag[5];
extern volatile bool detAlarmFlag[5];
extern unsigned long lastSeen[5];
void updateDetectorTimeouts();
//


// ===================== DATA =====================
typedef enum {
  STATUS_OK = 0,
  STATUS_ALARM = 1
} ModuleStatus;

typedef struct __attribute__((packed)) {
  uint32_t moduleID;
  uint8_t status;
} ModulePacket;

// ===================== API =====================
void initESPNow(uint32_t moduleID);
void sendStatus(uint32_t moduleID, ModuleStatus status);
void initESPNowReceiver();




#endif