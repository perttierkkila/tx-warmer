/****************************************************************
 * TX-warmer firmware v1.0, GPLv3-licence
 * Author    Pertti Erkkilä (pertti.erkkila@gmail.com)
 * Hardware  http://www.kopterit.net/index.php/topic,17572.0.html
 ****************************************************************/

#include <PinChangeInterrupt.h>

// TODO
// pwm prescaler
// sleep-mode
// sizeof-array

// heater element resistance, mOhm
const long HEATER_RESISTANCE = 1900; 

// heater element power settings, watts
const int HEATER_POWER[] = {
  10, 20, 30, 40, 50};
// count of HEATER_POWER-elements
const int HEATER_POWER_SIZE = 5;

// battery resistor divider, kOhm
const int ADC_RESISTOR_HIGH = 100;
const int ADC_RESISTOR_LOW = 56;

// Lipo-specific settings per cell, mV
const int LIPO_CUTOFF_VOLTAGE = 3750;
const int LIPO_CRITICAL_VOLTAGE = 3600;

// ATtiny pin mappings
// for ADC, use ADC-channel number
// other pins, running order skipping VCC, RESET and GND (eg pin2 = 1)
const int PIN_BATTERY = PA7; // A0 for atmega-dev
const int PIN_GREEN = 10;
const int PIN_RED = 9; // 12 for atmega-dev
const int PIN_PWM = 2;
const int PIN_BUTTON = 8;

// selected power-stage
// > 0: selected power stage from array
//  0: PWM off, battery OK
// -1: cutoff voltage triggered
// -2: critical voltage triggered
const int STAGE_OK = 0;
const int STAGE_CUTOFF = -1;
const int STAGE_CRITICAL = -2;
volatile int stage = STAGE_OK;

// current dutyCycle, valid range 0-255
volatile int dutyCycle = 0;

// Battery saving voltages, mV
// below cutoff = stop PWM, blink led
volatile long cutoffVoltage = 0;
// below critical = shutdown everything, sleep-mode
volatile long criticalVoltage = 0;

// for button debounce, ms
const int BUTTON_DEBOUNCE_DURATION = 50;
volatile unsigned long buttonLastPressed = 0;
volatile boolean stageChanged = false;

void setup() {
  // wait for user after plugging battery
  delay(1000);

  // pin setup
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_PWM, OUTPUT);
  pinMode(PIN_BATTERY, INPUT);
  pinMode(PIN_BUTTON, INPUT);
  digitalWrite(PIN_BUTTON, HIGH); // enable INPUT_PULLUP, not supported with attiny

  // calculate cutoff and critical voltages
  long batteryVoltage = readVoltage();
  int cellCount = calculateCellCount(batteryVoltage);

  // request at least 2 cells (LDO shouldn't even work = error)
  if (cellCount < 1)
    stage = STAGE_CUTOFF;

  cutoffVoltage = cellCount * LIPO_CUTOFF_VOLTAGE;
  criticalVoltage = cellCount * LIPO_CRITICAL_VOLTAGE;

  // indicate cell count by blinking both leds
  for (int i = 0; i < cellCount; i++) {
    digitalWrite(PIN_GREEN, HIGH);
    digitalWrite(PIN_RED, HIGH);
    delay(100);
    digitalWrite(PIN_GREEN, LOW);
    digitalWrite(PIN_RED, LOW);
    delay(750);
  }

  attachPcInterrupt(PIN_BUTTON, buttonPressed, FALLING);
  delay(500);
}

void loop() {
  if (stage == STAGE_CRITICAL) {
    // critical-stage, sleep
    delay(1000);
    return;
  }

  // check battery and adjust stage if needed
  long batteryVoltage = readVoltage();
  if (batteryVoltage < criticalVoltage) {
    // critical-stage reached, pwm and leds off
    stage = STAGE_CRITICAL;
    analogWrite(PIN_PWM, 0);
    digitalWrite(PIN_GREEN, LOW);
    digitalWrite(PIN_RED, LOW);
    return;
  }
  if (batteryVoltage < cutoffVoltage && stage > STAGE_CUTOFF) {
    // cutoff-stage reached, pwm off
    stage = STAGE_CUTOFF;
    analogWrite(PIN_PWM, 0);
  }

  if (stage == STAGE_CUTOFF) {
    // cutoff-stage, blink green led, red off
    digitalWrite(PIN_RED, LOW);
    digitalWrite(PIN_GREEN, HIGH);
    delay(10);
    digitalWrite(PIN_GREEN, LOW);
    delay(1500);
    return;
  }

  // running fine, keep green on
  digitalWrite(PIN_GREEN, HIGH);

  // calculate dutycycle and change if needed
  int newDutyCycle = calculateDutyCycle(batteryVoltage);
  if (dutyCycle != newDutyCycle) {
    dutyCycle = newDutyCycle;
    analogWrite(PIN_PWM, newDutyCycle);
  }

  // blink red according dutycycle, total led cycle duration 1.6secs
  if (dutyCycle > 0) {
    digitalWrite(PIN_RED, HIGH);
    stageChangeAwareDelay(dutyCycle * 6);
  }
  digitalWrite(PIN_RED, LOW);
  stageChangeAwareDelay((255 - dutyCycle) * 6);

  // reset variable
  stageChanged = false;
}

void stageChangeAwareDelay(int duration) {
  for (int i = 0; i < (duration / 10); i++) {
    if (stageChanged == true) return;
    delay(10);
  }
}

void buttonPressed() {
  // stage check, not applicable when cutoff reached
  if (stage < 0) return;

  // check debounce
  unsigned long time = millis();
  if ((time - BUTTON_DEBOUNCE_DURATION) < buttonLastPressed) return;
  buttonLastPressed = time;

  // next stage and check overflow
  stage++;
  if (stage > HEATER_POWER_SIZE)
    stage = 0;

  // break blinding delay
  stageChanged = true;
}

long readVoltage() {
  return readADC(PIN_BATTERY, ADC_RESISTOR_HIGH, ADC_RESISTOR_LOW);
}

// calculate dutycycle according stage (1-based index)
int calculateDutyCycle(long voltage) {
  if (stage <= STAGE_OK || stage > HEATER_POWER_SIZE)
    return 0;
  return calculateDutyCycle(voltage, HEATER_RESISTANCE, HEATER_POWER[stage-1]);
}

//-------------------------------------------------
// Library stuff

// calculate lipo-cell count
// voltage=mV
int calculateCellCount(long voltage) {
  if (voltage <= 0)
    return 0;
  return (voltage - 100) / 4200 + 1;
}

// calculate PWM dutyCycle, output 0-255
// voltage=mV, resistance=mOhm, power=W
int calculateDutyCycle(long voltage, long resistance, long power) {
  if (voltage < 1 || resistance < 1 || power < 1)
    return 0;
  long wantedCurrent  = 1000000 * power / voltage;
  long maxCurrent = 1000 * voltage / resistance;

  if (wantedCurrent >= maxCurrent)
    return 255;
  return 255 * wantedCurrent / maxCurrent;
}

// Read ADC five times and calculate resistor divider
long readADC(int pin, int resistorHigh, int resistorLow) {
  // read 5 samples
  long result = 0;
  for (int i = 0; i < 5; i++) {
    delay(10);
    result += analogRead(pin);
  }
  // to mV (assumed 5V reference)
  result = (1000 * result) / 1023;

  // solve divider
  result = (result * (resistorHigh + resistorLow)) / resistorLow;
  return result;
}























