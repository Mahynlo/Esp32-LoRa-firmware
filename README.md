# esp32_lora — gateway y nodo del motor

Un proyecto, **dos firmwares** para dos ESP32 con módulo LoRa (SX1262):

| Firmware | Equipo | Qué hace |
|---|---|---|
| **gateway** (`pio run -e gateway`) | ESP32 + LoRa, donde hay Wi-Fi | Se conecta a **broker_api** por MQTT cifrado y reenvía el comando `motor` al nodo por LoRa |
| **nodo** (`pio run -e nodo`) | ESP32 + LoRa + relé, junto al motor | Sin Wi-Fi: enciende/apaga el relé con las órdenes del gateway y las confirma |

```
App ──▶ broker_api ──MQTT (wss)──▶ GATEWAY ──LoRa cifrado──▶ NODO ──▶ relé ──▶ motor
```

Versión actual: **0.4.0** (`FIRMWARE_VERSION`; el gateway la informa a la app como `fw`). Cableado y pines: [docs/CABLEADO.md](docs/CABLEADO.md). Enlace LoRa (protocolo y seguridad): [docs/LORA.md](docs/LORA.md).

El resto de este documento trata del **gateway** (la parte que habla con el servidor), salvo donde dice nodo. El gateway se conecta a broker_api **sobre WebSocket** (`wss://`, por defecto) o **por TCP** (`mqtts://`):

- Portal de configuración propio: Wi-Fi, servidor y **código de vinculación** (sin escribir id ni token a mano). Se vuelve a abrir con **BOOT 5 s** para cambiar la red o la IP sin perder la vinculación.
- **Todo cifrado**: canjea el código por HTTPS y se conecta por `wss://` o `mqtts://`, validando el certificado del servidor.
- **Transporte a elegir en el portal**: WebSocket (`wss`, por el mismo puerto que la API: pasa por cualquier hosting con HTTPS) o TCP (`mqtts`, puerto 8883).
- Enciende/apaga el motor del nodo por LoRa con los comandos de la app y confirma cada uno (`ack`) cuando el nodo lo confirma. Su LED azul muestra el estado del motor.
- Enlace LoRa **cifrado y autenticado** (AES-128-GCM) y con reto contra la repetición: ver [docs/LORA.md](docs/LORA.md).
- Informa su estado al conectar, al cambiar y cada 30 s (latido).
- **Última voluntad** MQTT: si se corta la corriente, el broker avisa a la app en ~22 s.
- Informa el **motivo del último reinicio** (corte de energía, caída de voltaje, watchdog…).
- **Estado seguro**: el relé del nodo arranca apagado y se apaga solo si el nodo pasa 3 min sin noticias del gateway.
- Reconexión automática del Wi-Fi, reconexión MQTT con espera creciente (1 s → 30 s) y **watchdog** de 30 s.
- Rechaza comandos repetidos, fuera de la lista blanca o de más de 30 s (esto último solo si tiene la hora por NTP, es decir, con internet).

Protocolo (tópicos y mensajes): `broker_api/docs/MQTT.md`.

## Compilar y subir

Con la extensión de PlatformIO en VS Code (botones de la barra inferior) o en terminal:

```bash
pio run                                   # compilar los dos
pio run -e gateway -t upload              # subir el gateway (USB); cierra antes el monitor serie
pio run -e nodo -t upload                 # subir el nodo
pio run -e nodo -t upload --upload-port COM6   # con los dos conectados, elegir el puerto
pio device monitor                        # ver los logs (115200 baudios)
```

En VS Code, el entorno (gateway / nodo) se elige en la barra inferior de PlatformIO antes de subir.

**Clave LoRa:** la primera compilación crea `include/lora_key.h` (al azar, no se versiona). Los dos firmwares la usan: **sube gateway y nodo desde esta misma carpeta**. Si la borras, se genera otra y hay que subir los dos de nuevo.

