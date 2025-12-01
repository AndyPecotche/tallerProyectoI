#ifndef VALIDACION_H_
#define VALIDACION_H_

#include <stdbool.h>
#include "espAT.h"

/* Funciones principales */
bool validarPin(const char *pin);
bool agregarPin(const char *nuevoPin);
const char* obtenerTagPorPin(const char *pin);
const char* obtenerTagPorRFID(const char *rfid);
const char* obtenerTagPorHuella(const char *huellaId);
bool validarHuella(const char *huellaId);

/* Asociación de credenciales */
bool asociarRFIDaPin(const char *pin, const char *rfid);
bool asociarHuellaaPin(const char *pin, const char *huella);

/* Validaciones de credenciales alternativas */
bool validarRFID(const char *rfid);

/* Sincronización con servidor */
bool sincronizarConServidor(void);

#endif /* VALIDACION_H_ */
