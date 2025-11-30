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
    { "11111", true, "", "", "master" }  // PIN maestro
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