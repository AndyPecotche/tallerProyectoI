#include "espAT.h"

//#include "sapi_peripheral_map.h"

/* UART del ESP en LPC4337: U3_TDX/U3_RTX -> indice 5 (UART_232 en sAPI) */
static const uartMap_t ESP_UART = (uartMap_t)5; // UART3 (RS232) indice 5
bool conectado = false;

void espATInit(uint32_t baudrate){
    if(baudrate == 0){
        baudrate = 115200;
    }

    uartConfig(ESP_UART, baudrate);

    printf("\r\n[ESP][espATInit] ################################################################\r\n");
	printf("\r\n[ESP][espATInit] Inicia espATInit Forzando reinicio del modulo...\r\n");

	// 1. Por si acaso qued� en "Modo Transparente" (descarga colgada)
	// Enviamos "+++" sin \r\n y esperamos 1 segundo. Esto corta cualquier transmisi�n.
	uartWriteString(ESP_UART, "+++");
	delay(1000);

	// 2. Enviamos el comando de RESET de software
	// AT+RST reinicia el chip (igual que quitarle la energ�a)
	uartWriteString(ESP_UART, "AT+RST\r\n");

	// 3. Esperamos a que arranque
	printf("[ESP][espATInit] Esperando reinicio (1s)...\r\n");
	delay(1000);

	// 4. Limpiar basura del buffer UART
	uint8_t dummy;
	while(uartReadByte(ESP_UART, &dummy));

    printf("\r\n[ESP][espATInit] Init UART idx=%d baud=%u\r\n", (int)ESP_UART, (unsigned)baudrate);
    printf("[ESP][espATInit] UART configurada, listo para comandos AT, termino ejecucion espATInit\r\n");
}

// Inicializa el ESP: AT handshake, luego intenta auto-conectar WiFi
// Si falla, habilita BluFi para provisioning manual
bool inicializarESP(uint32_t timeoutMs){
    printf("\r\n[ESP][inicializarESP] ##############inicializarESP############### Inicializando ESP...\r\n");
    char resp[256];
    bool ok = false;

    // 1) Enviar "AT" periódicamente hasta recibir "OK"
    uint32_t start = tickRead();
    while ((tickRead() - start) < timeoutMs){
        size_t n = enviarComandoAT("AT", resp, sizeof(resp), 1000);
        if (n > 0 && strstr(resp, "OK") != NULL){
            printf("\r\n[ESP][inicializarESP] AT -> OK\r\n");
            ok = true;
            break;
        } else {
            printf("\r\n[ESP][inicializarESP] AT sin OK, reintentando...\r\n");
            delay(500);
        }
    }

    if(!ok){
        printf("\r\n[ESP][inicializarESP] No se pudo establecer comunicación AT\r\n");
        return false;
    }

    // 2) Configurar modo Station y habilitar auto-reconexión
    enviarComandoAT("AT+CWMODE=1", resp, sizeof(resp), 1000);
    printf("\r\n[ESP][inicializarESP] Modo Station configurado\r\n");

    enviarComandoAT("AT+CIPMUX=0", resp, sizeof(resp), 1000);
    printf("\r\n[ESP][inicializarESP] Forzando modo Single Connection (CIPMUX=0)\r\n");

    enviarComandoAT("AT+CWAUTOCONN=1", resp, sizeof(resp), 1000);
    printf("\r\n[ESP][inicializarESP] Auto-reconexión habilitada\r\n");

	// --- ESPERAR CONEXI�N REAL ---
	uint32_t waitStart = tickRead();

	// Esperamos hasta 15 segundos a que nos den IP
	while((tickRead() - waitStart) < 15000){
		// AT+CIPSTATUS devuelve:
		// STATUS:2 -> Got IP (�xito)
		// STATUS:3 -> Connected (�xito, socket abierto)
		// STATUS:5 -> WiFi Disconnected (A�n no conecta)
		enviarComandoAT("AT+CIPSTATUS", resp, sizeof(resp), 1000);

		if(strstr(resp, "STATUS:2") != NULL || strstr(resp, "STATUS:3") != NULL){
			printf("\r\n[ESP][inicializarESP] WiFi Conectado y con IP!\r\n");
			enviarComandoAT("AT+CIFSR", resp, sizeof(resp), 1000);
			display_println("WiFi CONECTADO", 50);
			display_update();
			delay(1000);
			conectado = true;
			break;
		}
		printf(".");
		delay(1000); // Preguntar cada 1 segundo
	}


	if(conectado){
        printf("\r\n[ESP][inicializarESP] Termina inicializarESP Conexión WiFi verificada, listo para usar HTTP\r\n");
		return true; // Ahora s� estamos listos para usar HTTP
	} else {
		 printf("\r\n[ESP][inicializarESP] Timeout esperando IP. Revise su router.\r\n");
		 display_println("WiFi NO CONECTADO", 36);
		 display_println("REVISE ROUTER", 50);
		 display_update();
		 delay(100);
	}

	printf("\r\n[ESP][inicializarESP] Sin credenciales WiFi guardadas\r\n");
	display_println("WiFi NO CONECTADO", 36);
	display_println("SIN CREDENCIALES", 50);
	display_update();
	delay(100);
	printf("\r\n[ESP][inicializarESP] Habilitando BluFi para provisioning...\r\n");

	// 4) Configurar nombre de BluFi
	size_t n2 = enviarComandoAT("AT+BLUFINAME=\"cerradura\"", resp, sizeof(resp), 2000);
	if (n2 > 0){
		printf("\r\n[ESP][inicializarESP] BLUFINAME respuesta: %s\r\n", resp);
	}

	// 5) Habilitar BluFi
	size_t n3 = enviarComandoAT("AT+BLUFI=1", resp, sizeof(resp), 2000);
	if (n3 > 0){
		printf("\r\n[ESP][inicializarESP] BLUFI=1 respuesta: %s\r\n", resp);
		printf("\r\n[ESP][inicializarESP] Use la app ESP BluFi para configurar WiFi\r\n");
	}
    printf ("\r\n[ESP][inicializarESP] Fin de inicializarESP, BluFi habilitado\r\n");
	return false;

}

