#include "led.h"

#include <Arduino.h>

#include "app_config.h"

namespace {
bool state = false;
}

namespace led {

void begin() {
  pinMode(LED_PIN, OUTPUT);
  set(false);
}

void set(bool on) {
  state = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

bool isOn() { return state; }

}  // namespace led
