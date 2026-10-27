#include "provisioning.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <esp_task_wdt.h>

#include "app_config.h"
#include "ca_cert.h"
#include "led.h"

namespace {

WiFiManager wm;
DeviceConfig config;

// Campos extra del portal (además de la red Wi-Fi)
char hostBuf[64];
char apiPortBuf[6];
char mqttPortBuf[6];
WiFiManagerParameter paramHost("host", "Servidor (IP de la PC con broker_api)", "", sizeof(hostBuf) - 1);
WiFiManagerParameter paramApiPort("apiPort", "Puerto API (HTTPS)", "", sizeof(apiPortBuf) - 1);
WiFiManagerParameter paramMqttPort("mqttPort", "Puerto MQTT (MQTTS)", "", sizeof(mqttPortBuf) - 1);
WiFiManagerParameter paramCode("code", "Código de vinculación (lo muestra la app)", "", 12);

// Ayuda que se muestra arriba de los campos cuando el equipo ya está vinculado
WiFiManagerParameter paramPairedHelp(
    "<p><b>Este equipo ya está vinculado.</b><br>"
    "Deja <b>SSID y contraseña vacíos</b> para conservar la red Wi-Fi actual, "
    "y el <b>código vacío</b> para conservar la vinculación. "
    "Cambia solo lo que necesites y presiona <b>Save</b>.</p>");

bool paramsSaved = false;
bool wasPaired = false;  // al abrir el portal ya tenía id y token
unsigned long lastBlink = 0;

String apName() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char name[24];
  snprintf(name, sizeof(name), "ESP32-LORA-%02X%02X", mac[4], mac[5]);
  return name;
}

void openPortal() {
  String name = apName();
  Serial.printf("[config] Portal abierto: red \"%s\", clave \"%s\"\n", name.c_str(), CONFIG_AP_PASSWORD);
  Serial.println("[config] Conéctate desde el teléfono y abre http://192.168.4.1");
  wm.startConfigPortal(name.c_str(), CONFIG_AP_PASSWORD);
}

// Canjea el código por id y token por HTTPS. Devuelve un mensaje de error o "" si salió bien.
String claim(const char* code) {
  // El token llega en esta respuesta: solo se acepta un servidor con certificado de CA_CERTS
  WiFiClientSecure tls;
  tls.setCACert(CA_CERTS);
  tls.setHandshakeTimeout(TLS_HANDSHAKE_TIMEOUT_S);

  HTTPClient http;
  String url = "https://" + config.host + ":" + config.apiPort + "/api/devices/claim";
  Serial.printf("[config] Canjeando código en %s\n", url.c_str());
  http.setTimeout(10000);
  if (!http.begin(tls, url)) return "URL inválida";
  http.addHeader("Content-Type", "application/json");

  JsonDocument body;
  body["code"] = code;
  body["chipId"] = WiFi.macAddress();
  String payload;
  serializeJson(body, payload);

  int status = http.POST(payload);
  String response = http.getString();
  http.end();

  if (status <= 0) {
    char tlsError[100];
    String detail = tls.lastError(tlsError, sizeof(tlsError)) != 0 ? String(" - TLS: ") + tlsError : "";
    return "No se pudo conectar con el servidor (" + HTTPClient::errorToString(status) + ")" + detail;
  }
  if (status != 201) return "El servidor rechazó el código (HTTP " + String(status) + "): " + response;

  JsonDocument doc;
  if (deserializeJson(doc, response)) return "Respuesta inválida del servidor";
  const char* deviceId = doc["deviceId"];
  const char* token = doc["token"];
  if (!deviceId || !token) return "Respuesta incompleta del servidor";

  config.deviceId = deviceId;
  config.token = token;
  config.mqttPort = doc["mqttPort"] | config.mqttPort;
  return "";
}

// Puerto escrito en el portal, o el anterior si quedó vacío o no es un número válido
uint16_t portOr(const char* value, uint16_t fallback) {
  int port = atoi(value);
  return port > 0 && port <= 65535 ? static_cast<uint16_t>(port) : fallback;
}

