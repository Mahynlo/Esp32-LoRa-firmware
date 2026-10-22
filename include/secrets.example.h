#pragma once

// Copia este archivo como include/secrets.h y llena los valores.
// secrets.h está en .gitignore: nunca lo subas al repositorio.
// (En la fase 4 esto se reemplaza por el portal de configuración.)

// Red Wi-Fi
#define WIFI_SSID "mi-red"
#define WIFI_PASSWORD "mi-clave"

// Broker MQTT: IP local de la PC donde corre broker_api (no "localhost")
#define MQTT_HOST "192.168.1.10"
#define MQTT_PORT 1883

// Dispositivo registrado en la app (POST /api/devices devuelve id y token)
#define DEVICE_ID "00000000-0000-0000-0000-000000000000"
#define DEVICE_TOKEN "token-del-dispositivo"
