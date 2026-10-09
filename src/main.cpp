/*

.\rtl_433-rtlsdr-soapysdr.exe -d "driver = hackrf"  -f 433.92M -R 82

  ESP32-C3 LF Trigger for TPMS (Tire Pressure Monitoring System) - 125 kHz OOK
  To modify cheap EL-50448 which send only nonmodulated 125 kHz signal
  Default wake pattern for Continental or some VDO sensors (e.g. S180084730Z) is 0x61, 0x5E, 0x13, 0xC6, 0x6C, 0x39
  

 */

#include <Arduino.h>

#define LF_COIL_PIN 4
#define POWER_PIN 3
#define LED_PIN 5
#define LOW_BATT_LED_PIN 6
#define BATT_VOLTAGE_PIN 1

// WAKE PATTERN Continental or some VDO sensors (e.g. S180084730Z)
uint8_t wakePattern[] = {0x61, 0x5E, 0x13, 0xC6, 0x6C, 0x39}; 


// TIME CONFIGURATION VARIABLES (Modify these as needed)
static const uint32_t transmissionDurationMs = 10000; // Total window time (10 seconds)
static const uint32_t gapDurationMs = 5;              // Gap between bursts (5 milliseconds)

static const uint32_t LF_CARRIER_HZ = 125000;
static const uint8_t LF_OOK_DUTY = 128;  // 50% square when carrier ON

// PATTERN CONFIGURATION VARIABLES
uint8_t preamblenAndSyncPattern[] = {0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x55, 0x57, 0x8b, 0x32};
uint8_t wakeManchesterPattern[sizeof(wakePattern)*2]; // = {0x96, 0xa9, 0x99, 0x56, 0xa9, 0xa5, 0x5a, 0x96, 0x96, 0x5a, 0xa5, 0x69}
uint8_t localPattern[sizeof(preamblenAndSyncPattern) + sizeof(wakeManchesterPattern)]; // = preamblenAndSyncPattern[] + wakeManchesterPattern[]
constexpr uint8_t patternSize = sizeof(localPattern);


static const float BATT_DIVIDER_RATIO = (10000.0f + 1000.0f) / 1000.0f;
static const float LOW_BATT_THRESHOLD_V = 8.0f;
static const uint32_t LOW_BATT_BLINK_INTERVAL_MS = 500;
static const uint32_t BATT_SAMPLE_INTERVAL_MS = 1000;
float batteryVoltage = 0.0f;


hw_timer_t *timer = NULL;

// Volatile buffer for the active pattern being sent by the ISR
volatile uint8_t activePattern[patternSize]; 
volatile uint16_t totalBits = 0;
volatile uint16_t bitIndex = 0;
volatile bool isSending = false; 



void lfCarrierOff() {
  ledcWrite(LF_COIL_PIN, 0);
}

static inline void lfCarrierOn() {
  ledcWrite(LF_COIL_PIN, LF_OOK_DUTY);
}

// Interrupt Service Routine (ISR)
void IRAM_ATTR onTimer() {
  if (!isSending) {
    return;
  }

  // Keep the final bit on the output for a full timer interval before stopping.
  if (bitIndex >= totalBits) {
    bitIndex = 0;
    isSending = false;
    lfCarrierOff();
    return;
  }

  // 1. Calculate byte array tracking indexes
  uint8_t currentByteIndex = bitIndex / 8;
  uint8_t currentBitPosition = 7 - (bitIndex % 8); // MSB first
  
  // 2. Safely read from our active volatile buffer
  bool bitValue = (activePattern[currentByteIndex] >> currentBitPosition) & 0x01;
  
  // 3. Output to hardware pin
  if(bitValue) {
    lfCarrierOn();
  } else {
    lfCarrierOff();
  }
  
  // 4. Advance and wrap up when done
  bitIndex = bitIndex + 1;
}

// Function to trigger a custom pattern sequence from the loop
bool SendTriggerPattern(const uint8_t* patternArray, uint8_t arraySize) {
  if (isSending || arraySize > sizeof(activePattern)) return false; 
  
  // Critical Section: Temporarily pause interrupts to update variables safely
  noInterrupts();
  
  totalBits = arraySize * 8;
  bitIndex = 0;
  
  // Copy the local array pattern into the volatile ISR buffer
  for(uint8_t i = 0; i < arraySize; i++) {
    activePattern[i] = patternArray[i];
  }
  
  isSending = true;
  interrupts();
  
  return true;
}

void encodeWakePattern() {
  size_t outputBitIndex = 0;

  for (size_t byteIndex = 0; byteIndex < sizeof(wakePattern); byteIndex++) {
    for (int8_t bitPosition = 7; bitPosition >= 0; bitPosition--) {
      uint8_t pair = (wakePattern[byteIndex] & (1 << bitPosition)) ? 0b01 : 0b10;

      for (int8_t pairBit = 1; pairBit >= 0; pairBit--) {
        if ((pair >> pairBit) & 0x01) {
          wakeManchesterPattern[outputBitIndex / 8] |= 1 << (7 - (outputBitIndex % 8));
        }
        outputBitIndex++;
      }
    }
  }
}

