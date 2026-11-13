#include "secure_link.h"

#include <RadioLib.h>
#include <SPI.h>
#include <esp_random.h>
#include <mbedtls/gcm.h>

#include "lora_config.h"
#include "lora_key.h"
#include "pins.h"

namespace {

constexpr size_t HEADER_LENGTH = 2;  // versión + emisor (van autenticados, no cifrados)
constexpr size_t NONCE_LENGTH = 12;
constexpr size_t TAG_LENGTH = 16;
constexpr size_t OVERHEAD = HEADER_LENGTH + NONCE_LENGTH + TAG_LENGTH;
constexpr size_t MAX_FRAME = OVERHEAD + lora::MAX_PAYLOAD;

SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_RST, LORA_BUSY);
mbedtls_gcm_context gcm;

uint8_t ownId = 0;
uint8_t peerId = 0;

uint8_t lastFrame[MAX_FRAME];
size_t lastFrameLength = 0;

volatile bool packetReady = false;
void IRAM_ATTR onPacket() { packetReady = true; }

void listen() {
  // La propia transmisión también marca DIO1: se limpia antes de escuchar
  packetReady = false;
  radio.startReceive();
}

bool transmit(const uint8_t* frame, size_t length) {
  int state = radio.transmit(frame, length);
  listen();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[lora] Error al transmitir (%d)\n", state);
    return false;
  }
  return true;
}

}  // namespace

namespace lora {

uint32_t random32() { return esp_random(); }

bool begin(Role role) {
  ownId = static_cast<uint8_t>(role);
  peerId = role == Role::Gateway ? static_cast<uint8_t>(Role::Node) : static_cast<uint8_t>(Role::Gateway);

  mbedtls_gcm_init(&gcm);
  if (mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, LORA_KEY, 128) != 0) {
    Serial.println("[lora] No se pudo cargar la clave");
    return false;
  }

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  int state = radio.begin(LORA_FREQUENCY_MHZ, LORA_BANDWIDTH_KHZ, LORA_SPREADING_FACTOR, LORA_CODING_RATE,
                          LORA_SYNC_WORD, LORA_TX_POWER_DBM, LORA_PREAMBLE_LENGTH, LORA_TCXO_VOLTAGE);
  if (state != RADIOLIB_ERR_NONE) {
    Serial.printf("[lora] El módulo no responde (%d): revisa cableado y alimentación\n", state);
    return false;
  }
  radio.setRfSwitchPins(LORA_RXEN, LORA_TXEN);
  radio.setPacketReceivedAction(onPacket);
  listen();
  Serial.printf("[lora] Radio lista: %.1f MHz, SF%d, %d dBm\n", LORA_FREQUENCY_MHZ, LORA_SPREADING_FACTOR,
                LORA_TX_POWER_DBM);
  return true;
}

bool send(const uint8_t* data, size_t length) {
  if (length == 0 || length > MAX_PAYLOAD) return false;

  uint8_t* frame = lastFrame;
  frame[0] = LORA_PROTOCOL_VERSION;
  frame[1] = ownId;
  // Nonce al azar en cada trama: nunca se repite con la misma clave, aunque se
  // reinicie o se borre la flash (no depende de contadores guardados)
  uint8_t* nonce = frame + HEADER_LENGTH;
  esp_fill_random(nonce, NONCE_LENGTH);
  uint8_t* cipher = nonce + NONCE_LENGTH;
  uint8_t* tag = cipher + length;

  if (mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, length, nonce, NONCE_LENGTH, frame, HEADER_LENGTH, data,
                                cipher, TAG_LENGTH, tag) != 0) {
    return false;
  }
  lastFrameLength = OVERHEAD + length;
  return transmit(lastFrame, lastFrameLength);
}

bool resend() { return lastFrameLength > 0 && transmit(lastFrame, lastFrameLength); }

bool receive(Packet& packet) {
  if (!packetReady) return false;
  packetReady = false;

  uint8_t frame[MAX_FRAME];
  size_t length = radio.getPacketLength();
  bool fits = length > OVERHEAD && length <= MAX_FRAME;
  int state = fits ? radio.readData(frame, length) : RADIOLIB_ERR_PACKET_TOO_LONG;
  packet.rssi = radio.getRSSI();
  packet.snr = radio.getSNR();
  listen();

  // Otra red, otro protocolo, ruido o una trama nuestra: se ignora
  if (state != RADIOLIB_ERR_NONE || frame[0] != LORA_PROTOCOL_VERSION || frame[1] != peerId) return false;

  const uint8_t* nonce = frame + HEADER_LENGTH;
  const uint8_t* cipher = nonce + NONCE_LENGTH;
  packet.length = length - OVERHEAD;
  const uint8_t* tag = cipher + packet.length;

  // Clave distinta o trama alterada: la etiqueta no coincide
  return mbedtls_gcm_auth_decrypt(&gcm, packet.length, nonce, NONCE_LENGTH, frame, HEADER_LENGTH, tag, TAG_LENGTH,
                                  cipher, packet.data) == 0;
}

}  // namespace lora
