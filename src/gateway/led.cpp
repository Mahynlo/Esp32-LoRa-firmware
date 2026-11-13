#include "led.h"

#include <Arduino.h>

#include "pins.h"

namespace {
bool state = false;
}

namespace led {

void begin() {
  pinMode(STATUS_LED_PIN, OUTPUT);
  set(false);
}

void set(bool on) {
  state = on;
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

bool isOn() { return state; }

}  // namespace led
