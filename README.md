# esp32_lora — firmware del nodo

Firmware del ESP32 que se conecta a **broker_api** por MQTT. Hoy controla el LED integrado de la placa (GPIO2, `LED_PIN` en `include/app_config.h`) como prueba previa al motor.

- Portal de configuración propio: Wi-Fi, servidor y **código de vinculación** (sin escribir id ni token a mano).
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

## Primera configuración (vinculación)

1. Arranca **broker_api** (`pnpm dev`) y la app. La PC y el ESP32 deben estar en la misma red, con los puertos 3000 y 1883 permitidos en el Firewall de Windows.
2. En la app: **Dispositivos → Agregar** → nombre → **Obtener código** (vale 10 minutos).
3. Enciende el ESP32. Sin configuración, el LED parpadea y crea la red **`ESP32-LORA-XXXX`** (clave `lora1234`).
4. Conéctate a esa red desde el teléfono. Se abre el portal (si no, entra a `http://192.168.4.1`) → **Configure WiFi**.
5. Elige tu red Wi-Fi y su clave, escribe la **IP local de la PC** con broker_api (p. ej. `192.168.100.7`, no `localhost`), deja los puertos 3000 y 1883 y escribe el código.
6. Al guardar, el ESP32 canjea el código, guarda su id y token y se reinicia. En la app aparece como **en línea**.

En el monitor serie se ve cada paso (`[config]`, `[wifi]`, `[mqtt]`, `[cmd]`).

## Reconfigurar

| Situación | Qué pasa |
|---|---|
| Mantener **BOOT** presionado 3 s al encender | Borra todo (Wi-Fi, servidor, credenciales) y abre el portal |
| El dispositivo se borró en la app o se rotó su token | Tras 3 rechazos seguidos del broker, borra sus credenciales y abre el portal (el Wi-Fi se conserva) |
| Cambió la IP de la PC | Reconfigurar con BOOT |

## Estructura

```
include/app_config.h   Pines, intervalos, puertos por defecto, clave del portal, versión
src/
├── main.cpp           Arranque (portal o modo normal) y loop
├── config_store.*     Configuración en la memoria no volátil (NVS)
├── provisioning.*     Portal WiFiManager y canje del código (POST /api/devices/claim)
├── mqtt_link.*        Conexión MQTT, última voluntad, reconexión, estado y ack
├── commands.*         Validación de comandos (lista blanca, repetidos, antigüedad)
└── led.*              Salida controlada (después: motor)
```

## Seguridad

- Cambia `CONFIG_AP_PASSWORD` en `include/app_config.h`: mientras el portal está abierto, quien la conozca y esté cerca podría configurar el equipo.
- El token se guarda sin cifrar en la flash. Si el equipo se pierde, bórralo o rota su token desde la app: su conexión se corta al instante.
- Sin TLS todavía: solo para la red local de pruebas. Ver `broker_api/docs/SEGURIDAD.md`.

## Pendiente para el motor

- Acciones con rangos (velocidad, rampas) validadas aquí **y** en la API, y límites físicos que ningún comando pueda superar.
- Tiempo máximo sin conexión tras el cual el motor se detiene.
- La flash está al 81 %: para sumar LoRa y el motor, cambiar la tabla de particiones (`board_build.partitions = huge_app.csv` o una propia con OTA).