// Resetea las credenciales WiFi del ESP (llamar solo cuando usuario lo solicite)
bool resetearCredencialesESP(void){
    char resp[512];
    
    printf("\r\n[ESP][RESET] Borrando credenciales WiFi...\r\n");
    
    // AT+RESTORE borra todas las configuraciones (incluyendo WiFi)
    size_t n = enviarComandoAT("AT+RESTORE", resp, sizeof(resp), 5000);
    if (n > 0){
        printf("\r\n[ESP][RESET] RESTORE respuesta (%u bytes)\r\n", (unsigned)n);
        
        // Buscar "OK" seguido de CRLF (más robusto que solo OK)
        // La respuesta típica es: ...AT+RESTORE\r\n\r\nOK\r\n\r\nready\r\n
        bool foundOK = false;
        for(size_t i = 0; i < n - 3; i++){
            if(resp[i] == 'O' && resp[i+1] == 'K' && 
               (resp[i+2] == '\r' || resp[i+2] == '\n')){
                foundOK = true;
                break;
            }
        }
        
        if(foundOK){
            printf("\r\n[ESP][RESET] Credenciales borradas exitosamente\r\n");
            printf("\r\n[ESP][RESET] El ESP se reiniciará...\r\n");
            delay(3000); // Esperar a que el ESP se reinicie completamente
            
            // Re-inicializar comunicación AT
            printf("\r\n[ESP][RESET] Reinicializando comunicación...\r\n");
            delay(1000);
            inicializarESP(10000);
            
            return true;
        }
    }
    
    printf("\r\n[ESP][RESET] Error al borrar credenciales\r\n");
    return false;
}

// Funci�n auxiliar para buscar valores ignorando espacios
// Busca la clave (ej: "codigo") y extrae el valor entre comillas
static void buscarValorJSON(const char *fuente, const char *fin, const char *clave, char *destino, size_t maxLen) {
    char keyPatron[30];
    sprintf(keyPatron, "\"%s\"", clave); // Crear patr�n: "clave"

    const char *k = strstr(fuente, keyPatron);
    if (!k || k >= fin) return; // Clave no encontrada

    // Buscar los dos puntos ':' despu�s de la clave
    const char *dosPuntos = strchr(k + strlen(keyPatron), ':');
    if (!dosPuntos || dosPuntos >= fin) return;

    // Buscar la comilla de apertura del valor '"' despu�s de los dos puntos
    const char *inicioValor = strchr(dosPuntos, '\"');
    if (!inicioValor || inicioValor >= fin) return;

    inicioValor++; // Saltar la comilla de apertura

    // Copiar hasta la comilla de cierre
    size_t i = 0;
    while (inicioValor < fin && *inicioValor != '\"' && i < maxLen) {
        destino[i++] = *inicioValor++;
    }
    destino[i] = '\0';
}

