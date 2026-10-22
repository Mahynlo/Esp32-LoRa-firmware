#pragma once

#include "config_store.h"

// Portal de configuración: el ESP32 crea su propia red Wi-Fi ("ESP32-LORA-XXXX").
// Desde el teléfono se elige la red de la casa, la IP del servidor y el código de
// vinculación que muestra la app. El ESP32 canjea el código en broker_api
// (POST /api/devices/claim), guarda su id y token, y se reinicia ya vinculado.
namespace provisioning {

void start(const DeviceConfig& current);

// Llamar en cada vuelta del loop mientras el portal esté activo
void loop();

}  // namespace provisioning
