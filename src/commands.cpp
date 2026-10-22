#include "commands.h"

#include <ArduinoJson.h>
#include <time.h>

#include "app_config.h"
#include "led.h"

namespace {

// Últimos ids ejecutados, para no repetir un comando (p. ej. reenvío de QoS 1)
constexpr int RECENT_IDS = 8;
String recentIds[RECENT_IDS];
int recentIndex = 0;

bool alreadyExecuted(const String& id) {
  for (const String& seen : recentIds) {
    if (seen == id) return true;
  }
  return false;
}

void remember(const String& id) {
  recentIds[recentIndex] = id;
  recentIndex = (recentIndex + 1) % RECENT_IDS;
}

// Hora actual en ms desde época, o 0 si NTP todavía no la sincronizó
uint64_t nowMs() {
  time_t now = time(nullptr);
  if (now < 1700000000) return 0;  // antes de 2023: hora no sincronizada
  return static_cast<uint64_t>(now) * 1000;
}

CommandResult fail(const String& id, const char* error) {
  return {true, id, false, error};
}

}  // namespace

namespace commands {

CommandResult handle(const uint8_t* payload, unsigned int length) {
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) {
    Serial.println("[cmd] JSON inválido, ignorado");
    return {false, "", false, ""};
  }

  const char* idRaw = doc["id"];
  if (!idRaw) return {false, "", false, ""};
  String id(idRaw);

  if (alreadyExecuted(id)) {
    Serial.printf("[cmd] %s repetido, ignorado\n", id.c_str());
    return {false, "", false, ""};
  }
  remember(id);

  // Antigüedad: solo se puede revisar si la hora está sincronizada
  uint64_t now = nowMs();
  uint64_t issuedAt = doc["issuedAt"] | 0ULL;
  if (now > 0 && issuedAt > 0 && now > issuedAt + MAX_COMMAND_AGE_MS) {
    return fail(id, "Comando vencido");
  }

  // Lista blanca
  const char* action = doc["action"] | "";
  if (strcmp(action, "led") == 0 && doc["value"].is<bool>()) {
    bool on = doc["value"];
    led::set(on);
    Serial.printf("[cmd] LED %s\n", on ? "ENCENDIDO" : "apagado");
    return {true, id, true, ""};
  }

  return fail(id, "Comando no soportado");
}

}  // namespace commands
