#include "mqtt_link.h"

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <esp_system.h>

#include "app_config.h"
#include "commands.h"
#include "led.h"

namespace {

WiFiClient net;
PubSubClient client(net);
mqtt_link::Config cfg;

String topicCommands, topicState, topicAck, topicStatus;

unsigned long reconnectDelay = 1000;
unsigned long nextAttemptAt = 0;
bool firstStateSent = false;
int authFailures = 0;

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

void publishAck(const CommandResult& result) {
  JsonDocument doc;
  doc["id"] = result.id;
  doc["ok"] = result.ok;
  if (!result.ok) doc["error"] = result.error;
  doc["state"]["led"] = led::isOn();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  client.publish(topicAck.c_str(), reinterpret_cast<const uint8_t*>(buffer), n, false);
}

void onMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (topicCommands != topic) return;
  CommandResult result = commands::handle(payload, length);
  if (!result.reply) return;
  publishAck(result);
  if (result.ok) mqtt_link::publishState();
}

bool connectNow() {
  Serial.printf("[mqtt] Conectando a %s:%u...\n", cfg.host.c_str(), cfg.port);
  // Última voluntad: si se corta la corriente, el broker publica "offline" por nosotros
  bool ok = client.connect(cfg.deviceId.c_str(), cfg.deviceId.c_str(), cfg.token.c_str(),
                           topicStatus.c_str(), 1, false, "offline", true);
  if (!ok) {
    int state = client.state();
    Serial.printf("[mqtt] Falló (estado %d)\n", state);
    // 4 = credenciales incorrectas, 5 = no autorizado: el dispositivo se borró o su token se rotó
    if (state == MQTT_CONNECT_BAD_CREDENTIALS || state == MQTT_CONNECT_UNAUTHORIZED) {
      if (++authFailures >= MAX_AUTH_FAILURES && cfg.onUnauthorized) cfg.onUnauthorized();
    } else {
      authFailures = 0;
    }
    return false;
  }

  authFailures = 0;
  Serial.println("[mqtt] Conectado");
  client.subscribe(topicCommands.c_str(), 1);
  client.publish(topicStatus.c_str(), "online", false);
  mqtt_link::publishState();
  return true;
}

}  // namespace

namespace mqtt_link {

void begin(const Config& config) {
  cfg = config;
  String base = String("devices/") + cfg.deviceId + "/";
  topicCommands = base + "commands";
  topicState = base + "state";
  topicAck = base + "ack";
  topicStatus = base + "status";

  // PubSubClient guarda el puntero: cfg es una copia que vive todo el programa
  client.setServer(cfg.host.c_str(), cfg.port);
  client.setKeepAlive(MQTT_KEEPALIVE_S);
  client.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  client.setBufferSize(MQTT_BUFFER_SIZE);
  client.setCallback(onMessage);
}

void loop() {
  if (client.connected()) {
    client.loop();
    return;
  }
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() < nextAttemptAt) return;

  if (connectNow()) {
    reconnectDelay = 1000;
  } else {
    nextAttemptAt = millis() + reconnectDelay;
    reconnectDelay = min(reconnectDelay * 2, static_cast<unsigned long>(RECONNECT_MAX_DELAY_MS));
  }
}

bool connected() { return client.connected(); }

void publishState() {
  if (!client.connected()) return;

  JsonDocument doc;
  doc["led"] = led::isOn();
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  doc["fw"] = FIRMWARE_VERSION;
  // El motivo del reinicio solo en el primer estado tras arrancar
  if (!firstStateSent) doc["reset"] = resetReason();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  if (client.publish(topicState.c_str(), reinterpret_cast<const uint8_t*>(buffer), n, false)) {
    firstStateSent = true;
  }
}

}  // namespace mqtt_link
