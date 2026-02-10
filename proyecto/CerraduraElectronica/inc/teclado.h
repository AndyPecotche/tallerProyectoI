#ifndef TECLADO_H_
#define TECLADO_H_

#include <stdbool.h>
#include <stdint.h>
#include "display.h"

#define PIN_LENGTH 5
/* Inicialización y control del teclado */
void tecladoInit(void);
void tecladoReset(void);

/* Lecturas */
int tecladoLeerPin(char *pin, uint32_t *ultimaActividad);
bool tecladoLeerTecla(char *tecla);

#endif /* TECLADO_H_ */
