#pragma once

#include "commands.h"

// Lo que el gateway le informa a broker_api: arma los JSON y los publica por mqtt_link.
// Campos y tópicos: broker_api/docs/MQTT.md. Para agregar un dato al estado, es aquí.
namespace report {

// Estado en devices/{id}/state: motor, conexión del nodo, señal LoRa y Wi-Fi, versión...
// Se publica al conectar, cuando algo cambia y en cada latido.
void publishState();

// Respuesta a un comando en devices/{id}/ack: si se ejecutó o el error
void publishAck(const CommandResult& result);

}  // namespace report
