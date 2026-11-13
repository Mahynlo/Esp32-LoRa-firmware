#pragma once

#include <Arduino.h>

// Radio LoRa con mensajes cifrados y autenticados (AES-128-GCM, clave de include/lora_key.h).
// Trama: [versión][emisor][nonce 12][datos cifrados][etiqueta 16]. Ver docs/LORA.md
//
// Un mensaje modificado, de otra red o firmado con otra clave se descarta sin avisar.
// Esto NO evita que alguien grabe un mensaje válido y lo reenvíe: de eso se encargan
// los ids de petición y el reto del nodo (protocol.h).
namespace lora {

enum class Role : uint8_t { Gateway = 0, Node = 1 };

constexpr size_t MAX_PAYLOAD = 24;

struct Packet {
  uint8_t data[MAX_PAYLOAD];
  size_t length;
  float rssi;  // dBm
  float snr;   // dB
};

// Inicia la radio y queda escuchando. false si el módulo no responde (cableado, alimentación)
bool begin(Role role);

// Cifra y envía. Vuelve a escuchar al terminar.
bool send(const uint8_t* data, size_t length);

// Reenvía la última trama tal cual (reintento: el receptor la reconoce por su id)
bool resend();

// Sin bloquear: true si llegó un mensaje válido del otro equipo
bool receive(Packet& packet);

// Número al azar de 32 bits (hardware)
uint32_t random32();

}  // namespace lora
