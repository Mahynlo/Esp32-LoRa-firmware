#pragma once

// Pines del ESP32 DevKit de 30 pines (iguales en gateway y nodo).
// Cableado y colores: docs/CABLEADO.md

// LoRa DX-LR30-900M22S (SX1262) en el adaptador DX-PJ27, por SPI (VSPI)
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_NSS 5     // pin de arranque: en reposo queda en alto, como al encender
#define LORA_RST 27
#define LORA_BUSY 26
#define LORA_DIO1 33
#define LORA_TXEN 25   // switch de RF (lo maneja RadioLib)
#define LORA_RXEN 32
// DIO2 no se conecta

// Relé del motor: In1 del HL-52S (solo en el nodo). Activo en bajo.
#define RELAY_PIN 16

// LED azul de la placa. Gateway: estado confirmado del motor y parpadeo en el portal.
// Nodo: motor encendido. Botón BOOT: solo lo usa el gateway (gateway_config.h)
#define STATUS_LED_PIN 2
#define BOOT_BUTTON_PIN 0