Librerías (se descargan solas): RadioLib (los dos), ArduinoJson y WiFiManager (gateway). El cliente MQTT es el de ESP-IDF (`esp_mqtt_client`), que ya viene en el core de Arduino y soporta `wss://` y `mqtts://`.

**Antes de compilar el gateway** hace falta `include/ca_cert.h` (las autoridades en las que confía el ESP32). Lo genera broker_api:

```bash
# en broker_api
pnpm certs:dev --firmware <ruta de esta carpeta>
```

No se versiona: depende de la CA de cada PC de desarrollo. Si cambia la IP de la PC, basta con volver a correr ese comando en broker_api; la CA es la misma y el firmware no se recompila. Cómo funciona, qué valida el ESP32 y qué hacer en cada caso: `broker_api/docs/CERTIFICADOS.md`.

Tabla de particiones del gateway `min_spiffs.csv` (`board_build.partitions` en `platformio.ini`): 1,9 MB por app (el firmware ocupa ~59 %) manteniendo las dos particiones para OTA. La configuración guardada (NVS) está en el mismo lugar que con la tabla por defecto, así que cambiar de tabla no borra la vinculación.

## Primera configuración (vinculación)

1. Arranca **broker_api** (`pnpm dev`, con TLS configurado) y la app. La PC y el ESP32 deben estar en la misma red, con el puerto **3443** (HTTPS y WebSocket) permitido en el Firewall de Windows; también el **8883** si eliges TCP.
2. En la app: **Dispositivos → Agregar** → nombre → **Obtener código** (vale 10 minutos).
3. Enciende el **gateway** con el monitor serie abierto. Sin configuración, el LED parpadea y crea la red **`ESP32-LORA-XXXX`**. Su clave aparece en el monitor (`[boot] Clave del portal de este equipo: K7QMX-2PARN`): **anótala en una etiqueta pegada al equipo**, es la misma para siempre.
4. Conéctate a esa red desde el teléfono. Se abre el portal (si no, entra a `http://192.168.4.1`) → **Configure WiFi**.
5. Elige tu red Wi-Fi y su clave, escribe la **IP local de la PC** con broker_api (p. ej. `192.168.100.7`, no `localhost`), deja **Conexión** en `wss` y los puertos 3443 y 8883, y escribe el código.
6. Al guardar, el ESP32 canjea el código, guarda su id y token y se reinicia. En la app aparece como **en línea**.

En el monitor serie se ve cada paso. Un arranque normal se ve así:

```
[boot] Firmware 0.4.0
[boot] Clave del portal de este equipo: K7QMX-2PARN (anótala en una etiqueta)
[boot] Dispositivo 4ce6a204-…, servidor 192.168.100.7 (WebSocket)
[lora] Radio lista: 915.0 MHz, SF9, 10 dBm
[lora] Nodo del motor conectado
[wifi] Conectado, IP 192.168.100.23
[mqtt] Conectando a wss://192.168.100.7:3443/mqtt...
[mqtt] Conectado
[cmd] Motor: encender (enviando al nodo por LoRa)
[cmd] Motor ENCENDIDO (confirmado por el nodo)
```

| Prefijo | Qué informa |
|---|---|
| `[boot]` | Arranque, versión, clave del portal, botón BOOT |
| `[config]` | Portal, canje del código, configuración guardada |
| `[wifi]` | Conexión y desconexión de la red |
| `[mqtt]` | Conexión con el broker, errores de TLS (`[mqtt] TLS: ...`) o de red, reintentos y credenciales rechazadas |
| `[cmd]` | Comandos ejecutados, repetidos o rechazados |
| `[lora]` | Radio y conexión con el nodo |
| `[motor]` | (nodo) Motor encendido/apagado, apagado por seguridad |

## Reconfigurar (cambiar Wi-Fi o IP del servidor)

