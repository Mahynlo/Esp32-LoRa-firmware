#pragma once

// Versión del firmware (se informa en devices/{id}/state como "fw")
#define FIRMWARE_VERSION "0.1.0"

// Pin del LED controlado desde la app (después: salida del motor).
// 2 = LED integrado (azul) de la ESP32 DevKit, para pruebas sin cablear nada.
#define LED_PIN 2

// MQTT
#define MQTT_KEEPALIVE_S 15          // el broker detecta un corte en ~1,5 × keepalive (~22 s)
#define MQTT_SOCKET_TIMEOUT_S 5      // espera máxima de la conexión TCP
#define MQTT_BUFFER_SIZE 512         // tamaño máximo de un mensaje MQTT
#define TLS_HANDSHAKE_TIMEOUT_S 10   // espera máxima del saludo TLS

// Latido: estado publicado cada cierto tiempo aunque no cambie
#define HEARTBEAT_INTERVAL_MS 30000

// Reconexión con espera creciente: 1 s, 2 s, 4 s... hasta este máximo
#define RECONNECT_MAX_DELAY_MS 30000

// Comandos más viejos que esto se rechazan (solo si la hora está sincronizada por NTP)
#define MAX_COMMAND_AGE_MS 30000

// Watchdog: si el loop se congela este tiempo, el ESP32 se reinicia
#define WATCHDOG_TIMEOUT_S 30

// ── Configuración (portal) ───────────────────────────────────────────────
// Valores sugeridos en el portal; se pueden cambiar ahí.
// Todo va cifrado (TLS): HTTPS para canjear el código y MQTTS para el broker.
// El ESP32 valida el certificado del servidor con las CAs de include/ca_cert.h
// (lo genera broker_api con: pnpm certs:dev --firmware <esta carpeta>).
#define DEFAULT_SERVER_HOST "192.168.1.10"
#define DEFAULT_API_PORT 3443
#define DEFAULT_MQTT_PORT 8883

// Versión del formato guardado en NVS. 2 = puertos con TLS: una configuración
// guardada con los puertos sin cifrar (3000/1883) se pasa sola a 3443/8883.
#define CONFIG_VERSION 2

// Clave de la red "ESP32-LORA-XXXX" del portal: cada equipo genera la suya al azar la
// primera vez (p. ej. "K7QMX-2PARN") y la conserva siempre, incluso al borrar todo con
// BOOT, para poder anotarla en una etiqueta. Se muestra en el monitor serie al arrancar.
// Solo para pruebas: definir CONFIG_AP_PASSWORD (mínimo 8 caracteres) fija una clave
// igual para todos los equipos, lo que permite configurar cualquiera conociéndola.
// #define CONFIG_AP_PASSWORD "clave-de-pruebas"
#define PORTAL_PASSWORD_LENGTH 10

// Botón BOOT:
//   - con el equipo funcionando, mantenerlo CONFIG_PORTAL_HOLD_MS abre el portal para
//     cambiar Wi-Fi o servidor (conserva la vinculación);
//   - presionarlo justo al encender y mantenerlo CONFIG_BUTTON_HOLD_MS borra todo.
#define CONFIG_BUTTON_PIN 0
#define CONFIG_BUTTON_HOLD_MS 3000
#define CONFIG_PORTAL_HOLD_MS 5000

// Portal abierto en un equipo ya vinculado: sin uso este tiempo, se cierra y el equipo
// vuelve a funcionar con su configuración anterior (no queda fuera de servicio por error)
#define CONFIG_PORTAL_TIMEOUT_S 300

// Rechazos seguidos del broker (dispositivo borrado o token rotado) antes de volver al portal
#define MAX_AUTH_FAILURES 3
