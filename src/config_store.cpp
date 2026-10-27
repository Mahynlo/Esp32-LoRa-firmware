#include "config_store.h"

#include <Preferences.h>
#include <WiFi.h>

#include "app_config.h"

namespace {
constexpr const char* NAMESPACE = "broker";

// Configuraciones guardadas antes del cifrado usaban los puertos sin TLS
void migrate(Preferences& prefs) {
  if (prefs.getUChar("cfgVer", 1) >= CONFIG_VERSION) return;
  if (prefs.getUShort("apiPort", DEFAULT_API_PORT) == 3000) prefs.putUShort("apiPort", DEFAULT_API_PORT);
  if (prefs.getUShort("mqttPort", DEFAULT_MQTT_PORT) == 1883) prefs.putUShort("mqttPort", DEFAULT_MQTT_PORT);
  prefs.putUChar("cfgVer", CONFIG_VERSION);
  Serial.println("[config] Configuración actualizada a los puertos cifrados (3443/8883)");
}
}  // namespace

namespace config_store {

DeviceConfig load() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  migrate(prefs);
  DeviceConfig config{
      prefs.getString("host", DEFAULT_SERVER_HOST),
      prefs.getUShort("apiPort", DEFAULT_API_PORT),
      prefs.getUShort("mqttPort", DEFAULT_MQTT_PORT),
      prefs.getString("deviceId", ""),
      prefs.getString("token", ""),
  };
  prefs.end();
  return config;
}

void save(const DeviceConfig& config) {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putString("host", config.host);
  prefs.putUShort("apiPort", config.apiPort);
  prefs.putUShort("mqttPort", config.mqttPort);
  prefs.putString("deviceId", config.deviceId);
  prefs.putString("token", config.token);
  prefs.putUChar("cfgVer", CONFIG_VERSION);
  prefs.end();
}

void clearCredentials() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.remove("deviceId");
  prefs.remove("token");
  prefs.end();
}

void clearAll() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.clear();
  prefs.end();
  WiFi.disconnect(true, true);  // borra también la red Wi-Fi guardada
}

}  // namespace config_store
