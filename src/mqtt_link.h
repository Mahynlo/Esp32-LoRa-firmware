#pragma once

#include <Arduino.h>

// Conexión con broker_api por MQTTS (TLS, validando el certificado del servidor con
// CA_CERTS de include/ca_cert.h). Protocolo: broker_api/docs/MQTT.md
namespace mqtt_link {

struct Config {
  String host;
  uint16_t port;
  String deviceId;
  String token;
  // Se llama tras MAX_AUTH_FAILURES rechazos seguidos (dispositivo borrado o token rotado)
  void (*onUnauthorized)();
};

void begin(const Config& config);

// Llamar en cada vuelta del loop: reconecta con espera creciente y procesa mensajes
void loop();

bool connected();

// Publica el estado actual en devices/{id}/state
void publishState();

}  // namespace mqtt_link
