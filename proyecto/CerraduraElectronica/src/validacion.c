#include "validacion.h"


/* ---------------------------------------------------------------------------
   Base de datos local de usuarios
--------------------------------------------------------------------------- */
PinUsuario_t listaPins[MAX_PINS] = {
    { "11111", true, "", "", "master" },  // PIN maestro
    { "22222", true, "9876543210FF", "0001", "andy" },
    { "33333", true, "1234567890AB", "0002", "pepe" },
    { "44444", true, "1211899891AA", "0003", "juan" },
    { "55555", true, "", "0004", "invitado" },
    { "66666", true, "", "", "usuario6" } //Tiraria error al intentar grabar Huella
};
// Funci�n p�blica para sobrescribir la base de datos local
void actualizarBaseDeDatos(PinUsuario_t *nuevosUsuarios, int cantidad) {
    if (cantidad > MAX_PINS) {
        cantidad = MAX_PINS; // Protecci�n para no desbordar
    }

    // 1. Limpiar la base de datos actual (poner todo en cero)
    memset(listaPins, 0, sizeof(listaPins));

    // 2. Copiar los nuevos datos recibidos
    // Copiamos bloque de memoria directo, es m�s r�pido que un for
    memcpy(listaPins, nuevosUsuarios, cantidad * sizeof(PinUsuario_t));

    printf("\r\n[DB] Base de datos actualizada con %d usuarios.\r\n", cantidad);

    // Debug: Imprimir el primer usuario para verificar
    if(cantidad > 0){
        printf("[DB] Usuario 1: %s (Tag: %s)\r\n", listaPins[0].codigo, listaPins[0].tag);
    }
}

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
//bool asociarHuellaaPin(const char *pin, const char *huella) {
//    for (int i = 0; i < MAX_PINS; i++) {
//        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
//            strcpy(listaPins[i].huella, huella);
//            printf("\r\n[HUELLA] Asociada con éxito al PIN %s\r\n", pin);
//            return true;
//        }
//    }
//    printf("\r\n[ERROR] No se encontró el PIN para asociar huella\r\n");
//    return false;
//}
// 2. Guarda el ID en la memoria
bool asociarHuellaaPin(const char *pin, const char *idHuella) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
            // Guardamos el ID en la estructura
            strncpy(listaPins[i].    huella, idHuella, 4);
            listaPins[i].huella[4] = '\0'; // Seguridad

            printf("\r\n[DB] Huella %s vinculada a %s\r\n", idHuella, listaPins[i].tag);
            return true;
        }
    }
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
    printf("\r\n[DEBUG] Buscando RFID y se devolvera tag si se encuentra: '%s'\r\n", rfid);
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strlen(listaPins[i].rfid) > 0 && strcmp(listaPins[i].rfid, rfid) == 0) {
            if (strlen(listaPins[i].tag) > 0) {
                return listaPins[i].tag;
            }
            return NULL;
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

/* ---------------------------------------------------------------------------
   Obtener el ID de huella asignado para un PIN
   - Si tiene asignado en listaPins, se devuelve ese
   - Si no, se deriva uno estable usando el índice del arreglo
--------------------------------------------------------------------------- */
//bool obtenerHuellaAsignadaPorPin(const char *pin, char outId[5]){
//    if(!pin || !outId) return false;
//    for (int i = 0; i < MAX_PINS; i++) {
//        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
//            if (strlen(listaPins[i].huella) >= 4) {
//                strncpy(outId, listaPins[i].huella, 4);
//                outId[4] = '\0';
//                return true;
//            } else {
//                // No tiene ID de huella asignado: no permitir continuar
//                return false;
//            }
//        }
//    }
//    return false;
//}

// 1. Obtiene el ID. Si no existe, lo genera (Index + 1).
bool obtenerHuellaAsignadaPorPin(const char *pin, char *idBuffer) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {

            // A) Si ya tiene ID, lo usamos
            if (strlen(listaPins[i].huella) > 0) {
                strcpy(idBuffer, listaPins[i].huella);
            }
            // B) Si est� vac�o, generamos uno nuevo autom�tico
            else {
                // Generamos ID basado en su posici�n (�ndice + 1)
                // Ej: El usuario en la posici�n 2 tendr� ID "0003"
                sprintf(idBuffer, "%04d", i + 1);
            }
            return true; // Encontrado y con ID listo
        }
    }
    return false; // PIN no existe
}


