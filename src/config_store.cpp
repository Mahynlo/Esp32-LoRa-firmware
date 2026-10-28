#include "config_store.h"

#include <Preferences.h>
#include <WiFi.h>
#include <esp_random.h>

#include "app_config.h"

namespace {
constexpr const char* NAMESPACE = "broker";
// Datos propios del equipo que sobreviven al borrado de la configuración
constexpr const char* DEVICE_NAMESPACE = "device";

// Sin 0/O ni 1/I/L: se lee y se escribe sin confusiones desde una etiqueta
constexpr const char* PASSWORD_ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789";

// "K7QMX-2PARN": PORTAL_PASSWORD_LENGTH caracteres al azar, con un guion a la mitad
String generatePassword() {
  const size_t alphabetSize = strlen(PASSWORD_ALPHABET);
  String password;
  for (int i = 0; i < PORTAL_PASSWORD_LENGTH; i++) {
    if (i == PORTAL_PASSWORD_LENGTH / 2) password += '-';
    password += PASSWORD_ALPHABET[esp_random() % alphabetSize];
  }
  return password;
}

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

void requestPortal() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putBool("portal", true);
  prefs.end();
}

bool takePortalRequest() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  bool requested = prefs.getBool("portal", false);
  if (requested) prefs.remove("portal");
  prefs.end();
  return requested;
}

String portalPassword() {
#ifdef CONFIG_AP_PASSWORD
  return CONFIG_AP_PASSWORD;
#else
  Preferences prefs;
  prefs.begin(DEVICE_NAMESPACE, false);
  String password = prefs.getString("portalPass", "");
  if (password.length() < 8) {
    password = generatePassword();
    prefs.putString("portalPass", password);
  }
  prefs.end();
  return password;
#endif
}

void clearAll() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.clear();
  prefs.end();
  WiFi.disconnect(true, true);  // borra también la red Wi-Fi guardada
}

}  // namespace config_store
