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

// Latido: estado publicado cada cierto tiempo aunque no cambie
#define HEARTBEAT_INTERVAL_MS 30000

// Reconexión con espera creciente: 1 s, 2 s, 4 s... hasta este máximo
#define RECONNECT_MAX_DELAY_MS 30000

// Comandos más viejos que esto se rechazan (solo si la hora está sincronizada por NTP)
#define MAX_COMMAND_AGE_MS 30000

// Watchdog: si el loop se congela este tiempo, el ESP32 se reinicia
#define WATCHDOG_TIMEOUT_S 30

// ── Configuración (portal) ───────────────────────────────────────────────
// Valores sugeridos en el portal; se pueden cambiar ahí
#define DEFAULT_SERVER_HOST "192.168.1.10"
#define DEFAULT_API_PORT 3000
#define DEFAULT_MQTT_PORT 1883

// Clave de la red "ESP32-LORA-XXXX" del portal (mínimo 8 caracteres).
// Cámbiala: quien la conozca y esté cerca podría reconfigurar el equipo mientras el portal esté abierto.
#define CONFIG_AP_PASSWORD "lora1234"

// Mantener presionado BOOT al encender este tiempo borra la configuración y abre el portal
#define CONFIG_BUTTON_PIN 0
#define CONFIG_BUTTON_HOLD_MS 3000

// Rechazos seguidos del broker (dispositivo borrado o token rotado) antes de volver al portal
#define MAX_AUTH_FAILURES 3