void updateLowBatteryIndicator() {
  static uint32_t lastSampleTime = 0;
  static uint32_t lastBlinkTime = 0;
  static bool hasSampled = false;
  static bool lowBattery = false;
  static bool ledOn = false;
  const uint32_t now = millis();

  if (!hasSampled || now - lastSampleTime >= BATT_SAMPLE_INTERVAL_MS) {
    const float pinVoltage = analogReadMilliVolts(BATT_VOLTAGE_PIN) / 1000.0f;
    batteryVoltage = pinVoltage * BATT_DIVIDER_RATIO;
    lowBattery = batteryVoltage < LOW_BATT_THRESHOLD_V;
    lastSampleTime = now;
    hasSampled = true;

    if (!lowBattery) {
      ledOn = false;
      digitalWrite(LOW_BATT_LED_PIN, HIGH); // Turn off the low battery LED when voltage is sufficient
    }
  }

  if (lowBattery && now - lastBlinkTime >= LOW_BATT_BLINK_INTERVAL_MS) {
    ledOn = !ledOn;
    digitalWrite(LOW_BATT_LED_PIN, ledOn ? HIGH : LOW);
    lastBlinkTime = now;
  }
}


void setup() {

 
  pinMode(LED_PIN, OUTPUT);
  pinMode(LOW_BATT_LED_PIN, OUTPUT);
  digitalWrite(LOW_BATT_LED_PIN, HIGH); // Start with LED off (active low)
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  analogReadResolution(12);
  analogSetPinAttenuation(BATT_VOLTAGE_PIN, ADC_11db);
  Serial.begin(115200);

  // Configure hardware timer (1 MHz base frequency)
  timer = timerBegin(1000000); 
  timerAttachInterrupt(timer, &onTimer);
  
  // Set alarm threshold for 3900 Hz frequency, double for machester encoding
  uint32_t alarmValue = 1000000 / 3900 / 2; 
  timerAlarm(timer, alarmValue, true, 0); 
  
  Serial.println("System initialized. Active loop running...");
  delay(1000);

  if (!ledcAttach(LF_COIL_PIN, LF_CARRIER_HZ, 8)) {
    Serial.println("LF ERR: ledcAttach failed on GPIO - check LF_COIL_PIN");
  }
  //lfCarrierOn();
  lfCarrierOff();

  encodeWakePattern();
  for (size_t i = 0; i < sizeof(preamblenAndSyncPattern); i++) {
    localPattern[i] = preamblenAndSyncPattern[i];
  }
  for (size_t i = 0; i < sizeof(wakeManchesterPattern); i++) {
    localPattern[sizeof(preamblenAndSyncPattern) + i] = wakeManchesterPattern[i];
  }

  /* for (size_t i = 0; i < sizeof(localPattern); i++) {
    Serial.println(localPattern[i], HEX);
  } */

}

void loop() {
  updateLowBatteryIndicator();

  // State machine tracking variables
  static enum { STATE_IDLE, STATE_BURSTING, STATE_GAP } currentState = STATE_IDLE;
  static uint32_t macroStartTime = 0; 
  static uint32_t gapStartTime = 0;   
  
  // Start the transmission process window automatically upon boot
  if (currentState == STATE_IDLE) {
    Serial.printf(">>> STARTING %u MS TRANSMISSION WINDOW <<<\n", transmissionDurationMs);
    macroStartTime = millis();
    currentState = STATE_BURSTING;
    SendTriggerPattern(localPattern, patternSize); 
  }

  // Handle overall timeout boundaries based on transmissionDurationMs variable
  if (currentState == STATE_BURSTING || currentState == STATE_GAP) {
    if (millis() - macroStartTime >= transmissionDurationMs) { 
      isSending = false;
      lfCarrierOff();
      currentState = STATE_IDLE;
      Serial.println(">>> TRANSMISSION WINDOW COMPLETE. <<<");

      Serial.print("batteryVoltage: ");
      Serial.println(batteryVoltage);

      if (digitalRead(POWER_PIN) == HIGH) {
        Serial.println("Powering off");
        digitalWrite(POWER_PIN, LOW);
      }
      
      //while(true) { delay(1000); } // Stop the loop script here
      delay(1000); 
    }
  }

  // Manage transitions between bursting and the gap based on gapDurationMs variable
  if (currentState == STATE_BURSTING) {
    if (!isSending) {
      gapStartTime = millis(); 
      currentState = STATE_GAP;
    }
  } 
  else if (currentState == STATE_GAP) {
    if (millis() - gapStartTime >= gapDurationMs) {
      currentState = STATE_BURSTING;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN) );   // blink LED while burst active
      SendTriggerPattern(localPattern, patternSize); // Re-triggering using the local variables
    }
  }
  

}