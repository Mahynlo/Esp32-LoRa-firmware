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

bool paramsSaved = false;
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

void onSaveParams() {
  config.host = paramHost.getValue();
  config.apiPort = static_cast<uint16_t>(atoi(paramApiPort.getValue()));
  config.mqttPort = static_cast<uint16_t>(atoi(paramMqttPort.getValue()));
  paramsSaved = true;
}

}  // namespace

namespace provisioning {

void start(const DeviceConfig& current) {
  config = current;
  snprintf(hostBuf, sizeof(hostBuf), "%s", config.host.c_str());
  snprintf(apiPortBuf, sizeof(apiPortBuf), "%u", config.apiPort);
  snprintf(mqttPortBuf, sizeof(mqttPortBuf), "%u", config.mqttPort);
  paramHost.setValue(hostBuf, sizeof(hostBuf) - 1);
  paramApiPort.setValue(apiPortBuf, sizeof(apiPortBuf) - 1);
  paramMqttPort.setValue(mqttPortBuf, sizeof(mqttPortBuf) - 1);

  wm.addParameter(&paramHost);
  wm.addParameter(&paramApiPort);
  wm.addParameter(&paramMqttPort);
  wm.addParameter(&paramCode);
  wm.setTitle("Configurar ESP32");
  wm.setSaveParamsCallback(onSaveParams);
  wm.setBreakAfterConfig(true);        // avisar al guardar aunque falle el Wi-Fi
  wm.setConfigPortalBlocking(false);   // el loop sigue corriendo (watchdog, LED)
  wm.setConfigPortalTimeout(0);        // abierto hasta que se configure
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

  String error = claim(paramCode.getValue());
  if (error.length() > 0) {
    Serial.printf("[config] %s\n", error.c_str());
    openPortal();
    return;
  }

  config_store::save(config);
  Serial.printf("[config] Vinculado como %s. Reiniciando...\n", config.deviceId.c_str());
  led::set(false);
  delay(500);
  ESP.restart();
}

}  // namespace provisioning