static size_t parsearJSONPins(const char *json, size_t jsonLen, PinUsuario_t *pinsOut, size_t maxPins) {
    size_t count = 0;
    const char *ptr = json;
    const char *end = json + jsonLen;

    printf("\r\n[ESP][JSON] Parseando %u bytes (Modo Robusto)...\r\n", (unsigned)jsonLen);

    // 1. Buscar d�nde empieza el array "pins"
    // Buscamos solo "pins" para ser tolerantes a espacios antes de : o [
    const char *keyPins = strstr(ptr, "\"pins\"");
    if (!keyPins) {
        printf("[ERROR] No se encontr� la clave \"pins\"\r\n");
        return 0;
    }
    
    // Buscamos el corchete de apertura '[' despu�s de "pins"
    const char *arrayStart = strchr(keyPins, '[');
    if (!arrayStart || arrayStart >= end) {
        printf("[ERROR] No se encontr� el inicio del array '['\r\n");
        return 0;
    }
    
    ptr = arrayStart + 1;

    // 2. Recorrer objetos dentro del array
    while (ptr < end && count < maxPins) {
        // Buscar inicio de objeto '{'
        const char *objStart = strchr(ptr, '{');
        if (!objStart || objStart >= end) break; // No hay m�s objetos

        // Buscar fin de objeto '}'
        const char *objEnd = strchr(objStart, '}');
        if (!objEnd || objEnd >= end) break; // JSON malformado

        // --- EXTRACCI�N DE DATOS ---
        PinUsuario_t pin;
        memset(&pin, 0, sizeof(pin));
        
        // Usamos la funci�n auxiliar que ignora los espacios
        buscarValorJSON(objStart, objEnd, "codigo", pin.codigo, PIN_LENGTH);
        buscarValorJSON(objStart, objEnd, "rfid", pin.rfid, 19);
        buscarValorJSON(objStart, objEnd, "huella", pin.huella, 4);
        buscarValorJSON(objStart, objEnd, "tag", pin.tag, 19);

        // El campo "activo" es especial porque es booleano (true/false sin comillas)
        const char *pActivo = strstr(objStart, "\"activo\"");
        if (pActivo && pActivo < objEnd) {
            // Buscamos ':' y luego miramos si dice 'true'
            const char *dosPuntos = strchr(pActivo, ':');
            if (dosPuntos && dosPuntos < objEnd) {
                if (strstr(dosPuntos, "true") && strstr(dosPuntos, "true") < objEnd) {
                    pin.activo = true;
                } else {
                    pin.activo = false;
                }
            }
        }

        // Solo guardamos si tiene c�digo
        if (strlen(pin.codigo) > 0) {
            pinsOut[count] = pin;
            printf("  -> Usuario %d: %s (%s)\r\n", count+1, pin.codigo, pin.tag);
            count++;
        }

        // Avanzar puntero despu�s de este objeto
        ptr = objEnd + 1;
    }

    printf("[ESP][JSON] Total extraidos: %u\r\n", (unsigned)count);
    return count;
}

/* ---------------------------------------------------------------------------
   Obtiene usuarios desde servidor HTTP
--------------------------------------------------------------------------- */
size_t espHTTPGetPins(PinUsuario_t *pinsOut, size_t maxPins, uint32_t timeoutMs){
    if(!pinsOut || maxPins == 0) return 0;
    
    // Buffer grande para respuesta
    static char httpResp[2048];
    memset(httpResp, 0, sizeof(httpResp));
    
    printf("\r\n[ESP] Solicitando lista de usuarios (Modo TCP)...\r\n");
    
    if (!espRawHTTPGet("/syncPins/?allPins=true", httpResp, sizeof(httpResp))) {
        printf("\r\n[ESP][ERROR] Fall� la descarga TCP\r\n");
        return 0;
    }

    // Extraer JSON del body (buscar primer '{' y �ltimo '}')
    const char *jsonStart = NULL;
    const char *jsonEnd = NULL;
    size_t len = strlen(httpResp); // Calculamos longitud de lo recibido

    for(size_t i=0; i<len; i++){
        if(httpResp[i] == '{'){ jsonStart = &httpResp[i]; break; }
    }
    for(size_t i=len; i>0; i--){
        if(httpResp[i-1] == '}'){ jsonEnd = &httpResp[i-1]; break; }
    }
    
    if(!jsonStart || !jsonEnd || jsonEnd < jsonStart){
        printf("\r\n[ESP][ERROR] JSON no encontrado en la respuesta\r\n");
        // Debug: Imprimir lo que lleg� para ver si es un error HTML
        // printf("%s\n", httpResp);
        return 0;
    }
    
    size_t jsonLen = (size_t)(jsonEnd - jsonStart + 1);
    printf("\r\n[ESP] JSON v�lido encontrado: %u bytes\r\n", (unsigned)jsonLen);
    
    // Parsear JSON y llenar array
    return parsearJSONPins(jsonStart, jsonLen, pinsOut, maxPins);
}

