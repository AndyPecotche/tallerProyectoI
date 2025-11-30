#include "espAT.h"
#include "sapi.h"
#include <string.h>
//#include "sapi_peripheral_map.h"

/* UART del ESP en LPC4337: U3_TDX/U3_RTX -> indice 5 (UART_232 en sAPI) */
static const uartMap_t ESP_UART = (uartMap_t)5; // UART3 (RS232) indice 5

void espATInit(uint32_t baudrate){
    if(baudrate == 0){
        baudrate = 115200;
    }
    printf("\r\n[ESP] Init UART idx=%d baud=%u\r\n", (int)ESP_UART, (unsigned)baudrate);
    uartConfig(ESP_UART, baudrate);
}

// Chequeo básico: envía "AT" y espera "OK" en la respuesta.
bool espATCheck(uint32_t timeoutMs){    
    //Esperar aca hasta que reciba respuesta
    for (int i = 0; i < 3; i++) {
        printf("\r\n[ESP][CHECK] Enviando comando AT, intento %d...\r\n", i+1);
        char resp[64];
        size_t n = enviarComandoAT("AT", resp, sizeof(resp), timeoutMs);
        // Buscar "OK" (puede venir con CR/LF)
        if(n > 0){
            if(strstr(resp, "OK") != NULL){
                printf("\r\n[ESP][CHECK] OK detectado en respuesta\r\n");
                return true;
            } else {
                printf("\r\n[ESP][CHECK] Respuesta sin OK: '%s'\r\n", resp);
            }
        } else {
            printf("\r\n[ESP][CHECK] Sin bytes recibidos\r\n");
        }
        delay(1000); // Esperar antes del siguiente intento
    }
    return false;
}

/* ---------------------------------------------------------------------------
   Parseo simple JSON -> array PinUsuario_t
   Busca patrones "codigo":"XXXXX" y demás campos para llenar struct
--------------------------------------------------------------------------- */
static size_t parsearJSONPins(const char *json, size_t jsonLen, PinUsuario_t *pinsOut, size_t maxPins){
    size_t count = 0;
    const char *ptr = json;
    const char *end = json + jsonLen;
    
    printf("\r\n[ESP][JSON] Parseando %u bytes...\r\n", (unsigned)jsonLen);
    
    // Buscar inicio del array "pins":[ 
    const char *arrayStart = strstr(ptr, "\"pins\":[");
    if(!arrayStart || arrayStart >= end){
        printf("\r\n[ESP][JSON][ERROR] No se encuentra array 'pins'\r\n");
        return 0;
    }
    ptr = arrayStart + 8; // saltar "pins":[
    
    // Iterar objetos { ... }
    while(ptr < end && count < maxPins){
        // Buscar inicio de objeto
        const char *objStart = strchr(ptr, '{');
        if(!objStart || objStart >= end) break;
        const char *objEnd = strchr(objStart, '}');
        if(!objEnd || objEnd >= end) break;
        
        // Extraer campos del objeto
        PinUsuario_t pin;
        memset(&pin, 0, sizeof(pin));
        pin.activo = false;
        
        // "codigo":"XXXXX"
        const char *cPtr = strstr(objStart, "\"codigo\":\"");
        if(cPtr && cPtr < objEnd){
            cPtr += 10; // saltar "codigo":"
            size_t i = 0;
            while(cPtr < objEnd && *cPtr != '"' && i < PIN_LENGTH){
                pin.codigo[i++] = *cPtr++;
            }
            pin.codigo[i] = '\0';
        }
        
        // "activo":true|false
        const char *aPtr = strstr(objStart, "\"activo\":");
        if(aPtr && aPtr < objEnd){
            aPtr += 9;
            if(strncmp(aPtr, "true", 4) == 0) pin.activo = true;
        }
        
        // "rfid":"..."
        const char *rPtr = strstr(objStart, "\"rfid\":\"");
        if(rPtr && rPtr < objEnd){
            rPtr += 8;
            size_t i = 0;
            while(rPtr < objEnd && *rPtr != '"' && i < 19){
                pin.rfid[i++] = *rPtr++;
            }
            pin.rfid[i] = '\0';
        }
        
        // "huella":"..."
        const char *hPtr = strstr(objStart, "\"huella\":\"");
        if(hPtr && hPtr < objEnd){
            hPtr += 10;
            size_t i = 0;
            while(hPtr < objEnd && *hPtr != '"' && i < 3){
                pin.huella[i++] = *hPtr++;
            }
            pin.huella[i] = '\0';
        }
        
        // "tag":"..."
        const char *tPtr = strstr(objStart, "\"tag\":\"");
        if(tPtr && tPtr < objEnd){
            tPtr += 7;
            size_t i = 0;
            while(tPtr < objEnd && *tPtr != '"' && i < 19){
                pin.tag[i++] = *tPtr++;
            }
            pin.tag[i] = '\0';
        }
        
        // Almacenar si tiene código válido
        if(strlen(pin.codigo) > 0){
            pinsOut[count++] = pin;
            printf("\r\n[ESP][JSON] Pin[%u]: cod=%s act=%d rfid=%s huella=%s tag=%s\r\n",
                   (unsigned)(count-1), pin.codigo, pin.activo, pin.rfid, pin.huella, pin.tag);
        }
        
        // Avanzar al siguiente objeto
        ptr = objEnd + 1;
    }
    
    printf("\r\n[ESP][JSON] Total parseado: %u usuarios\r\n", (unsigned)count);
    return count;
}

/* ---------------------------------------------------------------------------
   Obtiene usuarios desde servidor HTTP
--------------------------------------------------------------------------- */
size_t espHTTPGetPins(const char *url, PinUsuario_t *pinsOut, size_t maxPins, uint32_t timeoutMs){
    if(!url || !pinsOut || maxPins == 0) return 0;
    
    // Buffer para respuesta HTTP
    static char httpResp[2048];
    memset(httpResp, 0, sizeof(httpResp));
    
    // Construir comando AT+HTTPCGET
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "AT+HTTPCGET=\"%s\"", url);
    
    printf("\r\n[ESP][HTTP] GET %s\r\n", url);
    size_t len = enviarComandoAT(cmd, httpResp, sizeof(httpResp), timeoutMs);
    
    if(len == 0){
        printf("\r\n[ESP][HTTP][ERROR] Sin respuesta\r\n");
        return 0;
    }
    
    // Extraer JSON del body (buscar primer '{' y último '}')
    const char *jsonStart = NULL;
    const char *jsonEnd = NULL;
    for(size_t i=0; i<len; i++){
        if(httpResp[i] == '{'){ jsonStart = &httpResp[i]; break; }
    }
    for(size_t i=len; i>0; i--){
        if(httpResp[i-1] == '}'){ jsonEnd = &httpResp[i-1]; break; }
    }
    
    if(!jsonStart || !jsonEnd || jsonEnd < jsonStart){
        printf("\r\n[ESP][HTTP][ERROR] JSON no encontrado o incompleto\r\n");
        return 0;
    }
    
    size_t jsonLen = (size_t)(jsonEnd - jsonStart + 1);
    printf("\r\n[ESP][HTTP] JSON body: %u bytes\r\n", (unsigned)jsonLen);
    
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
        }
        // Nota: evitamos salida anticipada; dejamos que el timeout global gobierne
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
