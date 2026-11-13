#pragma once

// LED de estado del gateway (motor encendido / parpadeo en el portal). Arranca apagado.
namespace led {
void begin();
void set(bool on);
bool isOn();
}
