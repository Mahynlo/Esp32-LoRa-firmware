// Firmware del nodo: LED controlado desde broker_api por MQTT.
// Protocolo y tópicos: broker_api/docs/MQTT.md
//
// Arranque:
//   - Sin configurar (o BOOT presionado 3 s al encender): portal de configuración
//     "ESP32-LORA-XXXX" para elegir Wi-Fi, servidor y código de vinculación.
//   - Configurado: se conecta al Wi-Fi guardado y al broker MQTT.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "app_config.h"
#include "config_store.h"
#include "led.h"
#include "mqtt_link.h"
#include "provisioning.h"

namespace {
DeviceConfig config;
bool configuring = false;
unsigned long lastHeartbeat = 0;
wl_status_t lastWifiStatus = WL_IDLE_STATUS;

void logWifiChanges() {
  wl_status_t status = WiFi.status();
  if (status == lastWifiStatus) return;
  lastWifiStatus = status;
  if (status == WL_CONNECTED) {
    Serial.printf("[wifi] Conectado, IP %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("[wifi] Sin conexión, reintentando...");
  }
}

// ¿BOOT presionado durante CONFIG_BUTTON_HOLD_MS al encender?
bool configButtonHeld() {
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  if (digitalRead(CONFIG_BUTTON_PIN) != LOW) return false;
  Serial.println("[boot] BOOT presionado: mantenlo para borrar la configuración...");
  unsigned long start = millis();
  while (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
    if (millis() - start >= CONFIG_BUTTON_HOLD_MS) return true;
    delay(20);
  }
  return false;
}

// El broker rechazó las credenciales varias veces: el dispositivo ya no existe en la app
void onUnauthorized() {
  Serial.println("[mqtt] Credenciales rechazadas: se borran y se vuelve al portal de configuración");
  config_store::clearCredentials();
  ESP.restart();
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[boot] Firmware %s\n", FIRMWARE_VERSION);

  // Estado seguro: el LED arranca apagado siempre
  led::begin();

  // Watchdog: reinicia el ESP32 si el loop deja de ejecutarse
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(nullptr);

  if (configButtonHeld()) {
    Serial.println("[boot] Configuración borrada");
    config_store::clearAll();
  }

  config = config_store::load();
  WiFi.mode(WIFI_STA);

  if (!config.paired()) {
    configuring = true;
    provisioning::start(config);
    return;
  }

  // Wi-Fi guardado con reconexión automática (no bloquea el arranque)
  WiFi.setAutoReconnect(true);
  WiFi.begin();

  // Hora por NTP para descartar comandos vencidos (si no hay internet, se omite esa revisión)
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  Serial.printf("[boot] Dispositivo %s, servidor %s\n", config.deviceId.c_str(), config.host.c_str());
  mqtt_link::begin({config.host, config.mqttPort, config.deviceId, config.token, onUnauthorized});
}

void loop() {
  esp_task_wdt_reset();

  if (configuring) {
    provisioning::loop();
    delay(10);
    return;
  }

  logWifiChanges();
  mqtt_link::loop();

  // Latido: estado cada HEARTBEAT_INTERVAL_MS aunque no cambie
  if (mqtt_link::connected() && millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = millis();
    mqtt_link::publishState();
  }

  delay(10);
}
