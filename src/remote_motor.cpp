#include "remote_motor.h"

#include <esp_task_wdt.h>

#include "lora/protocol.h"
#include "lora/secure_link.h"
#include "lora_config.h"

namespace {

bool radioReady = false;
bool stateKnown = false;
bool motorOn = false;
bool online = false;
bool changed = false;
int misses = 0;
int rssi = 0;
float snr = 0;

uint32_t challenge = 0;  // el último que entregó el nodo
unsigned long lastPollAt = 0;

struct Reply {
  protocol::Result result;
  bool motor;
};

void setOnline(bool value) {
  if (online == value) return;
  online = value;
  changed = true;
  Serial.println(value ? "[lora] Nodo del motor conectado" : "[lora] El nodo del motor no responde");
}

// Envía la petición y espera la respuesta con su id. Reintenta reenviando la misma trama
// (el nodo reconoce el id y no ejecuta dos veces).
bool exchange(const uint8_t* request, size_t length, int attempts, unsigned long deadline, Reply& out) {
  uint32_t requestId = protocol::getU32(request + 1);
  for (int attempt = 0; attempt < attempts && millis() < deadline; attempt++) {
    if (!(attempt == 0 ? lora::send(request, length) : lora::resend())) continue;

    unsigned long waitUntil = min(millis() + LORA_REPLY_TIMEOUT_MS, deadline);
    while (millis() < waitUntil) {
      lora::Packet packet;
      if (lora::receive(packet) && packet.length == protocol::REPLY_LENGTH && packet.data[0] == protocol::REPLY &&
          protocol::getU32(packet.data + 1) == requestId) {
        out.result = static_cast<protocol::Result>(packet.data[5]);
        out.motor = packet.data[6] == 1;
        challenge = protocol::getU32(packet.data + 7);
        rssi = lroundf(packet.rssi);
        snr = packet.snr;
        misses = 0;
        setOnline(true);
        if (!stateKnown || motorOn != out.motor) changed = true;
        stateKnown = true;
        motorOn = out.motor;
        return true;
      }
      delay(2);
    }
  }
  return false;
}

bool poll(unsigned long deadline) {
  uint8_t msg[protocol::PING_LENGTH];
  msg[0] = protocol::PING;
  protocol::putU32(msg + 1, lora::random32());
  Reply reply;
  return exchange(msg, sizeof(msg), LORA_POLL_ATTEMPTS, deadline, reply);
}

}  // namespace

namespace remote_motor {

bool begin() {
  radioReady = lora::begin(lora::Role::Gateway);
  if (radioReady) lastPollAt = millis() - LORA_POLL_INTERVAL_MS;  // consultar ya
  return radioReady;
}

void loop() {
  if (!radioReady || millis() - lastPollAt < LORA_POLL_INTERVAL_MS) return;
  lastPollAt = millis();
  if (poll(millis() + LORA_COMMAND_DEADLINE_MS)) return;
  if (++misses >= LORA_OFFLINE_AFTER_MISSES) setOnline(false);
}

Result set(bool on) {
  if (!radioReady) return {false, "Radio LoRa del gateway sin iniciar"};
  unsigned long deadline = millis() + LORA_COMMAND_DEADLINE_MS;

  // Dos rondas: si el reto que tenía ya venció, el nodo responde con uno nuevo y se repite
  for (int round = 0; round < 2; round++) {
    uint8_t msg[protocol::SET_MOTOR_LENGTH];
    msg[0] = protocol::SET_MOTOR;
    protocol::putU32(msg + 1, lora::random32());
    protocol::putU32(msg + 5, challenge);
    msg[9] = on ? 1 : 0;

    Reply reply;
    if (!exchange(msg, sizeof(msg), LORA_COMMAND_ATTEMPTS, deadline, reply)) {
      if (++misses >= LORA_OFFLINE_AFTER_MISSES) setOnline(false);
      return {false, "El nodo del motor no responde por LoRa"};
    }
    lastPollAt = millis();  // acaba de responder: no hace falta consultarlo
    esp_task_wdt_reset();
    if (reply.result == protocol::STALE_CHALLENGE) continue;
    if (reply.result != protocol::OK) return {false, "El nodo rechazó el comando"};
    if (reply.motor != on) return {false, "El motor no cambió de estado"};
    return {true, nullptr};
  }
  return {false, "El nodo no aceptó el comando (reto vencido)"};
}

bool known() { return stateKnown; }
bool isOn() { return motorOn; }
bool nodeOnline() { return online; }
int linkRssi() { return rssi; }
float linkSnr() { return snr; }

bool takeChanged() {
  bool value = changed;
  changed = false;
  return value;
}

}  // namespace remote_motor
