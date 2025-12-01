#include "as608.h"
#include "sapi.h"
#include <string.h>
#include <stdio.h>

// Selección del UART para el AS608 en placa edu-ciaa_nxp (LPC4337)

#ifndef AS608_UART
    #define AS608_UART ((uartMap_t)0)
#endif

/* Baudrate por defecto del AS608 */
#define AS608_DEFAULT_BAUD 57600

/* Parametrización de tiempos y delays (ms)
 * Breve descripción de cada constante:
 * - AS608_FLUSH_MS_SHORT: tiempo para vaciar/descartar bytes pendientes en RX antes de enviar.
 * - AS608_FLUSH_MS_TINY: versión más corta del flush para comandos rápidos.
 * - AS608_DELAY_BEFORE_SEND_MS: pequeña espera antes de transmitir un paquete (estabiliza UART/sensor).
 * - AS608_GETIMAGE_TIMEOUT_MS: tiempo máximo esperando el ACK del comando GetImage.
 * - AS608_GETIMAGE_LONG_TIMEOUT_MS: variante de timeout más holgada para capturas durante ENROLL/polling.
 * - AS608_IMAGE2TZ_PRE_DELAY_MS: espera previa a convertir imagen a características (Image2Tz).
 * - AS608_IMAGE2TZ_TIMEOUT_MS: tiempo máximo esperando el ACK de Image2Tz.
 * - AS608_SEARCH_TIMEOUT_MS: tiempo máximo esperando respuesta de HiSpeedSearch.
 * - AS608_PROBE_TIMEOUT_MS: tiempo de espera al sondear baudios con VfyPwd.
 * - AS608_CHECK_DELAY_MS: pequeña espera tras VfyPwd antes de leer respuesta en as608Check.
 * - AS608_CHECK_TIMEOUT_MS: tiempo máximo esperando el ACK de VfyPwd en as608Check.
 * - AS608_TEMPLATECOUNT_TIMEOUT_MS: tiempo máximo esperando respuesta de TemplateCount.
 * - AS608_REGMODEL_TIMEOUT_MS: tiempo máximo esperando respuesta de RegModel.
 * - AS608_STORE_TIMEOUT_MS: tiempo máximo esperando respuesta de Store.
 * - AS608_ENROLL_STEP_DELAY_MS: pausa entre captura y conversión durante ENROLL/poll.
 * - AS608_ENROLL_NOFINGER_DELAY_MS: pausa cuando no hay dedo detectado, para evitar saturar.
 * - AS608_ENROLL_ERROR_DELAY_MS: pausa ante errores de captura para dar estabilidad.
 */
#define AS608_FLUSH_MS_SHORT           5
#define AS608_FLUSH_MS_TINY            2
#define AS608_DELAY_BEFORE_SEND_MS     5
#define AS608_GETIMAGE_LONG_TIMEOUT_MS 300
#define AS608_IMAGE2TZ_PRE_DELAY_MS    80
#define AS608_IMAGE2TZ_TIMEOUT_MS      600
#define AS608_SEARCH_TIMEOUT_MS        250
#define AS608_CHECK_DELAY_MS           5
#define AS608_CHECK_TIMEOUT_MS         3000
#define AS608_TEMPLATECOUNT_TIMEOUT_MS 3000
#define AS608_REGMODEL_TIMEOUT_MS      300
#define AS608_STORE_TIMEOUT_MS         500
#define AS608_ENROLL_STEP_DELAY_MS     100
#define AS608_ENROLL_NOFINGER_DELAY_MS 120
#define AS608_ENROLL_ERROR_DELAY_MS    150

/* Intervalo mínimo entre intentos de captura durante ENROLL (ms) */
#define AS608_ENROLL_ATTEMPT_INTERVAL_MS 2000

/* Nivel de debug runtime (configurable) desde llamado a funcion*/
static uint8_t g_as608DebugLevel;
void as608SetDebug(uint8_t level){ g_as608DebugLevel = level; }

/* Estadísticas de respuesta eliminadas para simplificar el módulo */

static bool as608Send(const uint8_t *data, size_t len){
    for(size_t i=0; i<len; i++){
        uartWriteByte(AS608_UART, data[i]);
    }
    return true;
}
static void as608FlushRx(uint32_t ms){
    uint32_t start = tickRead();
    uint8_t b;
    while ((tickRead() - start) < ms){
        if (!uartReadByte(AS608_UART, &b)){
            // No data right now, small wait
            delay(1);
        }
    }
}
static size_t as608Recv(uint8_t *buf, size_t maxLen, uint32_t timeoutMs){
    uint32_t start = tickRead();
    size_t n = 0;
    uint8_t b;
    while ((tickRead() - start) < timeoutMs){
        if (uartReadByte(AS608_UART, &b)){
            if (n < maxLen) buf[n++] = b;
            else break;
        }
    }
    return n;
}
/*
 * as608Check: verifica comunicación con el sensor (función interna)
 * Envía comando VfyPwd (0x13) con contraseña por defecto 0x00000000
 */
