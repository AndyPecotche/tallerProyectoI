#include "validacion.h"
#include "espAT.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>

#define MAX_PINS   99

/* ---------------------------------------------------------------------------
   Base de datos local de usuarios
--------------------------------------------------------------------------- */
PinUsuario_t listaPins[MAX_PINS] = {
    { "11111", true, "", "", "master" },  // PIN maestro
    { "12345", true, "9876543210FF", "0001", "andy" }
};

/* ---------------------------------------------------------------------------
    Validación de PIN existente
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
    Obtener el tag (nombre) asociado a un PIN
    - Devuelve puntero al tag si existe y está activo
    - Devuelve "usuario" si no hay tag, pero PIN existe
    - Devuelve NULL si el PIN no está en la base
--------------------------------------------------------------------------- */
const char* obtenerTagPorPin(const char *pin) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
            if (strlen(listaPins[i].tag) > 0) {
                return listaPins[i].tag;
            }
            return "usuario";
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
    Agregar un nuevo PIN (desde la app o configuración remota)
--------------------------------------------------------------------------- */
bool agregarPin(const char *nuevoPin) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (!listaPins[i].activo) {
            strcpy(listaPins[i].codigo, nuevoPin);
            listaPins[i].activo = true;
            printf("\r\n[NUEVO PIN] Registrado con éxito: %s\r\n", nuevoPin);
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
            printf("\r\n[RFID] Asociado con éxito al PIN %s\r\n", pin);
            return true;
        }
    }
    printf("\r\n[ERROR] No se encontró el PIN para asociar RFID\r\n");
    return false;
}

/* ---------------------------------------------------------------------------
    Asociar una nueva huella a un PIN existente
--------------------------------------------------------------------------- */
bool asociarHuellaaPin(const char *pin, const char *huella) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
            strcpy(listaPins[i].huella, huella);
            printf("\r\n[HUELLA] Asociada con éxito al PIN %s\r\n", pin);
            return true;
        }
    }
    printf("\r\n[ERROR] No se encontró el PIN para asociar huella\r\n");
    return false;
}

/* ---------------------------------------------------------------------------
   Validar RFID existente
--------------------------------------------------------------------------- */
bool validarRFID(const char *rfid) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strlen(listaPins[i].rfid) > 0 && strcmp(listaPins[i].rfid, rfid) == 0) {
            return true;
        }
    }
    return false;
}

/* ---------------------------------------------------------------------------
    Obtener el tag asociado a un RFID válido
    - Devuelve puntero al tag si existe y está activo
    - Devuelve "usuario" si no hay tag, pero RFID existe
    - Devuelve NULL si el RFID no está en la base
--------------------------------------------------------------------------- */
const char* obtenerTagPorRFID(const char *rfid) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strlen(listaPins[i].rfid) > 0 && strcmp(listaPins[i].rfid, rfid) == 0) {
            if (strlen(listaPins[i].tag) > 0) {
                return listaPins[i].tag;
            }
            return "usuario";
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
   Obtener el tag asociado a una huella registrada
   - huellaId es el string de 4 dígitos ("0001", etc.)
   - Devuelve puntero al tag si existe y está activo
   - Devuelve "usuario" si no hay tag pero huella coincide
   - Devuelve NULL si la huella no está asociada
--------------------------------------------------------------------------- */
const char* obtenerTagPorHuella(const char *huellaId) {
    if(!huellaId) return NULL;
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strlen(listaPins[i].huella) > 0 && strcmp(listaPins[i].huella, huellaId) == 0) {
            if (strlen(listaPins[i].tag) > 0) {
                return listaPins[i].tag;
            }
            return "usuario";
        }
    }
    return NULL;
}

/* ---------------------------------------------------------------------------
   Validar huella (ID string) y logear bienvenida centralizada
--------------------------------------------------------------------------- */
bool validarHuella(const char *huellaId){
    if(!huellaId) return false;
    printf("\r\n[DEBUG] Buscando huella: '%s'\r\n", huellaId);
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo) {
            printf("[DEBUG] Pin[%d]: activo=%d huella='%s' strlen=%u\r\n", 
                   i, listaPins[i].activo, listaPins[i].huella, (unsigned)strlen(listaPins[i].huella));
        }
        if (listaPins[i].activo && strlen(listaPins[i].huella) > 0 && strcmp(listaPins[i].huella, huellaId) == 0) {
            const char *tag = (strlen(listaPins[i].tag) > 0)? listaPins[i].tag : "usuario";
            printf("\r\n[ACCESO] Huella ID=%s - Bienvenido, %s\r\n", huellaId, tag);
            return true;
        }
    }
    printf("[DEBUG] No se encontró coincidencia para huella '%s'\r\n", huellaId);
    return false;
}

bool sincronizarConServidor(void) {
    printf("\r\n[SYNC] Iniciando sincronización con servidor...\r\n");
    
    PinUsuario_t pinsServidor[MAX_PINS];
    memset(pinsServidor, 0, sizeof(pinsServidor));
    
    size_t count = espHTTPGetPins("http://192.168.0.190:3000/syncPins/?allPins=true", 
                                   pinsServidor, MAX_PINS, 8000);
    
    if(count == 0){
        printf("\r\n[SYNC][ERROR] No se obtuvieron usuarios del servidor\r\n");
        return false;
    }
    
    printf("\r\n[SYNC] Usuarios recibidos: %u\r\n", (unsigned)count);
    
    // Actualizar listaPins con los datos del servidor
    // Mantener el PIN maestro (índice 0) y agregar los demás
    size_t destIdx = 1;
    for(size_t i = 0; i < count && destIdx < MAX_PINS; i++){
        // No sobrescribir el PIN maestro
        if(strcmp(pinsServidor[i].codigo, "11111") != 0){
            listaPins[destIdx] = pinsServidor[i];
            printf("\r\n[SYNC] Pin[%u]: %s (%s) - activo=%d\r\n",
                   (unsigned)destIdx, listaPins[destIdx].codigo, 
                   listaPins[destIdx].tag, listaPins[destIdx].activo);
            destIdx++;
        }
    }
    
    // Marcar el resto como inactivo
    for(size_t i = destIdx; i < MAX_PINS; i++){
        listaPins[i].activo = false;
        memset(listaPins[i].codigo, 0, sizeof(listaPins[i].codigo));
    }
    
    printf("\r\n[SYNC] Sincronización completada: %u usuarios activos\r\n", (unsigned)destIdx);
    return true;
}