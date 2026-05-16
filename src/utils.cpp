#include "utils.h"
#include <math.h>

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