static bool as608Check(void){
    printf("\r\n[AS608][INIT] Verificando comunicación con sensor...\r\n");
    const uint8_t cmdVfyPwd[] = {
        0xEF, 0x01,                     // Header
        0xFF, 0xFF, 0xFF, 0xFF,         // Address
        0x01,                           // Package identifier
        0x00, 0x07,                     // Length
        0x13,                           // Instruction VfyPwd
        0x00, 0x00, 0x00, 0x00,         // Password
        0x00, 0x1B                      // Checksum
    };
    as608FlushRx(AS608_FLUSH_MS_TINY);
    as608Send(cmdVfyPwd, sizeof(cmdVfyPwd));
    delay(AS608_CHECK_DELAY_MS);
    uint8_t resp[32];
    size_t n = as608Recv(resp, sizeof(resp), AS608_CHECK_TIMEOUT_MS);
    
    if(n < 12 || resp[0] != 0xEF || resp[1] != 0x01 || resp[6] != 0x07){
        printf("[AS608][CHECK] Respuesta inválida\r\n");
        return false;
    }
    if(resp[9] == 0x00){
        printf("[AS608][CHECK] Sensor responde OK\r\n");
        return true;
    }
    printf("[AS608][CHECK] Código de error: 0x%02X\r\n", resp[9]);
    return false;
}

/* Utilidad: construir y enviar paquete según protocolo AS608 */
static void as608WritePacket(uint8_t packetType, const uint8_t *payload, uint16_t payloadLen){
    uint8_t header[9];
    header[0] = 0xEF; header[1] = 0x01; // start
    header[2] = 0xFF; header[3] = 0xFF; header[4] = 0xFF; header[5] = 0xFF; // address
    header[6] = packetType; // type
    uint16_t len = payloadLen + 2; // length includes checksum
    header[7] = (uint8_t)(len >> 8);
    header[8] = (uint8_t)(len & 0xFF);

    // checksum = type + len bytes + payload bytes
    uint16_t sum = packetType + header[7] + header[8];
    for(uint16_t i=0;i<payloadLen;i++) sum += payload[i];

    // Send header, payload, checksum
    // Build single buffer and send once for robustness
    uint8_t frame[9 + 256 + 2];
    size_t idx = 0;
    memcpy(&frame[idx], header, sizeof(header)); idx += sizeof(header);
    if(payloadLen){ memcpy(&frame[idx], payload, payloadLen); idx += payloadLen; }
    frame[idx++] = (uint8_t)(sum >> 8);
    frame[idx++] = (uint8_t)(sum & 0xFF);
    as608Send(frame, idx);
}

/* Espera ACK (tipo 0x07) y devuelve confirm code, o 0xFF si timeout/paquete inválido */
static uint8_t as608GetAck(uint8_t *respBuf, size_t respMax, uint32_t timeoutMs){
    size_t n = as608Recv(respBuf, respMax, timeoutMs);
    if(n < 12) return 0xFF;
    if(respBuf[0]!=0xEF || respBuf[1]!=0x01) return 0xFF;
    if(respBuf[6]!=0x07) return 0xFF;
    return respBuf[9];
}

/* Helper genérico para comandos simples que retornan solo código de confirmación (ACK) */
static uint8_t as608CmdAck(uint8_t instr, const uint8_t *extra, uint16_t extraLen,
                           uint32_t timeoutMs, uint32_t preDelayMs, uint32_t flushMs){
    uint8_t payload[8];
    uint16_t len = 0;
    payload[len++] = instr;
    if(extra && extraLen){
        if(extraLen > sizeof(payload)-1) extraLen = sizeof(payload)-1;
        memcpy(&payload[len], extra, extraLen);
        len += extraLen;
    }
    if(preDelayMs) delay(preDelayMs);
    as608FlushRx(flushMs);
    as608WritePacket(0x01, payload, len);
    uint8_t resp[32];
    return as608GetAck(resp, sizeof(resp), timeoutMs);
}

static uint8_t as608CmdGetImageLong(void){
    return as608CmdAck(0x01, NULL, 0,
                       AS608_GETIMAGE_LONG_TIMEOUT_MS,
                       AS608_DELAY_BEFORE_SEND_MS,
                       AS608_FLUSH_MS_SHORT);
}

