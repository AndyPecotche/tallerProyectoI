#ifndef AS608_H_
#define AS608_H_

#include <stdint.h>
#include <stdbool.h>

/*
 * Módulo AS608 (sensor de huellas, similar R305)
 * Protocolo UART a 57600 baudios por defecto.
 * Estas funciones interactúan con el sensor para:
 * - grabarHuella: capturar y almacenar una nueva huella, devolviendo su ID (4 chars)
 * - leerHuella: buscar/validar una huella apoyada, devolviendo su ID (4 chars)
 *
 * Las funciones retornan true si el flujo se completó y copian el ID
 * en el buffer provisto (char id[4+1]).
 */

/* Inicializa el puerto UART conectado al AS608 (57600 por defecto). */
void as608Init(uint32_t baudrate);

/* Verifica comunicación con el sensor (handshake/VfyPwd). Retorna true si responde. */
bool as608Check(void);

/* Captura y registra una nueva huella. Devuelve true y copia el ID (4 chars). */
bool grabarHuella(char idOut[5]);

/* Lee la huella apoyada: imprime todo TX/RX y reporta ID vía log. */
bool leerHuella(void);
/* Poll no bloqueante: si hay dedo y coincide, retorna true y escribe ID/score via punteros */
bool as608PollHuella(uint16_t *idOut, uint16_t *scoreOut);

/* Prueba distintos baudios (9600/57600/115200) buscando actividad RX */
bool as608Probe(void);
/* Enroll real: captura dos imágenes, crea modelo y almacena, devuelve ID (4 dígitos) */
bool as608Enroll(char idOut[5]);

/* Ajusta nivel de debug en tiempo de ejecución.
 * level 0: silencioso (solo mensajes de alto nivel ya existentes)
 * level 1: eventos de polling (detección dedo / errores resumidos)
 * level 2: incluye códigos de retorno frecuentes (muestra r==0x02 y 0xFF throttle)
 */
void as608SetDebug(uint8_t level);
/* Imprime estadísticas acumuladas de polling (códigos GetImage, fallos, etc.) */
void as608PrintStats(void);

#endif /* AS608_H_ */