Con el equipo **ya funcionando**, mantén presionado **BOOT 5 segundos**. Se reinicia en el portal: el LED parpadea lento y aparece la red `ESP32-LORA-XXXX` con la clave de la etiqueta. Mientras el portal está abierto, el equipo se ve desconectado en la app.

En el portal (**Configure WiFi**) dice *"Este equipo ya está vinculado"*. Cambia solo lo que necesites:

| Quiero… | SSID y contraseña | Servidor, conexión y puertos | Código |
|---|---|---|---|
| Cambiar de red Wi-Fi | La red nueva | Sin cambios | **Vacío** |
| Cambiar la IP o dominio del servidor | **Vacíos** (conserva la red actual) | La IP o dominio nuevo (en producción, puerto API **443**) | **Vacío** |
| Cambiar entre WebSocket y TCP | Vacíos | **Conexión**: `wss` o `mqtts` | **Vacío** |
| Vincularlo a otra cuenta | Vacíos o la red nueva | Sin cambios | Un código nuevo de la app |

Al presionar **Save** guarda y se reinicia. Con el código vacío sigue siendo **el mismo dispositivo** en la app, con su historial. Si nadie usa el portal durante 5 minutos, se cierra solo y el equipo vuelve a funcionar como estaba.

Si cambias la IP del servidor, corre antes `pnpm certs:dev` en broker_api: el certificado tiene que incluir la IP nueva. La CA no cambia, así que el firmware no se recompila.

**Servidor en Render** (prueba en internet): Servidor `<servicio>.onrender.com` (sin `https://`), Conexión `wss`, Puerto API `443`. El certificado de `onrender.com` es de Google Trust Services: el `ca_cert.h` generado por `pnpm certs:dev` ya incluye sus raíces (si tu firmware es anterior, regenéralo y súbelo). Guía completa en `broker_api/docs/DESPLIEGUE.md`.

### Otras situaciones

| Situación | Qué pasa |
|---|---|
| Borrar todo (de fábrica) | Presiona **EN** y, justo después de soltarlo, mantén **BOOT** 3 s. Borra Wi-Fi, servidor y vinculación y abre el portal; hará falta un código nuevo. Si BOOT ya estaba presionado al soltar EN, la placa entra en modo de subida de firmware: presiona EN otra vez |
| El dispositivo se borró en la app o se rotó su token | Tras 3 rechazos seguidos del broker, borra sus credenciales y abre el portal (el Wi-Fi se conserva) |
| Venía de un firmware sin cifrado | Al arrancar pasa solo los puertos guardados 3000/1883 a 3443/8883 y conserva la vinculación |

## Estructura

```
include/pins.h         Pines (LoRa, relé, LED, BOOT), iguales en los dos equipos
include/lora_config.h  Parámetros de radio y tiempos del enlace LoRa
include/lora_key.h     Clave AES del enlace LoRa (generada, no se versiona)
include/app_config.h   Intervalos, puertos por defecto, clave del portal, versión
include/ca_cert.h      CAs de confianza (generado por broker_api, no se versiona)
scripts/lora_key.py    Genera lora_key.h en la primera compilación
docs/                  CABLEADO.md (hardware) y LORA.md (protocolo y seguridad)
src/
├── lora/              (los dos) Radio con tramas cifradas (secure_link) y mensajes (protocol.h)
├── nodo/main.cpp      Firmware del NODO: relé, reto, apagado por seguridad
│                      ── el resto es el GATEWAY ──
├── main.cpp           Arranque (portal o modo normal) y loop
├── remote_motor.*     Motor del nodo por LoRa: comandos con reintentos y consulta cada 20 s
├── config_store.*     NVS: servidor, puertos, id y token ("broker"); clave del portal ("device", no se borra); pedido de abrir el portal
├── provisioning.*     Portal WiFiManager (conserva red y vinculación si ya estaba vinculado; se cierra tras 5 min sin uso) y canje del código por HTTPS
├── mqtt_link.*        Cliente MQTT de ESP-IDF (wss:// o mqtts://), última voluntad, reconexión, estado y ack.
│                      Corre en su propia tarea; sus eventos pasan al loop por una cola
├── commands.*         Validación de comandos (lista blanca, repetidos, antigüedad)
└── led.*              LED de estado (motor encendido, parpadeo en el portal)
```

