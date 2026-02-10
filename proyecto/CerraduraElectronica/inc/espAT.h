#ifndef ESPAT_H_
#define ESPAT_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "mef_config.h"
#include <stdlib.h>
#include "sapi.h"
#include <string.h>
#include "display.h"

#define PIN_LENGTH 5

/* Estructura de usuario con credenciales */
typedef struct {
    char codigo[PIN_LENGTH + 1];
    bool activo;
    char rfid[20];
    char huella[5];  // 4 dígitos + null terminator
    char tag[20];
} PinUsuario_t;

extern bool conectado;

/* Inicializa UART3 (U3_TDX/U3_RTX) para el ESP */
void espATInit(uint32_t baudrate);

/*
 * Envía un comando AT por UART3 y lee la respuesta.
 * - cmd: comando ASCII (sin CRLF; la función agregará "\r\n").
 * - respuesta: buffer donde se almacena la respuesta recibida.
 * - maxLen: tamaño máximo del buffer de respuesta.
 * - timeoutMs: tiempo máximo de espera para recibir respuesta.
 * Devuelve la cantidad de bytes almacenados en 'respuesta'.
 */
size_t enviarComandoAT(const char *cmd, char *respuesta, size_t maxLen, uint32_t timeoutMs);

/* Chequea conexión AT básica (envía AT y espera OK) */
bool espATCheck(uint32_t timeoutMs);

/*
 * Obtiene lista de usuarios desde servidor HTTP vía ESP-AT.
 * - url: URL completa para HTTP GET
 * - pinsOut: array donde almacenar usuarios parseados
 * - maxPins: tamaño máximo del array pinsOut
 * - timeoutMs: timeout para la petición HTTP
 * Devuelve cantidad de usuarios parseados exitosamente.
 */
size_t espHTTPGetPins(PinUsuario_t *pinsOut, size_t maxPins, uint32_t timeoutMs);
bool inicializarESP(uint32_t timeoutMs);
bool resetearCredencialesESP(void);
bool espEnviarNuevaHuella(const char *pin, const char *idHuella);
bool espEnviarNuevoRFID(const char* pin, const char* rfidHex);
bool espBorrarHuellaServidor(const char *pin);
bool espBorrarTodasHuellasServidor(void);
double espObtenerTiempoServidor(void);
bool espRawHTTPGet(const char *path, char *bufferResp, int bufferSize);
#endif /* ESPAT_H_ */
