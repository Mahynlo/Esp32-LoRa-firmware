#include "mqtt_link.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>
#include <freertos/queue.h>
#include <mbedtls/error.h>
#include <mqtt_client.h>

#include "app_config.h"
#include "ca_cert.h"
#include "commands.h"
#include "led.h"

namespace {

// El cliente MQTT de ESP-IDF corre en su propia tarea. Sus eventos se copian a esta
// cola y se atienden en el loop: comandos, LED y estado se manejan en un solo hilo.
enum class EventType : uint8_t { Connected, Disconnected, AuthRejected, TransportError, Command };

struct Event {
  EventType type;
  int tlsError;      // TransportError: código de mbedTLS (0 si no fue TLS)
  int socketErrno;   // TransportError: errno del socket
  uint16_t length;   // Command: bytes en data
  char data[MQTT_BUFFER_SIZE];
};

constexpr int EVENT_QUEUE_LENGTH = 4;

esp_mqtt_client_handle_t client = nullptr;
QueueHandle_t events = nullptr;
volatile bool isConnected = false;
mqtt_link::Config cfg;

String topicCommands, topicState, topicAck, topicStatus;

bool started = false;
bool waitingReconnect = false;
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

bool publish(const String& topic, const char* data, size_t length, int qos) {
  if (!isConnected) return false;
  return esp_mqtt_client_publish(client, topic.c_str(), data, static_cast<int>(length), qos, 0) >= 0;
}

void publishAck(const CommandResult& result) {
  JsonDocument doc;
  doc["id"] = result.id;
  doc["ok"] = result.ok;
  if (!result.ok) doc["error"] = result.error;
  doc["state"]["led"] = led::isOn();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  publish(topicAck, buffer, n, 1);
}

// ── Tarea MQTT: solo copia lo necesario a la cola ──────────────────────────

void queueEvent(const Event& event) {
  // Cola llena: se descarta (un comando perdido no recibe ack y la app marca "sin respuesta")
  xQueueSend(events, &event, 0);
}

void onMqttEvent(void*, esp_event_base_t, int32_t eventId, void* eventData) {
  auto* data = static_cast<esp_mqtt_event_handle_t>(eventData);
  Event event{};

  switch (static_cast<esp_mqtt_event_id_t>(eventId)) {
    case MQTT_EVENT_CONNECTED:
      isConnected = true;
      event.type = EventType::Connected;
      queueEvent(event);
      break;

    case MQTT_EVENT_DISCONNECTED:
      isConnected = false;
      event.type = EventType::Disconnected;
      queueEvent(event);
      break;

    case MQTT_EVENT_ERROR: {
      const esp_mqtt_error_codes_t* error = data->error_handle;
      if (error->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        // 4 = usuario incorrecto, 5 = no autorizado: el dispositivo se borró o su token se rotó
        auto code = error->connect_return_code;
        if (code == MQTT_CONNECTION_REFUSE_BAD_USERNAME || code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED) {
          event.type = EventType::AuthRejected;
          queueEvent(event);
        }
      } else if (error->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        event.type = EventType::TransportError;
        event.tlsError = error->esp_tls_stack_err;
        event.socketErrno = error->esp_transport_sock_errno;
        queueEvent(event);
      }
      break;
    }

    case MQTT_EVENT_DATA:
      // Un comando llega completo en un solo evento (son de pocos bytes)
      if (data->current_data_offset != 0 || data->data_len != data->total_data_len) break;
      if (data->data_len <= 0 || data->data_len >= static_cast<int>(sizeof(event.data))) break;
      if (topicCommands.length() != static_cast<unsigned>(data->topic_len) ||
          strncmp(topicCommands.c_str(), data->topic, data->topic_len) != 0) {
        break;
      }
      event.type = EventType::Command;
      event.length = static_cast<uint16_t>(data->data_len);
      memcpy(event.data, data->data, data->data_len);
      queueEvent(event);
      break;

    default:
      break;
  }
}

// ── Loop: atiende los eventos ──────────────────────────────────────────────

void scheduleReconnect() {
  waitingReconnect = true;
  nextAttemptAt = millis() + reconnectDelay;
  Serial.printf("[mqtt] Reintento en %lu s\n", reconnectDelay / 1000);
  reconnectDelay = min(reconnectDelay * 2, static_cast<unsigned long>(RECONNECT_MAX_DELAY_MS));
}

void handleEvent(const Event& event) {
  switch (event.type) {
    case EventType::Connected:
      authFailures = 0;
      reconnectDelay = 1000;
      waitingReconnect = false;
      Serial.println("[mqtt] Conectado");
      esp_mqtt_client_subscribe(client, topicCommands.c_str(), 1);
      publish(topicStatus, "online", 6, 1);
      mqtt_link::publishState();
      break;

    case EventType::Disconnected:
      Serial.println("[mqtt] Desconectado");
      scheduleReconnect();
      break;

    case EventType::AuthRejected:
      Serial.println("[mqtt] El broker rechazó las credenciales");
      if (++authFailures >= MAX_AUTH_FAILURES && cfg.onUnauthorized) cfg.onUnauthorized();
      break;

    case EventType::TransportError:
      // Fallo de red o de TLS: no es un rechazo de credenciales, se reintenta sin borrar nada
      authFailures = 0;
      if (event.tlsError != 0) {
        char text[100];
        mbedtls_strerror(event.tlsError, text, sizeof(text));
        Serial.printf("[mqtt] TLS: %s\n", text);
      } else {
        Serial.printf("[mqtt] Sin conexión con el servidor (errno %d)\n", event.socketErrno);
      }
      break;

    case EventType::Command: {
      CommandResult result = commands::handle(reinterpret_cast<const uint8_t*>(event.data), event.length);
      if (!result.reply) break;
      publishAck(result);
      if (result.ok) mqtt_link::publishState();
      break;
    }
  }
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

  events = xQueueCreate(EVENT_QUEUE_LENGTH, sizeof(Event));

  // El cliente copia URI, usuario, contraseña y última voluntad; CA_CERTS es estático
  esp_mqtt_client_config_t mqttConfig = {};
  mqttConfig.uri = cfg.uri.c_str();
  mqttConfig.client_id = cfg.deviceId.c_str();
  mqttConfig.username = cfg.deviceId.c_str();
  mqttConfig.password = cfg.token.c_str();
  mqttConfig.cert_pem = CA_CERTS;              // valida el certificado del servidor
  mqttConfig.keepalive = MQTT_KEEPALIVE_S;
  mqttConfig.network_timeout_ms = TLS_HANDSHAKE_TIMEOUT_S * 1000;
  mqttConfig.buffer_size = MQTT_BUFFER_SIZE;
  mqttConfig.disable_auto_reconnect = true;    // la espera creciente la maneja loop()
  // Última voluntad: si se corta la corriente, el broker publica "offline" por nosotros
  mqttConfig.lwt_topic = topicStatus.c_str();
  mqttConfig.lwt_msg = "offline";
  mqttConfig.lwt_qos = 1;
  mqttConfig.lwt_retain = 0;

  client = esp_mqtt_client_init(&mqttConfig);
  esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, onMqttEvent, nullptr);
}

