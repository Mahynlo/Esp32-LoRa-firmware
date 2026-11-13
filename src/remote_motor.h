#pragma once

#include <Arduino.h>

// Motor del nodo, controlado por LoRa desde el gateway. Protocolo: docs/LORA.md
namespace remote_motor {

// Inicia la radio. false si el módulo LoRa no responde (los comandos fallarán)
bool begin();

// Llamar en cada vuelta del loop: consulta el estado del nodo cada LORA_POLL_INTERVAL_MS
void loop();

struct Result {
  bool ok;
  const char* error;  // nullptr si ok
};

// Enciende o apaga el motor y espera la confirmación del nodo (bloquea hasta ~8 s)
Result set(bool on);

bool known();       // ¿ya se sabe el estado del motor? (el nodo respondió alguna vez)
bool isOn();        // último estado confirmado por el nodo
bool nodeOnline();
int linkRssi();     // con qué intensidad oye el gateway al nodo (dBm)
float linkSnr();

// ¿Cambió algo desde la última llamada? (motor o conexión del nodo) Para publicar el estado
bool takeChanged();

}  // namespace remote_motor
