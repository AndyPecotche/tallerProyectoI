#ifndef TECLADO_H_
#define TECLADO_H_

#include <stdbool.h>
#include <stdint.h>

/* Inicialización y control del teclado */
void tecladoInit(void);
void tecladoReset(void);

/* Lecturas */
int tecladoLeerPin(char *pin, uint32_t *ultimaActividad);
bool tecladoLeerTecla(char *tecla);

#endif /* TECLADO_H_ */
