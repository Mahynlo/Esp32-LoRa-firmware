// Firmware del nodo: LED controlado desde broker_api por MQTT.
// Protocolo y tópicos: broker_api/docs/MQTT.md
//
// Arranque:
//   - Sin configurar: portal de configuración "ESP32-LORA-XXXX" para elegir Wi-Fi,
//     servidor y código de vinculación.
//   - Configurado: se conecta al Wi-Fi guardado y al broker MQTT.
//   - BOOT 5 s con el equipo funcionando: portal para cambiar Wi-Fi o servidor,
//     conservando la vinculación.
//   - BOOT presionado justo al encender, 3 s: borra todo y abre el portal.
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

// BOOT mantenido CONFIG_PORTAL_HOLD_MS con el equipo funcionando: reiniciar en el portal
// de configuración, conservando Wi-Fi y vinculación (se cambia solo lo que se escriba)
void checkPortalButton() {
  static unsigned long pressedSince = 0;
  static bool pressed = false;
  if (digitalRead(CONFIG_BUTTON_PIN) != LOW) {
    if (pressed) Serial.println("[boot] BOOT soltado antes de tiempo: no se abre el portal");
    pressed = false;
    return;
  }
  if (!pressed) {
    pressed = true;
    pressedSince = millis();
    Serial.printf("[boot] BOOT presionado: mantenlo %d s para abrir el portal de configuración\n",
                  CONFIG_PORTAL_HOLD_MS / 1000);
    return;
  }
  if (millis() - pressedSince < CONFIG_PORTAL_HOLD_MS) return;

  Serial.println("[boot] Abriendo el portal de configuración...");
  config_store::requestPortal();
  led::set(false);
  delay(200);
  ESP.restart();
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

  // Con el Wi-Fi iniciado, esp_random() usa el ruido de la radio: la clave es realmente al azar
  Serial.printf("[boot] Clave del portal de este equipo: %s (anótala en una etiqueta)\n",
                config_store::portalPassword().c_str());

  // Sin vincular, o se pidió con BOOT 5 s: portal de configuración
  if (!config.paired() || config_store::takePortalRequest()) {
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

  checkPortalButton();
  logWifiChanges();
  mqtt_link::loop();

  // Latido: estado cada HEARTBEAT_INTERVAL_MS aunque no cambie
  if (mqtt_link::connected() && millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = millis();
    mqtt_link::publishState();
  }

  delay(10);
}