static uint8_t as608CmdImage2Tz(uint8_t slot /*1 o 2*/){
    uint8_t extra = slot;
    return as608CmdAck(0x02, &extra, 1,
                       AS608_IMAGE2TZ_TIMEOUT_MS,
                       AS608_IMAGE2TZ_PRE_DELAY_MS,
                       AS608_FLUSH_MS_SHORT);
}

/* HiSpeedSearch: busca desde startPage por pageCount, devuelve id & score via punteros */
static int as608CmdHiSpeedSearch(uint16_t startPage, uint16_t pageCount, uint16_t *outId, uint16_t *outScore){
    uint8_t payload[] = { 0x1B, 0x01, (uint8_t)(startPage>>8), (uint8_t)(startPage&0xFF), (uint8_t)(pageCount>>8), (uint8_t)(pageCount&0xFF) };
    as608FlushRx(2);
    as608WritePacket(0x01, payload, sizeof(payload));
    uint8_t resp[32];
    size_t n = as608Recv(resp, sizeof(resp), AS608_SEARCH_TIMEOUT_MS);
    if(n < 14) return -1;
    if(resp[0]!=0xEF || resp[1]!=0x01) return -1;
    if(resp[6]!=0x07) return -1;
    // confirm code in resp[9]
    if(resp[9] != 0x00) return resp[9];
    // After confirm OK, next bytes: page ID (2), match score (2)
    uint16_t id = ((uint16_t)resp[10]<<8) | resp[11];
    uint16_t score = ((uint16_t)resp[12]<<8) | resp[13];
    if(outId) *outId = id;
    if(outScore) *outScore = score;
    return 0;
}

bool as608PollHuella(uint16_t *idOut, uint16_t *scoreOut){
    // Usar versión "long" para mayor robustez
    uint8_t r = as608CmdGetImageLong();
    if (r != 0x00){
        if(g_as608DebugLevel >= 1 && r != 0x02){
            printf("[AS608][DBG] GetImage code=0x%02X\r\n", r);
        }
        return false; // sin dedo o error
    }
    if(g_as608DebugLevel >= 1){
        printf("[AS608][DBG] Dedo detectado (GetImage OK)\r\n");
    }
    // Pequeña espera adicional antes de convertir
    delay(AS608_ENROLL_STEP_DELAY_MS);
    uint8_t tz = as608CmdImage2Tz(0x01);
    if (tz != 0x00){
        if(g_as608DebugLevel >= 1){
            printf("[AS608][DBG] Image2Tz fallo code=0x%02X\r\n", tz);
        }
        return false; // conversión falló silenciosamente
    }
    uint16_t id=0, score=0;
    int sr = as608CmdHiSpeedSearch(0x0000, 0x00A3, &id, &score);
    if (sr == 0){
        if(idOut) *idOut = id;
        if(scoreOut) *scoreOut = score;
        if(g_as608DebugLevel >= 1){
            printf("[AS608][DBG] Coincidencia ID=%u score=%u\r\n", (unsigned)id, (unsigned)score);
        }
        return true; // éxito; el mensaje se mostrará fuera (MEF/validación)
    }
    if(g_as608DebugLevel >= 1){
        printf("[AS608][DBG] Búsqueda sin coincidencia (sr=0x%02X)\r\n", (unsigned)sr);
    }
    return false;
}

/* Comando RegModel (0x05) para combinar CharBuffer1 y CharBuffer2 en un template */
static uint8_t as608CmdRegModel(void){
    return as608CmdAck(0x05, NULL, 0,
                       AS608_REGMODEL_TIMEOUT_MS,
                       0,
                       AS608_FLUSH_MS_TINY);
}

/* Comando Store (0x06) buffer=0x01, ID destino */
static uint8_t as608CmdStore(uint16_t id){
    uint8_t extra[3] = { 0x01, (uint8_t)(id>>8), (uint8_t)(id&0xFF) };
    return as608CmdAck(0x06, extra, sizeof(extra),
                       AS608_STORE_TIMEOUT_MS,
                       0,
                       AS608_FLUSH_MS_TINY);
}

/* Comando TemplateCount (0x1D): devuelve cantidad de templates almacenados */
static int as608CmdTemplateCount(void){
    uint8_t payload[] = { 0x1D };
    as608FlushRx(AS608_FLUSH_MS_TINY);
    as608WritePacket(0x01, payload, sizeof(payload));
    uint8_t resp[32];
    size_t n = as608Recv(resp, sizeof(resp), AS608_TEMPLATECOUNT_TIMEOUT_MS);
    if(n < 14) return -1;
    if(resp[0]!=0xEF || resp[1]!=0x01) return -1;
    if(resp[6]!=0x07) return -1;
    if(resp[9] != 0x00) return -1; // confirm code
    // count bytes after confirm: high, low
    int count = ((int)resp[10]<<8) | resp[11];
    return count;
}

