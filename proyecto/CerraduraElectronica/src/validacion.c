#include "validacion.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#define PIN_LENGTH 5
#define MAX_PINS   5

typedef struct {
   char codigo[PIN_LENGTH + 1];
   bool activo;
   char rfid[20];                // C�digo RFID asociado (opcional)
   char huella[20];              // ID de huella asociada (opcional)
} PinUsuario_t;

/* ---------------------------------------------------------------------------
   Base de datos local de usuarios
--------------------------------------------------------------------------- */
static PinUsuario_t listaPins[MAX_PINS] = {
    { "12345", true, "", "" },
    { "54321", true, "", "" },
    { "11111", true, "", "" },  // PIN maestro
    { "00000", false, "", "" },
    { "00000", false, "", "" }
};

/* ---------------------------------------------------------------------------
   Validaci�n de PIN existente
--------------------------------------------------------------------------- */
bool validarPin(const char *pin) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
            return true;
        }
    }
    return false;
}

/* ---------------------------------------------------------------------------
   Verifica si el PIN es el maestro
--------------------------------------------------------------------------- */
bool esPinMenu(const char *pin) {
    return strcmp(pin, "#####") == 0;
}

/* ---------------------------------------------------------------------------
   Agregar un nuevo PIN (desde la app o configuraci�n remota)
--------------------------------------------------------------------------- */
bool agregarPin(const char *nuevoPin) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (!listaPins[i].activo) {
            strcpy(listaPins[i].codigo, nuevoPin);
            listaPins[i].activo = true;
            printf("\r\n[NUEVO PIN] Registrado con �xito: %s\r\n", nuevoPin);
            return true;
        }
    }
    printf("\r\n[ERROR] No hay espacio para nuevos PINs\r\n");
    return false;
}

/* ---------------------------------------------------------------------------
   Asociar un nuevo RFID a un PIN existente
--------------------------------------------------------------------------- */
bool asociarRFIDaPin(const char *pin, const char *rfid) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
            strcpy(listaPins[i].rfid, rfid);
            printf("\r\n[RFID] Asociado con �xito al PIN %s\r\n", pin);
            return true;
        }
    }
    printf("\r\n[ERROR] No se encontr� el PIN para asociar RFID\r\n");
    return false;
}

/* ---------------------------------------------------------------------------
   Asociar una nueva huella a un PIN existente
--------------------------------------------------------------------------- */
bool asociarHuellaaPin(const char *pin, const char *huella) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
            strcpy(listaPins[i].huella, huella);
            printf("\r\n[HUELLA] Asociada con �xito al PIN %s\r\n", pin);
            return true;
        }
    }
    printf("\r\n[ERROR] No se encontr� el PIN para asociar huella\r\n");
    return false;
}