size_t enviarComandoAT(const char *cmd, char *respuesta, size_t maxLen, uint32_t timeoutMs){
    if (!cmd || !respuesta || maxLen == 0){
        return 0;
    }

    // Enviar comando + CRLF
    printf("\r\n[ESP][TX] CMD: %s\r\n", cmd);
    uartWriteString(ESP_UART, cmd);
    uartWriteString(ESP_UART, "\r\n");

    // Leer respuesta hasta timeout o buffer lleno
    uint32_t start = tickRead();
    size_t pos = 0;
    uint8_t byte;
    uint32_t lastByteTime = start;
    // Para evitar esperar todo el timeout incluso cuando la respuesta ya llegó,
    // salimos si no llegan bytes nuevos durante `idleTimeoutMs` después de
    // haber recibido al menos un byte.
    const uint32_t idleTimeoutMs = 1000; // tiempo en ms sin recibir datos para considerar fin
    while ((tickRead() - start) < timeoutMs){
        if (uartReadByte(ESP_UART, &byte)){
            if (pos < (maxLen - 1)){
                respuesta[pos++] = (char)byte;
            }
            // Si el buffer está al límite, salimos conservando espacio para null-terminator
            if (pos >= (maxLen - 1)){
                break;
            }
            lastByteTime = tickRead();
        } else {
            // Si ya recibimos datos y llevamos un rato sin bytes nuevos, asumimos fin
            if (pos > 0 && (tickRead() - lastByteTime) > idleTimeoutMs) {
                break;
            }
            // Pequeña espera para no busy-loop
            delay(1);
        }
    }

    respuesta[pos] = '\0';
    printf("\r\n[ESP][RX] %u bytes recibidos\r\n", (unsigned)pos);
    
    // Debug hex dump primeros 64 bytes
    printf("[ESP][HEX] ");
    for(size_t i = 0; i < pos && i < 64; i++){
        printf("%02X ", (uint8_t)respuesta[i]);
        if((i+1) % 16 == 0) printf("\r\n[ESP][HEX] ");
    }
    printf("\r\n");
    
    // Debug ASCII filtrado
    printf("[ESP][ASCII] ");
    for(size_t i = 0; i < pos && i < 128; i++){
        char c = respuesta[i];
        if(c >= 32 && c <= 126){
            printf("%c", c);
        } else if(c == '\r'){
            printf("<CR>");
        } else if(c == '\n'){
            printf("<LF>");
        } else {
            printf("[%02X]", (uint8_t)c);
        }
    }
    printf("\r\n");
    
    return pos;

}

bool espEnviarNuevaHuella(const char *pin, const char *idHuella) {
    char path[200];
    char resp[512];

    // Construimos solo la parte del path (Ruta)
    sprintf(path, "/update-huella?pin=%s&huella=%s", pin, idHuella);

    if (espRawHTTPGet(path, resp, sizeof(resp))) {
        if (strstr(resp, "UPDATED OK")) return true;
    }
    return false;
}

bool espEnviarNuevoRFID(const char* pin, const char* rfidHex) {
    char ruta[200];
    char respuesta[256]; // Buffer para lo que responda el servidor

    // Construimos la URL. Ej: /api/asignar-rfid?pin=1234&rfid=A1B2C3D4
    // Asegurate que tu servidor espere estos par�metros
    sprintf(ruta, "/update-rfid?pin=%s&rfid=%s", pin, rfidHex);

    printf("\r\n[ESP] Subiendo RFID a la nube: PIN=%s UID=%s\r\n", pin, rfidHex);

    // Reutilizamos tu funci�n de GET (asumiendo que usas espRawHTTPGet o similar)
    // Si tu servidor requiere POST, habr�a que adaptar esta parte.
    if (espRawHTTPGet(ruta, respuesta, sizeof(respuesta))) {

        // Verificamos si el servidor respondi� OK (HTTP 200)
        // O si devuelve un JSON tipo {"status":"ok"}
        if (strstr(respuesta, "200 OK") != NULL || strstr(respuesta, "\"ok\"") != NULL) {
            printf("[ESP] Registro en nube EXITOSO.\r\n");
            return true;
        }
    }

    printf("[ESP] Error al subir datos al servidor.\r\n");
    return false;
}

