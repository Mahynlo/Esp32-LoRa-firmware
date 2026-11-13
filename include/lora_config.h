#pragma once

// Enlace LoRa entre el gateway y el nodo del motor. Protocolo: docs/LORA.md
// Los parámetros de radio deben ser IGUALES en los dos equipos.

// ── Radio ────────────────────────────────────────────────────────────────
#define LORA_FREQUENCY_MHZ 915.0     // banda ISM 902–928 MHz (México)
#define LORA_BANDWIDTH_KHZ 125.0
#define LORA_SPREADING_FACTOR 9
#define LORA_CODING_RATE 7           // 4/7
#define LORA_SYNC_WORD 0x12          // privado (RADIOLIB_SX126X_SYNC_WORD_PRIVATE)
#define LORA_TX_POWER_DBM 10         // hasta 22; subir solo si el alcance no basta
#define LORA_PREAMBLE_LENGTH 8
#define LORA_TCXO_VOLTAGE 0.0        // el DX-LR30 usa cristal, no TCXO

// ── Protocolo ────────────────────────────────────────────────────────────
#define LORA_PROTOCOL_VERSION 1

// Gateway: espera de la respuesta del nodo e intentos por mensaje
#define LORA_REPLY_TIMEOUT_MS 1200
#define LORA_COMMAND_ATTEMPTS 3
#define LORA_POLL_ATTEMPTS 2
// Tiempo máximo para completar un comando. broker_api espera 10 s el ack.
#define LORA_COMMAND_DEADLINE_MS 8000

// Gateway: consulta el estado del nodo cada cierto tiempo (también lo mantiene "vivo")
#define LORA_POLL_INTERVAL_MS 20000
// Consultas seguidas sin respuesta para dar el nodo por desconectado
#define LORA_OFFLINE_AFTER_MISSES 2

// Nodo: un comando solo vale con el reto (número al azar) de su última respuesta, y
// el reto vence a este tiempo. Así un comando grabado y reenviado después no sirve.
#define LORA_CHALLENGE_TTL_MS 15000

// Nodo: sin noticias del gateway este tiempo, apaga el motor (estado seguro).
// Con la consulta cada 20 s equivale a ~9 consultas perdidas. 0 = desactivado.
#define NODE_FAILSAFE_TIMEOUT_MS 180000

// Nodo: pausa antes de responder, para que el gateway ya esté escuchando
#define NODE_REPLY_DELAY_MS 30