//bool sincronizarConServidor(void) {
//    printf("\r\n[SYNC] Iniciando sincronización con servidor...\r\n");
//
//    PinUsuario_t pinsServidor[MAX_PINS];
//    memset(pinsServidor, 0, sizeof(pinsServidor));
//
//    size_t count = espHTTPGetPins(ESP_SERVER_URL,
//                                   pinsServidor, MAX_PINS, 8000);
//
//    if(count == 0){
//        printf("\r\n[SYNC][ERROR] No se obtuvieron usuarios del servidor\r\n");
//        return false;
//    }
//
//    printf("\r\n[SYNC] Usuarios recibidos: %u\r\n", (unsigned)count);
//
//    // Actualizar listaPins con los datos del servidor
//    // Mantener el PIN maestro (índice 0) y agregar los demás
//    size_t destIdx = 1;
//    for(size_t i = 0; i < count && destIdx < MAX_PINS; i++){
//        // No sobrescribir el PIN maestro
//        if(strcmp(pinsServidor[i].codigo, "11111") != 0){
//            listaPins[destIdx] = pinsServidor[i];
//            printf("\r\n[SYNC] Pin[%u]: %s (%s) - activo=%d\r\n",
//                   (unsigned)destIdx, listaPins[destIdx].codigo,
//                   listaPins[destIdx].tag, listaPins[destIdx].activo);
//            destIdx++;
//        }
//    }
//
//    // Marcar el resto como inactivo
//    for(size_t i = destIdx; i < MAX_PINS; i++){
//        listaPins[i].activo = false;
//        memset(listaPins[i].codigo, 0, sizeof(listaPins[i].codigo));
//    }
//
//    printf("\r\n[SYNC] Sincronización completada: %u usuarios activos\r\n", (unsigned)destIdx);
//    return true;
//}

unsigned char sincronizarConServidor(void) {
	unsigned char valor = 0;
    display_clear();
    display_println("SINCRONIZANDO...", 20);
    display_update();
    
    // 1. Buffer temporal para recibir los datos
    // Es importante NO escribir directamente en listaPins por si falla la descarga
    static PinUsuario_t bufferUsuarios[MAX_PINS];

    // Limpiamos el buffer antes de usar
    memset(bufferUsuarios, 0, sizeof(bufferUsuarios));

    // 2. Pedir datos al servidor
    // Usamos la URL definida en mef_config.h
    printf("\r\n[SYNC] Solicitando datos a: %s\r\n", ESP_SERVER_URL);

    size_t usuariosRecibidos = espHTTPGetPins(
                                    bufferUsuarios,
                                    MAX_PINS,
                                    15000 // Timeout 15 segundos
                                );

    // 3. Verificar si llegaron datos
    display_clear();
    if (usuariosRecibidos > 0) {
        printf("\r\n[SYNC] �xito. Recibidos %d usuarios.\r\n", (int)usuariosRecibidos);

        // 4. ACTUALIZAR LA VARIABLE GLOBAL "listaPins"
        actualizarBaseDeDatos(bufferUsuarios, usuariosRecibidos);

        display_println("SYNC COMPLETADA", 20);
        char buff[20];
        sprintf(buff, "USUARIOS: %d", (int)usuariosRecibidos);
        display_println(buff, 40);

    } else {
    	valor = 1;
        printf("\r\n[SYNC] Error: No se recibieron datos o JSON inv�lido.\r\n");
        display_println("ERROR DE SYNC", 20);
        display_println("MANTENIENDO DB", 40); // Mantenemos los datos viejos
    }
    display_update();
    delay(1000);
    display_clear();
    return valor;
}

bool esPinMaster(const char *pin) {
    // Verificamos si el PIN coincide con el del usuario en la posici�n 0 (Master)
    // Y adem�s verificamos que el Master est� activo.
    if (listaPins[0].activo && strcmp(listaPins[0].codigo, pin) == 0) {
        return true;
    }
    return false;
}

// Borra el ID de huella de un usuario espec�fico
bool eliminarHuellaLocal(const char *pin) {
    for (int i = 0; i < MAX_PINS; i++) {
        if (listaPins[i].activo && strcmp(listaPins[i].codigo, pin) == 0) {
            memset(listaPins[i].huella, 0, sizeof(listaPins[i].huella)); // Limpiar string
            return true;
        }
    }
    return false;
}

// Borra todas las huellas de la memoria RAM
void eliminarTodasHuellasLocal(void) {
    for (int i = 0; i < MAX_PINS; i++) {
         memset(listaPins[i].huella, 0, sizeof(listaPins[i].huella));
    }
}

