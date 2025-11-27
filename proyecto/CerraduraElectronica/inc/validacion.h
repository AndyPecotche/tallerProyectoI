#ifndef VALIDACION_H_
#define VALIDACION_H_

#include <stdbool.h>

/* Funciones principales */
bool validarPin(const char *pin);
bool esPinMaestro(const char *pin);
bool agregarPin(const char *nuevoPin);

/* Asociación de credenciales */
bool asociarRFIDaPin(const char *pin, const char *rfid);
bool asociarHuellaaPin(const char *pin, const char *huella);

#endif /* VALIDACION_H_ */
