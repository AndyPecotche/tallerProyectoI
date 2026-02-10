#ifndef VALIDACION_H_
#define VALIDACION_H_

#include <stdbool.h>
#include "espAT.h"
#include "mef_config.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "display.h"
#include "alertas.h"

#define MAX_PINS   99

/* Funciones principales */
bool validarPin(const char *pin);
bool agregarPin(const char *nuevoPin);
const char* obtenerTagPorPin(const char *pin);
const char* obtenerTagPorRFID(const char *rfid);
const char* obtenerTagPorHuella(const char *huellaId);
bool validarHuella(const char *huellaId);

/* Obtener el ID de huella asignado para un PIN.
 * Escribe en outId (formato "0001") y retorna true si existe.
 * Si no tiene asignado, puede derivar uno estable (p.ej., por índice).
 */
bool obtenerHuellaAsignadaPorPin(const char *pin, char outId[5]);

/* Asociación de credenciales */
bool asociarRFIDaPin(const char *pin, const char *rfid);
bool asociarHuellaaPin(const char *pin, const char *huella);

/* Validaciones de credenciales alternativas */
bool validarRFID(const char *rfid);

/* Sincronización con servidor */
unsigned char sincronizarConServidor(void);

bool esPinMaster(const char *pin);


bool eliminarHuellaLocal(const char *pin);

void eliminarTodasHuellasLocal(void);

#endif /* VALIDACION_H_ */