## Problemas comunes

| Síntoma | Causa | Solución |
|---|---|---|
| Al subir: `Wrong boot mode detected (0x13)` o se queda en `Connecting......` | El monitor serie tiene el puerto abierto, o la placa no entró sola al modo de descarga | Cerrar el monitor (Ctrl+C) y volver a subir. Si sigue, mantener **BOOT** mientras dice `Connecting...` y soltarlo cuando empiece `Writing at...` |
| Error al compilar: `ca_cert.h: No such file or directory` | Falta generar las CAs | `pnpm certs:dev --firmware <ruta de esta carpeta>` en broker_api |
| `[mqtt] TLS: X509 - Certificate verification failed…` | La IP configurada no está en el certificado del servidor, o el firmware tiene otra CA | `pnpm certs:dev --host <IP>` en broker_api y reiniciar la API; si la CA cambió, recompilar y subir |
| `[mqtt] Sin conexión con el servidor (errno …)` | No llega al servidor: IP equivocada, firewall o API apagada | Revisar la IP (portal con BOOT 5 s), el firewall (3443, u 8883 con TCP) y que broker_api esté corriendo con TLS |
| Conecta y se desconecta enseguida por WebSocket | La API no tiene `/mqtt` (versión vieja) o el proxy no deja pasar WebSocket | Actualizar broker_api; en el proxy, permitir el *upgrade* a WebSocket (Caddy lo hace solo) |
| `[config] No se pudo conectar con el servidor (connection refused)` | Puerto API equivocado (p. ej. 3000) o HTTPS apagado | Puerto **3443** en el portal; `TLS_CERT_FILE` en el `.env` de broker_api |
| `[config] El servidor rechazó el código (HTTP 400)` | Código vencido (10 min), ya usado o mal escrito | Generar otro en la app |
| No conozco la clave del portal | Se perdió la etiqueta | Conectar por USB y abrir el monitor: aparece al arrancar |
| BOOT 5 s no hace nada | El equipo está en el portal (ahí no se usa) o se soltó antes | Mirar el monitor: `BOOT soltado antes de tiempo` |

Más casos de certificados: `broker_api/docs/CERTIFICADOS.md`.

## Seguridad

- **Clave del portal propia de cada equipo**: se genera al azar la primera vez y se guarda en la NVS en un espacio que no se borra ni con el reinicio de fábrica. Se ve en el monitor serie al arrancar; anótala en una etiqueta. Si se pierde, conecta el equipo por USB y mira el monitor. Definir `CONFIG_AP_PASSWORD` en `include/app_config.h` fija una clave común (solo para pruebas).
- El token se guarda sin cifrar en la flash. Si el equipo se pierde, bórralo o rota su token desde la app: su conexión se corta al instante.
- La conexión con broker_api va cifrada (HTTPS y MQTTS) y el ESP32 rechaza un servidor cuyo certificado no esté firmado por las CAs de `ca_cert.h`, antes de enviar el token. Si el log muestra `[mqtt] TLS: ...`, el certificado del servidor no corresponde a la IP configurada o a la CA del firmware.
- No se revisa la fecha de vencimiento del certificado (el core no lo soporta); el del servidor de desarrollo dura 825 días.
- Ver `broker_api/docs/SEGURIDAD.md`.

## Pendiente para el motor

- Acciones con rangos (velocidad, rampas) validadas aquí **y** en la API, y límites físicos que ningún comando pueda superar.
- Lo pendiente del enlace LoRa (botón local de paro, varios nodos, normativa): [docs/LORA.md](docs/LORA.md#pendiente).
