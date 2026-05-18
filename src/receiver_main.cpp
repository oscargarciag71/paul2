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

volatile uint32_t button_press_start = 0;
volatile bool button_pressed = false;


// =====================
// ALARM TIMER (2 min)
// =====================
unsigned long alarmStartTime = 0;
bool alarmActive = false;

// =====================
// YELLOW BLINK STATE
// =====================
int yellowBlinkCount = 0;
int yellowCurrentCount = 0;
bool yellowBlinking = false;
unsigned long yellowTimer = 0;
bool yellowLedState = false;

// =====================
// GREEN BLINK
// =====================
unsigned long greenTimer = 0;
bool greenLedState = false;

void setup() {

  delay(100);
  setCpuFrequencyMhz(160);
  delay(100);

  //Serial.begin(115200); 
  
  // Setup Buzzer
  ledcAttachPin(BUZZER_PIN, 0);   // channel 0
  ledcWriteTone(0, 1000);         // 1 kHz tone
  delay(100);
  ledcWriteTone(0, 0);
  ledcDetachPin(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(2000);


  // RESET SYSTEM STATE ON BOOT
  for (int i = 1; i <= 4; i++) {
    detAlarmFlag[i] = false;
    detCheckFlag[i] = false;
  }

  alarmActive = false;
  yellowBlinking = false;
  yellowLedState = false;
  greenLedState = false;

  pinMode(RLED_PIN, OUTPUT);
  pinMode(GLED_PIN, OUTPUT);
  pinMode(YLED_PIN, OUTPUT);

  digitalWrite(RLED_PIN, HIGH);
  digitalWrite(GLED_PIN, HIGH);
  digitalWrite(YLED_PIN, HIGH);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setTxPower(WIFI_POWER_MINUS_1dBm);
  initESPNowReceiver();
}


void loop()
{
    updateDetectorTimeouts();

    unsigned long now = millis();

    // =========================================================
    // CHECK STATES
    // =========================================================
    bool anyAlarm =
        detAlarmFlag[1] ||
        detAlarmFlag[2] ||
        detAlarmFlag[3] ||
        detAlarmFlag[4];

    bool anyOffline =
        !detCheckFlag[1] ||
        !detCheckFlag[2] ||
        !detCheckFlag[3] ||
        !detCheckFlag[4];

    bool allOK =
        detCheckFlag[1] &&
        detCheckFlag[2] &&
        detCheckFlag[3] &&
        detCheckFlag[4] &&
        !anyAlarm;

    // =========================================================
    // ALARM (RED + BUZZER 2 MIN)
    // =========================================================

    if (anyAlarm)
    {
        unsigned long start = millis();
        digitalWrite(YLED_PIN, HIGH); // ON (active low)
        digitalWrite(GLED_PIN, HIGH); // ON (active low)
        //Serial.println("ALARM");

        while (millis() - start < 120000) // 2 minutes
        {
            ledcAttachPin(BUZZER_PIN, 0);   
            digitalWrite(RLED_PIN, LOW); // ON (active low)
            ledcWriteTone(0, 1000);
            delay(150);
            digitalWrite(RLED_PIN, HIGH); // ON (active low)
            ledcWriteTone(0, 2000);
            delay(150);

            bool button_state = digitalRead(BUTTON_PIN) == LOW;
            if (button_state)
            {
                for(int i=1; i<=4; i++) {
                    detAlarmFlag[i] = false;
                }

                break;   // breaks the while loop
            }
        }
        ledcWriteTone(0, 0);
        ledcDetachPin(BUZZER_PIN);
        digitalWrite(RLED_PIN, HIGH);
    }

    // =========================================================
    // YELLOW LED (ANY OFFLINE → BLINK)
    // =========================================================
    static unsigned long yellowTimer = 0;
    static bool yellowState = false;

    if (anyOffline)
    {
        //Serial.println("OFFLINE");
        if (now - yellowTimer >= 500)
        {
            yellowTimer = now;
            yellowState = !yellowState;
            digitalWrite(YLED_PIN, yellowState ? LOW : HIGH);
        }
    }
    else
    {
        digitalWrite(YLED_PIN, HIGH);
        yellowState = false;
    }

    // =========================================================
    // GREEN LED (ALL OK → BLINK EVERY 5s)
    // =========================================================
    static unsigned long greenTimer = 0;
    static bool greenState = false;

    if (allOK)
    {
        //Serial.println("ALLOK");
        if (now - greenTimer >= 5000)
        {
            greenTimer = now;
            greenState = !greenState;
            digitalWrite(GLED_PIN, greenState ? LOW : HIGH);
        }
    }
    else
    {
        digitalWrite(GLED_PIN, HIGH);
        greenState = false;
    }

    bool button_state = digitalRead(BUTTON_PIN) == LOW;

    if (button_state) {
    if (!button_pressed) {
        button_pressed = true;
        button_press_start = millis();
    } 
    else {
        if (millis() - button_press_start >= 2000) {
        //Serial.println("Entering deep sleep...");
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
}