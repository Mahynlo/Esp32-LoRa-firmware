# Enlace LoRa gateway ↔ nodo

```
App ──HTTPS──▶ broker_api ──MQTT (wss)──▶ GATEWAY ──LoRa 915 MHz──▶ NODO ──▶ relé ──▶ motor
                    ◀──────── ack ────────────┘ ◀──── confirmación ────┘
```

- **Gateway** (`pio run -e gateway`): el ESP32 con Wi-Fi. Es el "dispositivo" que se vincula en la app. Recibe el comando `motor` por MQTT, lo envía al nodo y responde el `ack` al servidor cuando el nodo lo confirma.
- **Nodo** (`pio run -e nodo`): el ESP32 junto al motor, con el relé. Sin Wi-Fi: solo escucha al gateway por LoRa.

Parámetros de radio: `include/lora_config.h`. Pines y cableado: [CABLEADO.md](CABLEADO.md).

## Qué ve la app

El gateway publica en su estado:

| Campo | Significado |
|---|---|
| `motor` | Último estado **confirmado por el nodo**. No aparece hasta que el nodo responde la primera vez |
| `nodeOnline` | `false` si el nodo dejó de responder (2 consultas seguidas, ~40 s) |
| `loraRssi` / `loraSnr` | Con qué intensidad oye el gateway al nodo |
| `rssi` | Señal Wi-Fi del gateway |

El gateway consulta al nodo cada 20 s y avisa al servidor cuando algo cambia. Si el nodo no responde a un comando, el `ack` sale con error ("El nodo del motor no responde por LoRa") y la app lo muestra.

## Seguridad

Cualquiera con un módulo LoRa de 915 MHz puede oír y transmitir en la misma frecuencia. Por eso:

1. **Mensajes cifrados y autenticados (AES-128-GCM)** con una clave compartida (`include/lora_key.h`). Un mensaje con otra clave o alterado se descarta. Cada trama lleva un nonce al azar de 96 bits: no depende de contadores guardados, así que borrar la flash o reiniciar no repite nonces.
2. **Reto del nodo contra la repetición**: cada respuesta del nodo trae un número al azar (reto). Un comando `SET_MOTOR` solo se acepta con el **último** reto, que **vence a los 15 s** y **se usa una vez**. Quien grabe un comando por radio y lo reenvíe después no logra nada. Si el reto que tenía el gateway ya venció, el nodo responde con uno nuevo y el gateway repite el comando solo (dos idas y vueltas, ~1,5 s).
3. **Ids de petición**: el gateway solo acepta la respuesta con el id de su petición (no se le puede colar una respuesta vieja), y el nodo reconoce un reintento por su id y no ejecuta dos veces.
4. **Estado seguro**: el relé arranca apagado. Si el nodo pasa **3 minutos** sin noticias del gateway (`NODE_FAILSAFE_TIMEOUT_MS`), apaga el motor.
5. El servidor sigue validando todo antes (lista blanca, sesión, dueño del dispositivo) y el gateway descarta comandos repetidos o vencidos.

### La clave

- La genera `scripts/lora_key.py` en la primera compilación si no existe, y la usan los dos firmwares. **Sube el gateway y el nodo desde esta misma carpeta.**
- **Es secreta**: quien la tenga puede controlar el motor. Está en `.gitignore`; no la compartas ni la subas al repositorio.
- Para cambiarla (p. ej. si se filtró): borra `include/lora_key.h`, compila y **sube los dos equipos**. Hasta subir los dos, no se entienden.
- Queda en la flash sin cifrar: con acceso físico al ESP32 se puede leer. Para evitarlo hace falta *flash encryption* (pendiente).

## Formato

Trama en el aire (máx. 54 bytes):

```
[versión 1][emisor 1][nonce 12][datos cifrados ≤24][etiqueta 16]
```

`emisor`: 0 = gateway, 1 = nodo. La versión y el emisor van autenticados (no se pueden cambiar sin invalidar la etiqueta).

Mensajes (dentro de los datos cifrados, enteros en little-endian):

| Mensaje | Dirección | Contenido |
|---|---|---|
| `PING` (0x01) | gateway → nodo | `[tipo][id u32]` |
| `SET_MOTOR` (0x02) | gateway → nodo | `[tipo][id u32][reto u32][encender u8]` |
| `REPLY` (0x81) | nodo → gateway | `[tipo][id u32][resultado u8][motor u8][reto nuevo u32][rssi i8][snr i8]` |

`resultado`: 0 = OK, 1 = reto vencido (repetir con el reto nuevo), 2 = no soportado.

### Tiempos

| | Valor |
|---|---|
| Tiempo en el aire de una trama (SF9, BW 125) | ~0,4 s |
| Espera de respuesta del gateway | 1,2 s, hasta 3 intentos |
| Tiempo máximo de un comando | 8 s (broker_api espera 10 s el ack) |
| Consulta del estado | cada 20 s |

## Monitor serie

Gateway:

```
[lora] Radio lista: 915.0 MHz, SF9, 10 dBm
[lora] Nodo del motor conectado
[cmd] Motor: encender (enviando al nodo por LoRa)
[cmd] Motor ENCENDIDO (confirmado por el nodo)
```

Nodo:

```
[boot] Nodo del motor, firmware 0.4.0
[lora] Radio lista: 915.0 MHz, SF9, 10 dBm
[boot] Esperando al gateway...
[motor] ENCENDIDO | RSSI -48 dBm | SNR 9.5 dB
```

| Síntoma | Causa | Solución |
|---|---|---|
| `[lora] El módulo no responde (-2)` | Cableado SPI o alimentación del LR30 | Revisar con [CABLEADO.md](CABLEADO.md#7-verificación) |
| Gateway: `El nodo del motor no responde` y el nodo no muestra nada | Nodo apagado, sin antena, otra clave o otros parámetros de radio | Subir los dos desde esta carpeta; revisar antenas |
| El nodo muestra `reto vencido` en cada comando | Normal si pasó más de 15 s desde la última consulta: el gateway repite solo | — |
| Alcance muy corto | TXEN/RXEN mal conectados o sin antena | Revisar D25/D32 y la antena; subir `LORA_TX_POWER_DBM` |

## Pendiente

- Botón local en el nodo para apagar el motor sin la app.
- Que el gateway apague el motor si pierde el servidor mucho tiempo (hoy solo el nodo se protege si pierde al gateway).
- Revisar la normativa de 915 MHz (tiempo en el aire y potencia) antes de una instalación definitiva.
- Varios nodos por gateway (hoy uno).
