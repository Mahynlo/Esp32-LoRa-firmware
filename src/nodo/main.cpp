// Firmware del NODO: ESP32 junto al motor, con el módulo LoRa y el relé (HL-52S).
// No usa Wi-Fi: recibe las órdenes del gateway por LoRa y le confirma cada una.
// Compilar y subir: pio run -e nodo -t upload. Protocolo: docs/LORA.md
//
// Seguridad:
//   - Solo acepta mensajes cifrados con la clave compartida (include/lora_key.h).
//   - Un comando debe traer el reto de la última respuesta (vence en 15 s, un solo uso):
//     un comando grabado por radio y reenviado después se rechaza.
//   - Estado seguro: el relé arranca apagado y se apaga solo si el gateway deja de
//     comunicarse NODE_FAILSAFE_TIMEOUT_MS.
#include <Arduino.h>
#include <bootloader_random.h>
#include <esp_task_wdt.h>

#include "common_config.h"
#include "lora/protocol.h"
#include "lora/secure_link.h"
#include "lora_config.h"
#include "pins.h"

namespace {

bool motorOn = false;

uint32_t challenge = 0;
unsigned long challengeIssuedAt = 0;

uint32_t lastRequestId = 0;
bool hasLastRequest = false;
unsigned long lastHeardAt = 0;

// ── Relé ────────────────────────────────────────────────────────────────
// Encender: LOW. Apagar: alta impedancia (INPUT). Así el relé se suelta por completo
// tanto con su Vcc a 3,3 V como a 5 V. Al arrancar el pin ya es entrada: sin "clic".
void setMotor(bool on) {
  if (on) {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
  } else {
    pinMode(RELAY_PIN, INPUT);
  }
  motorOn = on;
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

void newChallenge() {
  challenge = lora::random32();
  challengeIssuedAt = millis();
}

bool challengeValid(uint32_t received) {
  return challenge != 0 && received == challenge && millis() - challengeIssuedAt < LORA_CHALLENGE_TTL_MS;
}

void reply(uint32_t requestId, protocol::Result result, const lora::Packet& heard) {
  newChallenge();  // cada respuesta entrega un reto nuevo; el anterior ya no sirve
  uint8_t msg[protocol::REPLY_LENGTH];
  msg[0] = protocol::REPLY;
  protocol::putU32(msg + 1, requestId);
  msg[5] = result;
  msg[6] = motorOn ? 1 : 0;
  protocol::putU32(msg + 7, challenge);
  msg[11] = static_cast<uint8_t>(protocol::clampI8(heard.rssi));
  msg[12] = static_cast<uint8_t>(protocol::clampI8(heard.snr));
  delay(NODE_REPLY_DELAY_MS);
  lora::send(msg, sizeof(msg));
}

void handle(const lora::Packet& packet) {
  if (packet.length < protocol::PING_LENGTH) return;
  uint8_t type = packet.data[0];
  uint32_t requestId = protocol::getU32(packet.data + 1);
  lastHeardAt = millis();

  // Reintento del gateway (no le llegó la respuesta): repetirla sin ejecutar otra vez
  if (hasLastRequest && requestId == lastRequestId) {
    lora::resend();
    return;
  }
  lastRequestId = requestId;
  hasLastRequest = true;

  if (type == protocol::PING) {
    reply(requestId, protocol::OK, packet);
    return;
  }

  if (type == protocol::SET_MOTOR && packet.length == protocol::SET_MOTOR_LENGTH) {
    if (!challengeValid(protocol::getU32(packet.data + 5))) {
      Serial.println("[motor] Comando con reto vencido: se pide repetirlo");
      reply(requestId, protocol::STALE_CHALLENGE, packet);
      return;
    }
    bool on = packet.data[9] == 1;
    setMotor(on);
    Serial.printf("[motor] %s | RSSI %.0f dBm | SNR %.1f dB\n", on ? "ENCENDIDO" : "apagado", packet.rssi, packet.snr);
    reply(requestId, protocol::OK, packet);
    return;
  }

  reply(requestId, protocol::UNSUPPORTED, packet);
}

void checkFailsafe() {
#if NODE_FAILSAFE_TIMEOUT_MS > 0
  if (motorOn && millis() - lastHeardAt >= NODE_FAILSAFE_TIMEOUT_MS) {
    setMotor(false);
    Serial.printf("[motor] Sin noticias del gateway en %d s: motor APAGADO por seguridad\n",
                  NODE_FAILSAFE_TIMEOUT_MS / 1000);
  }
#endif
}

}  // namespace

void setup() {
  pinMode(RELAY_PIN, INPUT);  // relé apagado desde el primer instante
  pinMode(STATUS_LED_PIN, OUTPUT);
  setMotor(false);

  Serial.begin(115200);
  Serial.printf("\n[boot] Nodo del motor, firmware %s\n", FIRMWARE_VERSION);

  esp_task_wdt_init(WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(nullptr);

  // Sin Wi-Fi, el generador de azar necesita esta fuente de ruido para ser realmente al azar
  bootloader_random_enable();

  if (!lora::begin(lora::Role::Node)) {
    // Sin radio el nodo no sirve: parpadeo rápido 10 s y reinicio para reintentar
    for (int i = 0; i < 100; i++) {
      digitalWrite(STATUS_LED_PIN, !digitalRead(STATUS_LED_PIN));
      delay(100);
    }
    ESP.restart();
  }
  newChallenge();
  lastHeardAt = millis();
  Serial.println("[boot] Esperando al gateway...");
}

void loop() {
  esp_task_wdt_reset();
  lora::Packet packet;
  if (lora::receive(packet)) handle(packet);
  checkFailsafe();
  delay(2);
}
