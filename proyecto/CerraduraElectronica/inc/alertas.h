#ifndef ALERTAS_H_
#define ALERTAS_H_

#include "sapi.h"

// Inicializa LED y buzzer
void alertasInit(void);

// Alerta de éxito: LED fijo + pitido corto
void alertaExito(void);

// Alerta de error: 3 destellos + 3 pitidos
void alertaError(void);

#endif