// Avisar que borramos UNA huella
bool espBorrarHuellaServidor(const char *pin) {
    char url[256];
    sprintf(url, "http://%s:%s/delete-huella?pin=%s", SERVER_IP, SERVER_PORT, pin);

    char cmd[300], resp[128];
    snprintf(cmd, sizeof(cmd), "AT+HTTPCGET=\"%s\"", url);
    size_t len = enviarComandoAT(cmd, resp, sizeof(resp), 4000);

    return (len > 0 && strstr(resp, "DELETED OK"));
}

// Avisar que borramos TODAS
bool espBorrarTodasHuellasServidor(void) {
    char url[256];
    sprintf(url, "http://%s:%s/clear-all-huellas", SERVER_IP, SERVER_PORT);

    char cmd[300], resp[128];
    snprintf(cmd, sizeof(cmd), "AT+HTTPCGET=\"%s\"", url);
    size_t len = enviarComandoAT(cmd, resp, sizeof(resp), 4000);

    return (len > 0 && strstr(resp, "ALL CLEARED OK"));
}

double espObtenerTiempoServidor(void) {
    char resp[1024];

    // 1. Descargamos todo (Headers + Body)
    if (espRawHTTPGet("/check-status", resp, sizeof(resp))) {

        // 2. Buscamos d�nde empieza el JSON real (para ignorar headers)
        char *jsonBody = strchr(resp, '{');
        if (jsonBody == NULL) return -1.0; // No lleg� JSON

        // 3. Buscamos la etiqueta DENTRO del JSON
        char *ptr = strstr(jsonBody, "\"last_update\"");
        if(ptr) {
            ptr = strchr(ptr, ':');
            if(ptr) {
                ptr++; // Saltamos los dos puntos

                // atof es inteligente: lee n�meros y para cuando encuentra
                // una coma, un espacio o una llave de cierre '}'
                double val = atof(ptr);

                if (val > 1000000.0) return val; // Validaci�n m�nima de fecha coherente
            }
        }
    }
    return -1.0; // Error
}

// Funci�n gen�rica para hacer GET usando TCP puro (M�s compatible)
// Retorna: true si recibi� respuesta, false si fall�.
bool espRawHTTPGet(const char *path, char *bufferResp, int bufferSize) {
    char cmd[256];
    char aux[128];

    // 1. Asegurar limpieza antes de conectar
    enviarComandoAT("AT+CIPCLOSE", aux, sizeof(aux), 500); // Intentar cerrar por si acaso

    // 1. CONECTAR AL SERVIDOR
    printf("\r\n[ESP] Conectando TCP a %s:%s...\r\n", SERVER_IP, SERVER_PORT);
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%s", SERVER_IP, SERVER_PORT);

    // Intentamos conectar (Timeout 10s)
    if(enviarComandoAT(cmd, aux, sizeof(aux), 15000) == 0 || strstr(aux, "ERROR")) {
        printf("[ESP] Error al conectar TCP (Server caido o IP incorrecta)\r\n");
        printf("[AUX] %s\r\n", aux);
        return false;
    }

    // 2. ARMAR LA PETICI�N HTTP
    // Es vital poner \r\n al final de cada l�nea y una l�nea vac�a al final
    char request[300];
    sprintf(request, "GET %s HTTP/1.1\r\nHost: %s:%s\r\nConnection: close\r\n\r\n",
            path, SERVER_IP, SERVER_PORT);

    // 3. ENVIAR LA LONGITUD (AT+CIPSEND)
    sprintf(cmd, "AT+CIPSEND=%d", strlen(request));
    enviarComandoAT(cmd, aux, sizeof(aux), 2000);

    // Esperamos el prompt ">"
    if(!strstr(aux, ">")) {
        printf("[ESP] Error: No dio prompt de envio (>)\r\n");
        return false;
    }

    // 4. ENVIAR LOS DATOS REALES
    // Aqu� recibimos la respuesta del servidor
    memset(bufferResp, 0, bufferSize); // Limpiar buffer destino

    // Enviamos request y esperamos hasta 5 segundos la respuesta
    // Nota: enviarComandoAT devuelve la longitud recibida
    size_t len = enviarComandoAT(request, bufferResp, bufferSize, 5000);

    if(len > 0) {
        printf("[ESP] Respuesta recibida (%d bytes)\r\n", len);
        return true;
    }

    printf("[ESP] Error: Timeout esperando respuesta del servidor\r\n");
    return false;
}
