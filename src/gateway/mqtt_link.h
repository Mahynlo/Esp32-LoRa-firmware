#pragma once

#include <Arduino.h>

// Conexión con broker_api por MQTT cifrado, con el cliente MQTT de ESP-IDF:
//   - wss://servidor:puerto/mqtt   MQTT sobre WebSocket (por defecto; mismo puerto que la API)
//   - mqtts://servidor:8883        MQTT por TCP con TLS
// En ambos casos valida el certificado del servidor con CA_CERTS (include/ca_cert.h)
// antes de enviar el token. Protocolo: broker_api/docs/MQTT.md
namespace mqtt_link {

struct Config {
  String uri;       // wss://… o mqtts://… (DeviceConfig::mqttUri)
  String deviceId;
  String token;
  // Se llama tras MAX_AUTH_FAILURES rechazos seguidos (dispositivo borrado o token rotado)
  void (*onUnauthorized)();
};

void begin(const Config& config);

// Llamar en cada vuelta del loop: conecta cuando hay Wi-Fi, procesa los eventos y
// reconecta con espera creciente
void loop();

bool connected();

// Publica el estado actual en devices/{id}/state
void publishState();

}  // namespace mqtt_link
