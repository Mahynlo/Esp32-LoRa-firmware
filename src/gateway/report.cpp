#include "report.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>

#include "gateway_config.h"
#include "mqtt_link.h"
#include "remote_motor.h"

namespace {

bool firstStateSent = false;

const char* resetReason() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON: return "POWERON";      // se encendió (o volvió la corriente)
    case ESP_RST_BROWNOUT: return "BROWNOUT";    // caída de voltaje
    case ESP_RST_TASK_WDT:
    case ESP_RST_INT_WDT:
    case ESP_RST_WDT: return "WATCHDOG";         // el firmware se congeló
    case ESP_RST_PANIC: return "PANIC";          // error del firmware
    case ESP_RST_SW: return "SOFTWARE";          // reinicio pedido por el firmware
    case ESP_RST_EXT: return "EXTERNAL";         // botón EN / reset
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    default: return "UNKNOWN";
  }
}

}  // namespace

namespace report {

void publishState() {
  JsonDocument doc;
  // motor: último estado confirmado por el nodo (no va hasta que responda la primera vez)
  if (remote_motor::known()) doc["motor"] = remote_motor::isOn();
  doc["nodeOnline"] = remote_motor::nodeOnline();
  if (remote_motor::nodeOnline()) {
    doc["loraRssi"] = remote_motor::linkRssi();
    doc["loraSnr"] = roundf(remote_motor::linkSnr() * 10) / 10;
  }
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  doc["fw"] = FIRMWARE_VERSION;
  // El motivo del reinicio solo en el primer estado tras arrancar
  if (!firstStateSent) doc["reset"] = resetReason();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  if (mqtt_link::publishState(buffer, n)) firstStateSent = true;
}

void publishAck(const CommandResult& result) {
  JsonDocument doc;
  doc["id"] = result.id;
  doc["ok"] = result.ok;
  if (!result.ok) doc["error"] = result.error;
  if (remote_motor::known()) doc["state"]["motor"] = remote_motor::isOn();
  doc["state"]["nodeOnline"] = remote_motor::nodeOnline();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  mqtt_link::publishAck(buffer, n);
}

}  // namespace report
