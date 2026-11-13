#pragma once

#include <Arduino.h>

// Mensajes entre gateway y nodo (van cifrados por secure_link). Ver docs/LORA.md
//
//   Gateway → nodo
//     PING       [tipo][id u32]                          estado del nodo (y un reto nuevo)
//     SET_MOTOR  [tipo][id u32][reto u32][encender u8]   encender/apagar el motor
//   Nodo → gateway
//     REPLY      [tipo][id u32][resultado u8][motor u8][reto u32][rssi i8][snr i8]
//
// - id: número al azar de cada petición. El gateway solo acepta la respuesta con su id;
//   el nodo reconoce un reintento (mismo id) y repite la respuesta sin ejecutar otra vez.
// - reto: número al azar que el nodo entrega en cada respuesta. SET_MOTOR debe traer el
//   último, que vence a los LORA_CHALLENGE_TTL_MS y se usa una sola vez. Un comando
//   grabado y reenviado más tarde llega con un reto vencido y se rechaza.
// - rssi/snr: con qué intensidad oyó el nodo al gateway.
namespace protocol {

enum Type : uint8_t {
  PING = 0x01,
  SET_MOTOR = 0x02,
  REPLY = 0x81,
};

enum Result : uint8_t {
  OK = 0,
  STALE_CHALLENGE = 1,  // reto vencido o equivocado: reintentar con el de esta respuesta
  UNSUPPORTED = 2,
};

constexpr size_t PING_LENGTH = 5;
constexpr size_t SET_MOTOR_LENGTH = 10;
constexpr size_t REPLY_LENGTH = 13;

inline void putU32(uint8_t* p, uint32_t v) {
  p[0] = v;
  p[1] = v >> 8;
  p[2] = v >> 16;
  p[3] = v >> 24;
}

inline uint32_t getU32(const uint8_t* p) {
  return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24);
}

inline int8_t clampI8(float v) {
  return static_cast<int8_t>(constrain(lroundf(v), -128L, 127L));
}

}  // namespace protocol
