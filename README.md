# esp32_lora — firmware del nodo

Firmware del ESP32 que se conecta a **broker_api** por MQTT. Hoy controla el LED integrado de la placa (GPIO2, `LED_PIN` en `include/app_config.h`) como prueba previa al motor.

- Portal de configuración propio: Wi-Fi, servidor y **código de vinculación** (sin escribir id ni token a mano). Se vuelve a abrir con **BOOT 5 s** para cambiar la red o la IP sin perder la vinculación.
- **Todo cifrado**: canjea el código por HTTPS y se conecta por MQTTS, validando el certificado del servidor.
- Enciende/apaga el LED con los comandos de la app y confirma cada uno (`ack`).
- Informa su estado al conectar, al cambiar y cada 30 s (latido).
- **Última voluntad** MQTT: si se corta la corriente, el broker avisa a la app en ~22 s.
- Informa el **motivo del último reinicio** (corte de energía, caída de voltaje, watchdog…).
- **Estado seguro**: el LED arranca siempre apagado.
- Reconexión de Wi-Fi y MQTT con espera creciente (1 s → 30 s) y **watchdog** de 30 s.

Protocolo (tópicos y mensajes): `broker_api/docs/MQTT.md`.

## Compilar y subir

Con la extensión de PlatformIO en VS Code (botones de la barra inferior) o en terminal:

```bash
pio run                 # compilar
pio run -t upload       # subir al ESP32 (USB)
pio device monitor      # ver los logs (115200 baudios)
```

Librerías (se descargan solas): PubSubClient, ArduinoJson, WiFiManager.

**Antes de compilar** hace falta `include/ca_cert.h` (las autoridades en las que confía el ESP32). Lo genera broker_api:

```bash
# en broker_api
pnpm certs:dev --firmware <ruta de esta carpeta>
```

No se versiona: depende de la CA de cada PC de desarrollo. Si cambia la IP de la PC, basta con volver a correr ese comando en broker_api; la CA es la misma y el firmware no se recompila.

Tabla de particiones `min_spiffs.csv`: 1,9 MB por app (el firmware ocupa ~54 %) manteniendo OTA.

## Primera configuración (vinculación)

1. Arranca **broker_api** (`pnpm dev`, con TLS configurado) y la app. La PC y el ESP32 deben estar en la misma red, con los puertos **3443** (HTTPS) y **8883** (MQTTS) permitidos en el Firewall de Windows.
2. En la app: **Dispositivos → Agregar** → nombre → **Obtener código** (vale 10 minutos).
3. Enciende el ESP32. Sin configuración, el LED parpadea y crea la red **`ESP32-LORA-XXXX`** (clave `lora1234`).
4. Conéctate a esa red desde el teléfono. Se abre el portal (si no, entra a `http://192.168.4.1`) → **Configure WiFi**.
5. Elige tu red Wi-Fi y su clave, escribe la **IP local de la PC** con broker_api (p. ej. `192.168.100.7`, no `localhost`), deja los puertos 3443 y 8883 y escribe el código.
6. Al guardar, el ESP32 canjea el código, guarda su id y token y se reinicia. En la app aparece como **en línea**.

En el monitor serie se ve cada paso (`[config]`, `[wifi]`, `[mqtt]`, `[cmd]`).

## Reconfigurar (cambiar Wi-Fi o IP del servidor)

Con el equipo **ya funcionando**, mantén presionado **BOOT 5 segundos**. Se reinicia en el portal: el LED parpadea lento y aparece la red `ESP32-LORA-XXXX` (clave `lora1234`). Mientras el portal está abierto, el equipo se ve desconectado en la app.

En el portal (**Configure WiFi**) dice *"Este equipo ya está vinculado"*. Cambia solo lo que necesites:

| Quiero… | SSID y contraseña | Servidor y puertos | Código |
|---|---|---|---|
| Cambiar de red Wi-Fi | La red nueva | Sin cambios | **Vacío** |
| Cambiar la IP del servidor | **Vacíos** (conserva la red actual) | La IP nueva | **Vacío** |
| Vincularlo a otra cuenta | Vacíos o la red nueva | Sin cambios | Un código nuevo de la app |

Al presionar **Save** guarda y se reinicia. Con el código vacío sigue siendo **el mismo dispositivo** en la app, con su historial. Si nadie usa el portal durante 5 minutos, se cierra solo y el equipo vuelve a funcionar como estaba.

Si cambias la IP del servidor, corre antes `pnpm certs:dev` en broker_api: el certificado tiene que incluir la IP nueva. La CA no cambia, así que el firmware no se recompila.

### Otras situaciones

| Situación | Qué pasa |
|---|---|
| Borrar todo (de fábrica) | Presiona **EN** y, justo después de soltarlo, mantén **BOOT** 3 s. Borra Wi-Fi, servidor y vinculación y abre el portal; hará falta un código nuevo. Si BOOT ya estaba presionado al soltar EN, la placa entra en modo de subida de firmware: presiona EN otra vez |
| El dispositivo se borró en la app o se rotó su token | Tras 3 rechazos seguidos del broker, borra sus credenciales y abre el portal (el Wi-Fi se conserva) |
| Venía de un firmware sin cifrado | Al arrancar pasa solo los puertos guardados 3000/1883 a 3443/8883 y conserva la vinculación |

## Estructura

```
include/app_config.h   Pines, intervalos, puertos por defecto, clave del portal, versión
include/ca_cert.h      CAs de confianza (generado por broker_api, no se versiona)
src/
├── main.cpp           Arranque (portal o modo normal) y loop
├── config_store.*     Configuración en la memoria no volátil (NVS)
├── provisioning.*     Portal WiFiManager y canje del código por HTTPS (POST /api/devices/claim)
├── mqtt_link.*        Conexión MQTTS, última voluntad, reconexión, estado y ack
├── commands.*         Validación de comandos (lista blanca, repetidos, antigüedad)
└── led.*              Salida controlada (después: motor)
```

## Seguridad

- Cambia `CONFIG_AP_PASSWORD` en `include/app_config.h`: mientras el portal está abierto, quien la conozca y esté cerca podría configurar el equipo.
- El token se guarda sin cifrar en la flash. Si el equipo se pierde, bórralo o rota su token desde la app: su conexión se corta al instante.
- La conexión con broker_api va cifrada (HTTPS y MQTTS) y el ESP32 rechaza un servidor cuyo certificado no esté firmado por las CAs de `ca_cert.h`, antes de enviar el token. Si el log muestra `[mqtt] TLS: ...`, el certificado del servidor no corresponde a la IP configurada o a la CA del firmware.
- No se revisa la fecha de vencimiento del certificado (el core no lo soporta); el del servidor de desarrollo dura 825 días.
- Ver `broker_api/docs/SEGURIDAD.md`.

## Pendiente para el motor

- Acciones con rangos (velocidad, rampas) validadas aquí **y** en la API, y límites físicos que ningún comando pueda superar.
- Tiempo máximo sin conexión tras el cual el motor se detiene.
- Espacio: con `min_spiffs.csv` quedan ~0,9 MB libres para LoRa y el motor; si no alcanza, `huge_app.csv` (3 MB, sin OTA).
