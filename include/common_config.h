#pragma once

// Ajustes comunes al gateway y al nodo.
// Los del gateway están en gateway_config.h; los del enlace LoRa, en lora_config.h.

// Versión del firmware (gateway y nodo). El gateway la informa en devices/{id}/state como "fw"
#define FIRMWARE_VERSION "0.4.0"

// Watchdog: si el loop se congela este tiempo, el ESP32 se reinicia
#define WATCHDOG_TIMEOUT_S 30
