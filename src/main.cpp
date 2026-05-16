#include <Arduino.h>
#include "driver/adc.h"
#include "utils.h"

// PINS
#define MODULATION_PIN 18
#define ADC_PIN ADC1_CHANNEL_6
#define TEST_PIN 14

// CONFIG PARAMETERS
#define SAMPLE_RATE_HZ 10000 //Ratio SAMPLE_RATE_HZ/REF_FREQ_HZ must be 20
#define REF_FREQ_HZ 500 //Ratio SAMPLE_RATE_HZ/REF_FREQ_HZ must be 20
#define ADC_BUFFER_SIZE 1000

// ================= TIMERS ================= //
hw_timer_t *sample_timer = NULL;
hw_timer_t *modulation_timer = NULL;

// ================= ISRs ================= //

// MODULATION ISR  
volatile bool modulation_state = false;
void IRAM_ATTR modulation_isr() {
  modulation_state = !modulation_state;
  digitalWrite(MODULATION_PIN, modulation_state);
}

// SAMPLE ISR 
volatile uint32_t sample_count = 0;
void IRAM_ATTR sample_isr() {
  sample_count++;
}

// ================= LUT ================= //
static const float sin_lut[20] = {
    0.0000f, 0.3090f, 0.5878f, 0.8090f, 0.9511f,
    1.0000f, 0.9511f, 0.8090f, 0.5878f, 0.3090f,
    0.0000f, -0.3090f, -0.5878f, -0.8090f, -0.9511f,
    -1.0000f, -0.9511f, -0.8090f, -0.5878f, -0.3090f};
static const float cos_lut[20] = {
    1.0000f, 0.9511f, 0.8090f, 0.5878f, 0.3090f,
    0.0000f, -0.3090f, -0.5878f, -0.8090f, -0.9511f,
    -1.0000f, -0.9511f, -0.8090f, -0.5878f, -0.3090f,
    0.0000f, 0.3090f, 0.5878f, 0.8090f, 0.9511f};

// ================= Functions ================= //

// ================= Variables ================= //
volatile uint16_t adc_buffer[ADC_BUFFER_SIZE];
volatile uint32_t write_index = 0;
volatile bool buffer_ready = false;

bool beam_present = false;
uint32_t last_valid_time = 0;



void setup() {
  Serial.begin(115200);

  pinMode(MODULATION_PIN, OUTPUT);
  pinMode(TEST_PIN, OUTPUT);

  // ADC setup
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC_PIN, ADC_ATTEN_DB_12);

  // Modulation
  modulation_timer = timerBegin(0, 80, true); // 1 MHz tick
  timerAttachInterrupt(modulation_timer, &modulation_isr, true);
  timerAlarmWrite(modulation_timer, 1000, true); // 500 Hz square wave
  timerAlarmEnable(modulation_timer);

  // Sample
  sample_timer = timerBegin(1, 80, true); // 1 MHz tick
  timerAttachInterrupt(sample_timer, &sample_isr, true);
  timerAlarmWrite(sample_timer, 100, true); // 10 kHz 
  timerAlarmEnable(sample_timer);
}

void loop() {

  
  if (sample_count > 0 && !buffer_ready) {
    // Collect samples
    //digitalWrite(TEST_PIN,HIGH);

    noInterrupts();
    uint32_t n = sample_count;
    sample_count = 0;
    interrupts();

    while (n--) {
      if (write_index < ADC_BUFFER_SIZE) {
        adc_buffer[write_index++] = adc1_get_raw(ADC_PIN);
      }

      if (write_index >= ADC_BUFFER_SIZE) {
        buffer_ready = true;
        write_index = 0;
        break;
      }
    }
    //digitalWrite(TEST_PIN,LOW);
  }
  


  if (buffer_ready) {
    // Perform lock-in
    //digitalWrite(TEST_PIN,HIGH);

    double X = 0;
    double Y = 0;

    for (int i = 0; i < ADC_BUFFER_SIZE; i++) {
      int idx = i % 20;
      float s = adc_buffer[i];
      X += s * cos_lut[idx];
      Y += s * sin_lut[idx];
    }

    X /= ADC_BUFFER_SIZE;
    Y /= ADC_BUFFER_SIZE;
    float R = sqrt(X * X + Y * Y);
    float phi = atan2(Y, X);
    //digitalWrite(TEST_PIN,LOW);



    bool valid_signal =
        (isPhaseStable(phi))&&
        (isMagStable(R));

    if (valid_signal) {
      last_valid_time = millis();
      beam_present = true;
    } else {
      if (millis() - last_valid_time > 100) {
        beam_present = false;
      }
    }



    
    // -------- OUTPUT --------
    if (!beam_present) {
      Serial.println("🚨 BEAM LOST");
      Serial.print(" phi=");
      Serial.println(phi);
      Serial.print(" R=");
      Serial.println(R);
    
    } else {
      Serial.println("OK");
      Serial.print(" phi=");
      Serial.println(phi);
      Serial.print(" R=");
      Serial.println(R);
    }


    

    buffer_ready = false;

  }


}
