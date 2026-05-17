#include <Arduino.h>
#include "driver/adc.h"
#include "utils.h"
#include <WiFi.h>
#include <esp_now.h>

// PINS
#define MODULATION_PIN 18
#define ADC_PIN ADC1_CHANNEL_6
#define TEST_PIN 14
#define BUZZER_PIN 17
#define RLED_PIN 25
#define GLED_PIN 26
#define YLED_PIN 27
#define BUTTON_PIN 14

#define BROADCAST_INTERVAL_MS 10000


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
volatile bool sample_flag = 0;
void IRAM_ATTR sample_isr() {
  sample_flag = true;
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


// ================= Variables ================= //
volatile uint16_t adc_buffer[ADC_BUFFER_SIZE];
volatile uint32_t write_index = 0;
volatile bool buffer_ready = false;

bool beam_present = false;
uint32_t last_valid_time = 0;

volatile uint32_t button_press_start = 0;
volatile bool button_pressed = false;

unsigned long lastSend = 0;

ModuleStatus status;

// ================= ========= ================= //

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_2dBm);


  initESPNow(1);  // module ID = 1
  delay(2000);

  pinMode(MODULATION_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(GLED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  digitalWrite(BUZZER_PIN, HIGH);
  digitalWrite(GLED_PIN, HIGH);

  ledcAttachPin(BUZZER_PIN, 0);   // channel 0
  ledcWriteTone(0, 1000);         // 1 kHz tone
  delay(200);
  ledcWrite(0, 0);
  ledcDetachPin(BUZZER_PIN);


  // Sleep mode setup
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // wake when LOW

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

  
  if (sample_flag && !buffer_ready) {
    // Collect samples
    sample_flag = false;

    if (write_index < ADC_BUFFER_SIZE) {
      adc_buffer[write_index++] = adc1_get_raw(ADC_PIN);
    }
    if (write_index >= ADC_BUFFER_SIZE) {
      buffer_ready = true;
      write_index = 0;
    }
  }


  if (buffer_ready) {
    // Perform lock-in
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

    if (isMagStable2(R)) {
      last_valid_time = millis();
      beam_present = true;
      digitalWrite(GLED_PIN,LOW);
    } 
    else {
      if (millis() - last_valid_time > 200) {
        beam_present = false;
        digitalWrite(GLED_PIN,HIGH);
        status = STATUS_ALARM;
        sendStatus(status);
      }
    }
    buffer_ready = false;
  }


  bool button_state = digitalRead(BUTTON_PIN) == LOW;
  if (button_state) {
    if (!button_pressed) {
      button_pressed = true;
      button_press_start = millis();
    } 
    else {
      if (millis() - button_press_start >= 2000) {
        ledcAttachPin(BUZZER_PIN,0);
        ledcWriteTone(0, 1000);         // lower tone
        delay(2000);
        ledcWriteTone(0, 0);           // stop PWM
        ledcWrite(0, 0);
        ledcDetachPin(BUZZER_PIN);
        delay(2000);
        esp_deep_sleep_start();
      }
    }
  } 
  else {
    button_pressed = false;
  }

  
  if (millis() - lastSend > BROADCAST_INTERVAL_MS) {
    if (beam_present){
    status = STATUS_OK;
    }  
    else {
    status = STATUS_ALARM;
    }
    sendStatus(status);
    lastSend = millis();
  }


}