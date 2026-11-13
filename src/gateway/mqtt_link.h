#pragma once

#include <Arduino.h>

// Conexión con broker_api por MQTT cifrado, con el cliente MQTT de ESP-IDF:
//   - wss://servidor:puerto/mqtt   MQTT sobre WebSocket (por defecto; mismo puerto que la API)
//   - mqtts://servidor:8883        MQTT por TCP con TLS
// En ambos casos valida el certificado del servidor con CA_CERTS (include/ca_cert.h)
// antes de enviar el token. Protocolo: broker_api/docs/MQTT.md
//
// Este módulo solo transporta: conecta, reconecta, entrega los comandos que llegan y
// publica lo que se le pasa. Qué dicen los mensajes lo deciden commands (comandos) y
// report (estado y ack); main.cpp los une.
namespace mqtt_link {

struct Config {
  String uri;       // wss://… o mqtts://… (DeviceConfig::mqttUri)
  String deviceId;
  String token;
  // Conectado y suscrito a los comandos: momento de publicar el estado
  void (*onConnected)();
  // Llegó un mensaje en devices/{id}/commands (se llama desde loop(), no desde la tarea MQTT)
  void (*onCommand)(const uint8_t* payload, size_t length);
  // Se llama tras MAX_AUTH_FAILURES rechazos seguidos (dispositivo borrado o token rotado)
  void (*onUnauthorized)();
};

void begin(const Config& config);

// Llamar en cada vuelta del loop: conecta cuando hay Wi-Fi, procesa los eventos y
// reconecta con espera creciente
void loop();

bool connected();

// Publican un JSON ya armado. false si no hay conexión.
bool publishState(const char* json, size_t length);  // devices/{id}/state, QoS 0
bool publishAck(const char* json, size_t length);    // devices/{id}/ack, QoS 1

}  // namespace mqtt_link
