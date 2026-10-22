#pragma once

// Salida controlada desde la app. Al arrancar siempre queda APAGADA (estado seguro).
namespace led {
void begin();
void set(bool on);
bool isOn();
}
