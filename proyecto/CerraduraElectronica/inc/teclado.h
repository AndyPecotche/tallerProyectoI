#ifndef TECLADO_H_
#define TECLADO_H_

#include <stdbool.h>

/* Inicialización y control del teclado */
void tecladoInit(void);
void tecladoReset(void);

/* Lecturas */
bool tecladoLeerPin(char *pin);
bool tecladoLeerTecla(char *tecla);

#endif /* TECLADO_H_ */
