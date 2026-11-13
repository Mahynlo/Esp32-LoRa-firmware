#pragma once

#include <Arduino.h>

// Resultado de procesar un comando recibido en devices/{id}/commands
struct CommandResult {
  bool reply;     // false = no responder (sin id o repetido)
  String id;      // id del comando, para el ack
  bool ok;
  String error;
};

namespace commands {
// Valida (lista blanca, repetido, antigüedad) y ejecuta el comando.
CommandResult handle(const uint8_t* payload, unsigned int length);
}
