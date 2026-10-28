#pragma once

#include <Arduino.h>

// Configuración persistente en la memoria no volátil (NVS) del ESP32.
// La red Wi-Fi la guarda el propio ESP32 (WiFiManager); aquí va lo de broker_api.
struct DeviceConfig {
  String host;         // IP o nombre de la PC con broker_api
  uint16_t apiPort;    // puerto HTTPS (para canjear el código)
  uint16_t mqttPort;   // puerto MQTTS
  String deviceId;     // usuario MQTT (lo entrega /claim)
  String token;        // contraseña MQTT (lo entrega /claim)

  bool paired() const { return deviceId.length() > 0 && token.length() > 0; }
};

namespace config_store {
DeviceConfig load();
void save(const DeviceConfig& config);
// Borra id y token (el dispositivo fue eliminado en la app): vuelve a pedir un código
void clearCredentials();
// Borra todo, incluida la red Wi-Fi (botón de reconfiguración)
void clearAll();
// Pide abrir el portal en el próximo arranque (BOOT 5 s con el equipo funcionando)
void requestPortal();
// ¿Se pidió abrir el portal? Lee y borra el pedido
bool takePortalRequest();
// Clave de la red del portal: propia de cada equipo, se genera la primera vez y no se
// borra nunca (tampoco con clearAll). Llamar con el Wi-Fi ya iniciado (azar de hardware).
String portalPassword();
}