void onSaveParams() {
  String host = paramHost.getValue();
  host.trim();
  if (host.length() > 0) config.host = host;
  config.apiPort = portOr(paramApiPort.getValue(), config.apiPort);
  config.mqttPort = portOr(paramMqttPort.getValue(), config.mqttPort);
  paramsSaved = true;
}

// Sin uso del portal en un equipo vinculado: volver a funcionar como estaba
void onPortalTimeout() {
  Serial.println("[config] Portal sin uso: se cierra y se conserva la configuración anterior");
  led::set(false);
  ESP.restart();
}

}  // namespace

namespace provisioning {

void start(const DeviceConfig& current) {
  config = current;
  wasPaired = config.paired();
  snprintf(hostBuf, sizeof(hostBuf), "%s", config.host.c_str());
  snprintf(apiPortBuf, sizeof(apiPortBuf), "%u", config.apiPort);
  snprintf(mqttPortBuf, sizeof(mqttPortBuf), "%u", config.mqttPort);
  paramHost.setValue(hostBuf, sizeof(hostBuf) - 1);
  paramApiPort.setValue(apiPortBuf, sizeof(apiPortBuf) - 1);
  paramMqttPort.setValue(mqttPortBuf, sizeof(mqttPortBuf) - 1);

  if (wasPaired) wm.addParameter(&paramPairedHelp);
  wm.addParameter(&paramHost);
  wm.addParameter(&paramApiPort);
  wm.addParameter(&paramMqttPort);
  wm.addParameter(&paramCode);
  wm.setTitle("Configurar ESP32");
  wm.setSaveParamsCallback(onSaveParams);
  wm.setBreakAfterConfig(true);        // avisar al guardar aunque falle el Wi-Fi
  wm.setConfigPortalBlocking(false);   // el loop sigue corriendo (watchdog, LED)
  if (wasPaired) {
    // Ya funcionaba: si nadie usa el portal, se cierra solo (cuenta desde el último uso)
    wm.setConfigPortalTimeout(CONFIG_PORTAL_TIMEOUT_S);
    wm.setConfigPortalTimeoutCallback(onPortalTimeout);
  } else {
    wm.setConfigPortalTimeout(0);      // equipo nuevo: abierto hasta que se configure
  }
  wm.setConnectTimeout(15);            // segundos para conectar a la red elegida

  openPortal();
}

void loop() {
  wm.process();

  // Parpadeo lento: esperando configuración
  if (millis() - lastBlink > 500) {
    lastBlink = millis();
    led::set(!led::isOn());
  }

  if (!paramsSaved) return;
  paramsSaved = false;

  // Con SSID vacío WiFiManager no se conecta: usar la red Wi-Fi guardada
  if (WiFi.status() != WL_CONNECTED) WiFi.begin();

  // Esperar la conexión a la red elegida
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    esp_task_wdt_reset();
    delay(250);
  }
  esp_task_wdt_reset();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[config] No se pudo conectar a la red Wi-Fi. Revisa la clave.");
    openPortal();
    return;
  }

  String code = paramCode.getValue();
  code.trim();
  if (code.length() == 0 && !wasPaired) {
    Serial.println("[config] Falta el código de vinculación (lo muestra la app en Agregar dispositivo)");
    openPortal();
    return;
  }

  if (code.length() > 0) {
    // Código nuevo: el equipo se registra de nuevo (en esta u otra cuenta)
    String error = claim(code.c_str());
    if (error.length() > 0) {
      Serial.printf("[config] %s\n", error.c_str());
      openPortal();
      return;
    }
    Serial.printf("[config] Vinculado como %s\n", config.deviceId.c_str());
  } else {
    Serial.println("[config] Configuración actualizada; se conserva la vinculación");
  }

  config_store::save(config);
  Serial.printf("[config] Servidor %s (API %u, MQTT %u). Reiniciando...\n",
                config.host.c_str(), config.apiPort, config.mqttPort);
  led::set(false);
  delay(500);
  ESP.restart();
}

}  // namespace provisioning
