// Firmware del nodo: LED controlado desde broker_api por MQTT.
// Protocolo y tópicos: broker_api/docs/MQTT.md
#include <Arduino.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <time.h>

#include "app_config.h"
#include "led.h"
#include "mqtt_link.h"
#include "secrets.h"

namespace {
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
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.printf("\n[boot] Firmware %s\n", FIRMWARE_VERSION);

  // Estado seguro: el LED arranca apagado siempre
  led::begin();

  // Watchdog: reinicia el ESP32 si el loop deja de ejecutarse
  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(nullptr);

  // Wi-Fi con reconexión automática (no bloquea el arranque)
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  // Hora por NTP para descartar comandos vencidos (si no hay internet, se omite esa revisión)
  configTime(0, 0, "pool.ntp.org", "time.google.com");

  mqtt_link::begin({MQTT_HOST, MQTT_PORT, DEVICE_ID, DEVICE_TOKEN});
}

void loop() {
  esp_task_wdt_reset();
  logWifiChanges();
  mqtt_link::loop();

  // Latido: estado cada HEARTBEAT_INTERVAL_MS aunque no cambie
  if (mqtt_link::connected() && millis() - lastHeartbeat >= HEARTBEAT_INTERVAL_MS) {
    lastHeartbeat = millis();
    mqtt_link::publishState();
  }

  delay(10);
}
