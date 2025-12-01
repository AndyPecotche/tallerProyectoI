#ifndef AS608_H_
#define AS608_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Módulo AS608 (sensor de huellas, similar R305)
 * Protocolo UART a 57600 baudios por defecto.
 */

/* Inicializa el puerto UART conectado al AS608 (57600 por defecto). */
void as608Init(uint32_t baudrate);




/* Poll no bloqueante: si hay dedo y coincide, retorna true y escribe ID/score via punteros */
bool as608PollHuella(uint16_t *idOut, uint16_t *scoreOut);


/* Enroll real: captura dos imágenes, crea modelo y almacena, devuelve ID (4 dígitos) */
bool as608Enroll(char idOut[5]);

/* Ajusta nivel de debug en tiempo de ejecución.
 * level 0: silencioso (solo mensajes de alto nivel ya existentes)
 * level 1: eventos de polling (detección dedo / errores resumidos)
 * level 2: incluye códigos de retorno frecuentes (muestra r==0x02 y 0xFF throttle)
 */
void as608SetDebug(uint8_t level);

#endif /* AS608_H_ */