#pragma once

// Ajustes del GATEWAY: conexión con broker_api (MQTT), portal de configuración y botón BOOT.
// Comunes con el nodo: common_config.h. Enlace LoRa: lora_config.h. Pines: pins.h.
#include "common_config.h"
#include "pins.h"

// ── MQTT ─────────────────────────────────────────────────────────────────
#define MQTT_KEEPALIVE_S 15          // el broker detecta un corte en ~1,5 × keepalive (~22 s)
#define MQTT_BUFFER_SIZE 512         // tamaño máximo de un mensaje MQTT
#define TLS_HANDSHAKE_TIMEOUT_S 10   // espera máxima del saludo TLS

// Latido: estado publicado cada cierto tiempo aunque no cambie
#define HEARTBEAT_INTERVAL_MS 30000

// Reconexión con espera creciente: 1 s, 2 s, 4 s... hasta este máximo
#define RECONNECT_MAX_DELAY_MS 30000

// Comandos más viejos que esto se rechazan (solo si la hora está sincronizada por NTP)
#define MAX_COMMAND_AGE_MS 30000

// Rechazos seguidos del broker (dispositivo borrado o token rotado) antes de volver al portal
#define MAX_AUTH_FAILURES 3

// ── Servidor (valores sugeridos en el portal; se pueden cambiar ahí) ─────
// Todo va cifrado (TLS): HTTPS para canjear el código y wss:// o mqtts:// para el broker.
// El ESP32 valida el certificado del servidor con las CAs de include/ca_cert.h
// (lo genera broker_api con: pnpm certs:dev --firmware <esta carpeta>).
#define DEFAULT_SERVER_HOST "192.168.1.10"
#define DEFAULT_API_PORT 3443
#define DEFAULT_MQTT_PORT 8883

// Transporte MQTT:
//   "wss"   MQTT sobre WebSocket por el puerto de la API (wss://servidor:3443/mqtt; 443 en
//           producción detrás de un proxy). Pasa por cualquier hosting que acepte HTTPS.
//   "mqtts" MQTT por TCP con TLS (mqtts://servidor:8883). Necesita ese puerto abierto.
#define TRANSPORT_WSS "wss"
#define TRANSPORT_MQTTS "mqtts"
#define DEFAULT_TRANSPORT TRANSPORT_WSS
#define MQTT_WS_PATH "/mqtt"

// Versión del formato guardado en NVS. 2 = puertos con TLS: una configuración
// guardada con los puertos sin cifrar (3000/1883) se pasa sola a 3443/8883.
#define CONFIG_VERSION 2

// ── Portal de configuración ──────────────────────────────────────────────
// Clave de la red "ESP32-LORA-XXXX" del portal: cada equipo genera la suya al azar la
// primera vez (p. ej. "K7QMX-2PARN") y la conserva siempre, incluso al borrar todo con
// BOOT, para poder anotarla en una etiqueta. Se muestra en el monitor serie al arrancar.
// Solo para pruebas: definir CONFIG_AP_PASSWORD (mínimo 8 caracteres) fija una clave
// igual para todos los equipos, lo que permite configurar cualquiera conociéndola.
// #define CONFIG_AP_PASSWORD "clave-de-pruebas"
#define PORTAL_PASSWORD_LENGTH 10

// Portal abierto en un equipo ya vinculado: sin uso este tiempo, se cierra y el equipo
// vuelve a funcionar con su configuración anterior (no queda fuera de servicio por error)
#define CONFIG_PORTAL_TIMEOUT_S 300

// ── Botón BOOT (BOOT_BUTTON_PIN en pins.h) ───────────────────────────────
//   - con el equipo funcionando, mantenerlo CONFIG_PORTAL_HOLD_MS abre el portal para
//     cambiar Wi-Fi o servidor (conserva la vinculación);
//   - presionarlo justo al encender y mantenerlo CONFIG_BUTTON_HOLD_MS borra todo.
#define CONFIG_BUTTON_HOLD_MS 3000
#define CONFIG_PORTAL_HOLD_MS 5000