/* Helper: ciclo de captura y conversión Image2Tz para ENROLL */
static bool as608EnrollCapture(uint8_t slot, int pasoIndex){
    uint32_t start = tickRead();
    uint32_t lastAttempt = 0;
    int errorCount = 0;
    int tries = 0;
    while (((tickRead() - start) < 10000) || (tries < 5)){
        if((tickRead() - lastAttempt) < AS608_ENROLL_ATTEMPT_INTERVAL_MS){
            delay(10);
            continue;
        }
        lastAttempt = tickRead();
        uint8_t r = as608CmdGetImageLong();
        tries++;
        if(r == 0x00){
            printf("[AS608][ENROLL] Imagen %d capturada\r\n", pasoIndex);
            delay(AS608_ENROLL_STEP_DELAY_MS);
            uint8_t tz = as608CmdImage2Tz(slot);
            if(tz != 0x00){
                printf("[AS608][ENROLL] Error Image2Tz(%d) code=0x%02X\r\n", pasoIndex, tz);
                return false;
            }
            return true;
        } else if (r == 0x02){
            // Sin dedo, esperar un poco más para no saturar
            delay(AS608_ENROLL_NOFINGER_DELAY_MS);
        } else {
            errorCount++;
            if(errorCount % 5 == 0){
                printf("[AS608][ENROLL] GetImage errores acumulados=%d (último=0x%02X)\r\n", errorCount, r);
            }
            delay(AS608_ENROLL_ERROR_DELAY_MS);
        }
    }
    if(((tickRead() - start) >= 10000) && tries < 1){
        printf("[AS608][ENROLL] Timeout esperando dedo (imagen %d)\r\n", pasoIndex);
        return false;
    }
    return false;
}

bool as608Enroll(char idOut[5]){
    if(!idOut) return false;
    memset(idOut,0,5);
    printf("\r\n[AS608][ENROLL] Inicio de registro de huella\r\n");
    printf("[AS608][ENROLL] Paso 1: Coloque dedo (imágen 1)\r\n");
    // Espera adicional solicitada antes de iniciar escaneo
    delay(5000);
    if(!as608EnrollCapture(0x01, 1)) return false;

    printf("[AS608][ENROLL] Retire dedo...\r\n");
    delay(1500);
    printf("[AS608][ENROLL] Paso 2: Coloque nuevamente el mismo dedo (imágen 2)\r\n");
    delay(5000); // Espera adicional antes de segundo escaneo
    if(!as608EnrollCapture(0x02, 2)) return false;

    uint8_t rm = as608CmdRegModel();
    if(rm != 0x00){
        printf("[AS608][ENROLL] Error RegModel code=0x%02X\r\n", rm);
        return false;
    }
    printf("[AS608][ENROLL] Modelo creado\r\n");

    int count = as608CmdTemplateCount();
    if(count < 0){
        printf("[AS608][ENROLL] Error leyendo cantidad de templates\r\n");
        return false;
    }
    uint16_t newId = (uint16_t)count; // usar siguiente índice libre (asumiendo contiguos)
    if(newId > 0x00A3){
        printf("[AS608][ENROLL] Sin espacio para nueva huella (ID=%u)\r\n", (unsigned)newId);
        return false;
    }

    uint8_t st = as608CmdStore(newId);
    if(st != 0x00){
        printf("[AS608][ENROLL] Error Store ID=%u code=0x%02X\r\n", (unsigned)newId, st);
        return false;
    }
    printf("[AS608][ENROLL] Huella almacenada ID=%u\r\n", (unsigned)newId);
    // Formatear a 4 dígitos
    snprintf((char*)idOut, 5, "%04u", (unsigned)newId);
    return true;
}

void as608Init(uint32_t baudrate){
    if(baudrate == 0) baudrate = AS608_DEFAULT_BAUD;
    uartConfig(AS608_UART, baudrate);
    printf("\r\n[AS608] UART inicializado en %u baud\r\n", (unsigned)baudrate);
    printf("[AS608] Mapeado: sAPI_index=%d (UART_GPIO=0->GPIO1/GPIO2, UART_485=1, UART_USB=3, UART_232=5)\r\n", (int)AS608_UART);
    // Limpiar cualquier basura previa del buffer RX
    as608FlushRx(10);
    bool as608Ok = as608Check();
    printf("\r\n[AS608][INIT] Estado sensor: %s\r\n", as608Ok?"OK":"FALLÓ");
}
