#pragma once

#include <Arduino.h>

// Conexión con broker_api por MQTT. Protocolo: broker_api/docs/MQTT.md
namespace mqtt_link {

struct Config {
  const char* host;
  uint16_t port;
  const char* deviceId;
  const char* token;
};

void begin(const Config& config);

// Llamar en cada vuelta del loop: reconecta con espera creciente y procesa mensajes
void loop();

bool connected();

// Publica el estado actual en devices/{id}/state
void publishState();

}  // namespace mqtt_link
