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
        ledcAttachPin(BUZZER_PIN, 0);   // channel 0
        ledcWriteTone(0, 2000);         // 1 kHz tone
        delay(150);
        ledcWriteTone(0, 1000);         // 1 kHz tone
        delay(150);
        ledcWrite(0, 0);
        ledcDetachPin(BUZZER_PIN);
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
    ledcAttachPin(BUZZER_PIN, 0);   // channel 0
    ledcWriteTone(0, 1000);         // 1 kHz tone
    delay(150);
    ledcWriteTone(0, 2000);         // 1 kHz tone
    delay(150);
    ledcWrite(0, 0);
    ledcDetachPin(BUZZER_PIN);
  }
  else if (!stable && prev_stable) {
    // STABLE -> NOT STABLE
    ledcAttachPin(BUZZER_PIN, 0);   // channel 0
    ledcWriteTone(0, 2000);         // 1 kHz tone
    delay(150);
    ledcWriteTone(0, 1000);         // 1 kHz tone
    delay(150);
    ledcWrite(0, 0);
    ledcDetachPin(BUZZER_PIN);
  }

  prev_stable = stable;
  return stable;
}