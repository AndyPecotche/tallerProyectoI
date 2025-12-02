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


/* Enroll real: captura dos imágenes, crea modelo y almacena.
 * Variante 1: almacena en el primer ID libre y devuelve el ID (4 dígitos) */
bool as608Enroll(char idOut[5]);

/* Variante 2: almacena en un ID específico (sobrescribe si existe).
 * Retorna true en éxito y escribe el mismo ID en idOut (formato 4 dígitos). */
bool as608EnrollAtId(uint16_t idTarget, char idOut[5]);

/* Ajusta nivel de debug en tiempo de ejecución.
 * level 0: silencioso (solo mensajes de alto nivel ya existentes)
 * level 1: eventos de polling (detección dedo / errores resumidos)
 * level 2: incluye códigos de retorno frecuentes (muestra r==0x02 y 0xFF throttle)
 */
void as608SetDebug(uint8_t level);

/* ---------------------------------------------------------------
 * Mini MEF (estado interno) para escaneo no bloqueante del AS608
 * ---------------------------------------------------------------
 * El objetivo es dividir la operación GetImage -> Image2Tz -> Search
 * en pasos breves que se avanzan en cada llamada, evitando bloqueos
 * largos y permitiendo que el keypad y la MEF principal sigan fluidos.
 */

typedef enum {
	AS608_SCAN_IDLE = 0,
	AS608_SCAN_INPROGRESS,
	AS608_SCAN_MATCH,
	AS608_SCAN_NOMATCH,
	AS608_SCAN_ERROR
} as608ScanStatus_t;



/* Avanza la mini MEF en pasos cortos (2-5ms por llamada).
	bool as608EnrollAtId(uint16_t idTarget, char idOut[5]);
 * - AS608_SCAN_INPROGRESS: continuar llamando en el próximo ciclo
 * - AS608_SCAN_MATCH: se encontró coincidencia, idOut/scoreOut válidos
 * - AS608_SCAN_NOMATCH: sin coincidencia o sin dedo
 * - AS608_SCAN_ERROR: error de comunicación o protocolo
 * - AS608_SCAN_IDLE: aún no comenzó; se inicia automáticamente en la primera llamada
 */
as608ScanStatus_t as608ScanStep(uint16_t *idOut, uint16_t *scoreOut);

/* Obtiene la cantidad de templates almacenados (para diagnóstico). */
int as608GetTemplateCount(void);

/* Borra todos los templates almacenados en el sensor (comando Empty 0x0D). */
bool as608ClearAllTemplates(void);

#endif /* AS608_H_ */