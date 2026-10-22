#pragma once

// Versión del firmware (se informa en devices/{id}/state como "fw")
#define FIRMWARE_VERSION "0.1.0"

// Pin del LED controlado desde la app (después: salida del motor)
#define LED_PIN 23

// MQTT
#define MQTT_KEEPALIVE_S 15          // el broker detecta un corte en ~1,5 × keepalive (~22 s)
#define MQTT_SOCKET_TIMEOUT_S 5      // espera máxima de la conexión TCP
#define MQTT_BUFFER_SIZE 512         // tamaño máximo de un mensaje MQTT

// Latido: estado publicado cada cierto tiempo aunque no cambie
#define HEARTBEAT_INTERVAL_MS 30000

// Reconexión con espera creciente: 1 s, 2 s, 4 s... hasta este máximo
#define RECONNECT_MAX_DELAY_MS 30000

// Comandos más viejos que esto se rechazan (solo si la hora está sincronizada por NTP)
#define MAX_COMMAND_AGE_MS 30000

// Watchdog: si el loop se congela este tiempo, el ESP32 se reinicia
#define WATCHDOG_TIMEOUT_S 30