void loop() {
  // Primera conexión: en cuanto haya Wi-Fi
  if (!started) {
    if (WiFi.status() != WL_CONNECTED) return;
    Serial.printf("[mqtt] Conectando a %s...\n", cfg.uri.c_str());
    esp_mqtt_client_start(client);
    started = true;
  }

  Event event;
  while (xQueueReceive(events, &event, 0) == pdTRUE) handleEvent(event);

  if (waitingReconnect && WiFi.status() == WL_CONNECTED && millis() >= nextAttemptAt) {
    waitingReconnect = false;
    Serial.printf("[mqtt] Conectando a %s...\n", cfg.uri.c_str());
    esp_mqtt_client_reconnect(client);
  }
}

bool connected() { return isConnected; }

void publishState() {
  JsonDocument doc;
  doc["led"] = led::isOn();
  doc["uptime"] = millis() / 1000;
  doc["rssi"] = WiFi.RSSI();
  doc["fw"] = FIRMWARE_VERSION;
  // El motivo del reinicio solo en el primer estado tras arrancar
  if (!firstStateSent) doc["reset"] = resetReason();

  char buffer[256];
  size_t n = serializeJson(doc, buffer);
  if (publish(topicState, buffer, n, 0)) firstStateSent = true;
}

}  // namespace mqtt_link
