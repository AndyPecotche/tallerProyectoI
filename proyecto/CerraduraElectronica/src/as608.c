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
 * - AS608_IMAGE2TZ_PRE_DELAY_MS: espera previa a convertir imagen a características (Image2Tz).
 * - AS608_IMAGE2TZ_TIMEOUT_MS: tiempo máximo esperando el ACK de Image2Tz.
 * - AS608_SEARCH_TIMEOUT_MS: tiempo máximo esperando respuesta de HiSpeedSearch.
 * - AS608_CHECK_DELAY_MS: pequeña espera tras VfyPwd antes de leer respuesta en as608Check.
 * - AS608_CHECK_TIMEOUT_MS: tiempo máximo esperando el ACK de VfyPwd en as608Check.
 * - AS608_TEMPLATECOUNT_TIMEOUT_MS: tiempo máximo esperando respuesta de TemplateCount.
 * - AS608_REGMODEL_TIMEOUT_MS: tiempo máximo esperando respuesta de RegModel.
 * - AS608_STORE_TIMEOUT_MS: tiempo máximo esperando respuesta de Store.
 * - AS608_ENROLL_STEP_DELAY_MS: pausa entre captura y conversión durante ENROLL/poll.
 * - AS608_ENROLL_NOFINGER_DELAY_MS: pausa cuando no hay dedo detectado, para evitar saturar.
 * - AS608_ENROLL_ERROR_DELAY_MS: pausa ante errores de captura para dar estabilidad.
 */
#define AS608_FLUSH_MS_SHORT           2
#define AS608_FLUSH_MS_TINY            1
#define AS608_DELAY_BEFORE_SEND_MS     2
#define AS608_GETIMAGE_LONG_TIMEOUT_MS 200  // Timeout suficiente para respuesta del sensor
#define AS608_IMAGE2TZ_PRE_DELAY_MS    10   // Delay mínimo antes de conversión
#define AS608_IMAGE2TZ_TIMEOUT_MS      400  // Timeout suficiente para Image2Tz
#define AS608_SEARCH_TIMEOUT_MS        200  // Timeout suficiente para búsqueda
#define AS608_CHECK_DELAY_MS           5
#define AS608_CHECK_TIMEOUT_MS         3000
#define AS608_TEMPLATECOUNT_TIMEOUT_MS 3000
#define AS608_REGMODEL_TIMEOUT_MS      300
#define AS608_STORE_TIMEOUT_MS         500
#define AS608_ENROLL_STEP_DELAY_MS     100
#define AS608_ENROLL_NOFINGER_DELAY_MS 200
#define AS608_ENROLL_ERROR_DELAY_MS    300

/* Intervalo mínimo entre intentos de captura durante ENROLL (ms) */
#define AS608_ENROLL_ATTEMPT_INTERVAL_MS 300

/* Nivel de debug runtime (configurable) desde llamado a funcion*/
static uint8_t g_as608DebugLevel;
void as608SetDebug(uint8_t level){ g_as608DebugLevel = level; }

static bool as608Send(const uint8_t *data, size_t len){
    for(size_t i=0; i<len; i++){
        uartWriteByte(AS608_UART, data[i]);
    }
    return true;
}
static void as608FlushRx(uint32_t ms){
    uint32_t start = tickRead();
    uint8_t b;
    // Limpiar buffer durante todo el tiempo especificado
    while ((tickRead() - start) < ms){
        uartReadByte(AS608_UART, &b); // Descartar bytes si hay
    }
}
/* Helper pequeño para mejorar legibilidad: flush seguido de delay */
static void as608FlushAndDelay(uint32_t flushMs, uint32_t delayMs){
    as608FlushRx(flushMs);
    if(delayMs) delay(delayMs);
}
static size_t as608Recv(uint8_t *buf, size_t maxLen, uint32_t timeoutMs){
    uint32_t start = tickRead();
    size_t n = 0;
    uint8_t b;
    uint32_t lastByte = start;
    while ((tickRead() - start) < timeoutMs){
        if (uartReadByte(AS608_UART, &b)){
            if (n < maxLen) buf[n++] = b;
            else break;
            lastByte = tickRead();
        } else {
            // Si ya recibimos bytes y llevamos 30ms sin nuevos datos, asumir fin
            if(n > 0 && (tickRead() - lastByte) > 30){
                break;
            }
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

/* Nota: Se elimina lógica de búsqueda de IDs libres.
 * El flujo actual exige que el ID de huella provenga del registro de usuarios. */

/* Comando Empty (0x0D): borra todos los templates almacenados */
static uint8_t as608CmdEmpty(void){
    return as608CmdAck(0x0D, NULL, 0,
                       2000, // timeout ms
                       0,
                       AS608_FLUSH_MS_SHORT);
}

bool as608ClearAllTemplates(void){
    printf("\r\n[AS608] Borrando todas las huellas (Empty) ...\r\n");
    uint8_t r = as608CmdEmpty();
    if(r == 0x00){
        printf("[AS608] Templates borrados correctamente\r\n");
        return true;
    }
    printf("[AS608] Error al borrar templates, code=0x%02X\r\n", r);
    return false;
}

/* Helper: ciclo de captura y conversión Image2Tz para ENROLL */
static bool as608EnrollCapture(uint8_t slot, int pasoIndex){
    // Flush agresivo inicial para limpiar cualquier estado residual
    as608FlushAndDelay(50, 100);

    uint32_t start = tickRead();
    uint32_t lastAttempt = 0;
    int errorCount = 0;
    int tries = 0;
    const uint32_t TIMEOUT_MS = 30000; // 30 segundos timeout

    while ((tickRead() - start) < TIMEOUT_MS){
        if((tickRead() - lastAttempt) < AS608_ENROLL_ATTEMPT_INTERVAL_MS){
            delay(10);
            continue;
        }
        lastAttempt = tickRead();

        // Flush antes de cada intento para evitar datos viejos
        as608FlushAndDelay(10, 50);

        uint8_t r = as608CmdGetImageLong();
        tries++;

        if(r == 0x00){
            printf("[AS608][ENROLL] Imagen %d capturada (intento %d)\r\n", pasoIndex, tries);
            // Flush y delay más largos antes de conversión
            as608FlushAndDelay(20, 200);
            uint8_t tz = as608CmdImage2Tz(slot);
            if(tz != 0x00){
                printf("[AS608][ENROLL] Error Image2Tz(%d) code=0x%02X\r\n", pasoIndex, tz);
                return false;
            }
            return true;
        } else if (r == 0x02){
            // Sin dedo, esperar un poco más para no saturar
            if(tries % 10 == 0){
                printf("[AS608][ENROLL] Esperando dedo (imagen %d)...\r\n", pasoIndex);
            }
            delay(AS608_ENROLL_NOFINGER_DELAY_MS);
        } else {
            errorCount++;
            if(errorCount % 10 == 0){
                printf("[AS608][ENROLL] GetImage errores=%d (último=0x%02X)\r\n", errorCount, r);
            }
            delay(AS608_ENROLL_ERROR_DELAY_MS);
        }
    }

    printf("[AS608][ENROLL] Timeout esperando dedo (imagen %d) después de %d intentos\r\n", pasoIndex, tries);
    return false;
}

/* Enroll almacenando en un ID específico (sobrescribe si existe) */
bool as608EnrollAtId(uint16_t idTarget, char idOut[5]){
    if(!idOut) return false;
    memset(idOut,0,5);
    
    // Preparación del sensor
    printf("\r\n[AS608][ENROLL@ID] Preparando sensor...\r\n");
    as608FlushAndDelay(100, 200);
    
    printf("[AS608][ENROLL@ID] Paso 1: Coloque dedo (imágen 1)\r\n");
    delay(2000);
    if(!as608EnrollCapture(0x01, 1)) return false;

    printf("[AS608][ENROLL@ID] Retire dedo...\r\n");
    delay(3000);
    as608FlushRx(50);
    printf("[AS608][ENROLL@ID] Paso 2: Coloque nuevamente el mismo dedo (imágen 2)\r\n");
    delay(2000);
    if(!as608EnrollCapture(0x02, 2)) return false;

    uint8_t rm = as608CmdRegModel();
    if(rm != 0x00){
        printf("[AS608][ENROLL@ID] Error RegModel code=0x%02X\r\n", rm);
        return false;
    }
    printf("[AS608][ENROLL@ID] Modelo creado\r\n");

    if(idTarget > 0x00A3){
        printf("[AS608][ENROLL@ID] ID fuera de rango: %u\r\n", (unsigned)idTarget);
        return false;
    }

    uint8_t st = as608CmdStore(idTarget);
    if(st != 0x00){
        printf("[AS608][ENROLL@ID] Error Store ID=%u code=0x%02X\r\n", (unsigned)idTarget, st);
        return false;
    }
    printf("[AS608][ENROLL@ID] Huella almacenada ID=%u\r\n", (unsigned)idTarget);
    snprintf((char*)idOut, 5, "%04u", (unsigned)idTarget);
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
    int tc = -1;
    tc = as608CmdTemplateCount();
    if (tc >= 0){
        printf("[AS608][INIT] Templates almacenados: %d\r\n", tc);
    }
}

/* ---------------------------------------------------------------
 * Mini MEF de escaneo no bloqueante
 * --------------------------------------------------------------- */
typedef enum {
    ST_IDLE = 0,
    ST_SEND_GETIMAGE,
    ST_WAIT_GETIMAGE,
    ST_PREDLY_IMAGE2TZ,
    ST_SEND_IMAGE2TZ,
    ST_WAIT_IMAGE2TZ,
    ST_SEND_SEARCH,
    ST_WAIT_SEARCH,
    ST_DONE_MATCH,
    ST_DONE_NOMATCH,
    ST_DONE_ERROR
} as608ScanState_t;

static struct {
    as608ScanState_t st;
    uint32_t tCmdStart;
    uint32_t tLastByte;
    uint8_t resp[64];
    size_t respLen;
    uint16_t lastId;
    uint16_t lastScore;
} g_scan;

void as608ScanReset(void){
    memset(&g_scan, 0, sizeof(g_scan));
    g_scan.st = ST_SEND_GETIMAGE; // listo para comenzar en la primera llamada
}

/* Lee algunos bytes por un pequeño slice de tiempo (2-4ms) */
static void as608ReadSlice(uint8_t *buf, size_t *len, size_t maxLen, uint32_t sliceMs){
    uint32_t start = tickRead();
    uint8_t b;
    while ((tickRead() - start) < sliceMs){
        if (uartReadByte(AS608_UART, &b)){
            if (*len < maxLen){
                buf[(*len)++] = b;
                g_scan.tLastByte = tickRead();
            } else {
                break;
            }
        }
    }
}

/* Intenta extraer el confirm code del ACK acumulado; retorna 0xFF si aún incompleto/incorrecto */
static uint8_t as608TryParseAck(const uint8_t *buf, size_t len){
    if (len < 12) return 0xFF;
    if (buf[0] != 0xEF || buf[1] != 0x01) return 0xFF;
    if (buf[6] != 0x07) return 0xFF;
    return buf[9];
}

/* Tamaño total esperado del frame a partir del header (incluye checksum). 0 si aún no se puede calcular. */
static uint16_t as608ExpectedFrameLen(const uint8_t *buf, size_t len){
    if (len < 9) return 0;
    if (buf[0] != 0xEF || buf[1] != 0x01) return 0;
    uint16_t l = ((uint16_t)buf[7] << 8) | buf[8];
    // total = 9 bytes de header + l (payload + checksum)
    return 9 + l;
}

static void as608DbgDump(const char *tag, const uint8_t *buf, size_t len){
    if (g_as608DebugLevel < 2) return;
    printf("[AS608][DBG2] %s (%u bytes): ", tag, (unsigned)len);
    size_t n = len > 32 ? 32 : len;
    for(size_t i=0;i<n;i++) printf("%02X ", buf[i]);
    if (len > n) printf("...");
    printf("\r\n");
}

/* Avanza la mini MEF en pasos cortos */
as608ScanStatus_t as608ScanStep(uint16_t *idOut, uint16_t *scoreOut){
    // Duración máxima por paso de espera de respuesta (no bloqueante)
    const uint32_t SLICE_MS = 4;

    switch(g_scan.st){
        case ST_IDLE:
            // Si está idle, preparar nuevo intento
            g_scan.st = ST_SEND_GETIMAGE;
            // continuar flujo
        case ST_SEND_GETIMAGE: {
            g_scan.respLen = 0;
            as608FlushRx(AS608_FLUSH_MS_SHORT);
            // Enviar comando GetImage
            uint8_t payload = 0x01;
            as608WritePacket(0x01, &payload, 1);
            g_scan.tCmdStart = tickRead();
            if (g_as608DebugLevel >= 2) printf("[AS608][DBG2] -> GetImage\r\n");
            g_scan.st = ST_WAIT_GETIMAGE;
            return AS608_SCAN_INPROGRESS;
        }
        case ST_WAIT_GETIMAGE: {
            as608ReadSlice(g_scan.resp, &g_scan.respLen, sizeof(g_scan.resp), SLICE_MS);
            uint8_t code = as608TryParseAck(g_scan.resp, g_scan.respLen);
            if (code != 0xFF){
                if (g_as608DebugLevel >= 2){
                    printf("[AS608][DBG2] GetImage ACK=0x%02X len=%u exp=%u\r\n", code, (unsigned)g_scan.respLen, (unsigned)as608ExpectedFrameLen(g_scan.resp, g_scan.respLen));
                    as608DbgDump("GetImage RX", g_scan.resp, g_scan.respLen);
                }
                if (code == 0x00){
                    g_scan.st = ST_PREDLY_IMAGE2TZ;
                    g_scan.tCmdStart = tickRead();
                    return AS608_SCAN_INPROGRESS;
                } else if (code == 0x02){
                    g_scan.st = ST_DONE_NOMATCH; // sin dedo
                } else {
                    g_scan.st = ST_DONE_ERROR;
                }
            } else {
                // timeout razonable para GetImage
                if ((tickRead() - g_scan.tCmdStart) > AS608_GETIMAGE_LONG_TIMEOUT_MS){
                    g_scan.st = ST_DONE_NOMATCH; // tratar como sin dedo para no ser invasivo
                }
            }
            break;
        }
        case ST_PREDLY_IMAGE2TZ: {
            // Espera mínima sin bloquear
            if ((tickRead() - g_scan.tCmdStart) >= AS608_IMAGE2TZ_PRE_DELAY_MS){
                g_scan.st = ST_SEND_IMAGE2TZ;
            }
            return AS608_SCAN_INPROGRESS;
        }
        case ST_SEND_IMAGE2TZ: {
            g_scan.respLen = 0;
            as608FlushRx(AS608_FLUSH_MS_SHORT);
            uint8_t extra = 0x01; // usar CharBuffer1
            uint8_t payload[2] = { 0x02, extra };
            as608WritePacket(0x01, payload, 2);
            g_scan.tCmdStart = tickRead();
            if (g_as608DebugLevel >= 2) printf("[AS608][DBG2] -> Image2Tz(0x%02X)\r\n", extra);
            g_scan.st = ST_WAIT_IMAGE2TZ;
            return AS608_SCAN_INPROGRESS;
        }
        case ST_WAIT_IMAGE2TZ: {
            as608ReadSlice(g_scan.resp, &g_scan.respLen, sizeof(g_scan.resp), SLICE_MS);
            uint8_t code = as608TryParseAck(g_scan.resp, g_scan.respLen);
            if (code != 0xFF){
                if (g_as608DebugLevel >= 2){
                    printf("[AS608][DBG2] Image2Tz ACK=0x%02X len=%u exp=%u\r\n", code, (unsigned)g_scan.respLen, (unsigned)as608ExpectedFrameLen(g_scan.resp, g_scan.respLen));
                    as608DbgDump("Image2Tz RX", g_scan.resp, g_scan.respLen);
                }
                if (code == 0x00){
                    g_scan.st = ST_SEND_SEARCH;
                    return AS608_SCAN_INPROGRESS;
                } else {
                    g_scan.st = ST_DONE_ERROR;
                }
            } else {
                if ((tickRead() - g_scan.tCmdStart) > AS608_IMAGE2TZ_TIMEOUT_MS){
                    g_scan.st = ST_DONE_ERROR;
                }
            }
            break;
        }
        case ST_SEND_SEARCH: {
            g_scan.respLen = 0;
            as608FlushRx(AS608_FLUSH_MS_TINY);
            uint16_t startPage = 0x0000, pageCount = 0x00A3;
            uint8_t payload[] = { 0x1B, 0x01, (uint8_t)(startPage>>8), (uint8_t)(startPage&0xFF), (uint8_t)(pageCount>>8), (uint8_t)(pageCount&0xFF) };
            as608WritePacket(0x01, payload, sizeof(payload));
            g_scan.tCmdStart = tickRead();
            if (g_as608DebugLevel >= 2) printf("[AS608][DBG2] -> HiSpeedSearch(start=0x%04X,count=0x%04X)\r\n", startPage, pageCount);
            g_scan.st = ST_WAIT_SEARCH;
            return AS608_SCAN_INPROGRESS;
        }
        case ST_WAIT_SEARCH: {
            as608ReadSlice(g_scan.resp, &g_scan.respLen, sizeof(g_scan.resp), SLICE_MS);
            uint8_t code = as608TryParseAck(g_scan.resp, g_scan.respLen);
            if (code != 0xFF){
                uint16_t expect = as608ExpectedFrameLen(g_scan.resp, g_scan.respLen);
                if (g_as608DebugLevel >= 2){
                    printf("[AS608][DBG2] Search ACK=0x%02X len=%u exp=%u\r\n", code, (unsigned)g_scan.respLen, (unsigned)expect);
                    as608DbgDump("Search RX", g_scan.resp, g_scan.respLen);
                }
                if (code == 0x00){
                    if (expect != 0 && g_scan.respLen >= expect && g_scan.respLen >= 14){
                        g_scan.lastId = ((uint16_t)g_scan.resp[10]<<8) | g_scan.resp[11];
                        g_scan.lastScore = ((uint16_t)g_scan.resp[12]<<8) | g_scan.resp[13];
                        if (idOut) *idOut = g_scan.lastId;
                        if (scoreOut) *scoreOut = g_scan.lastScore;
                        // Mensaje solicitado: siempre que haya match almacenado, imprimir ID
                        printf("\r\nHOLA DESDE SENSOR AS608, TU HUELLA ES EL ID: %u\r\n", (unsigned)g_scan.lastId);
                        if (g_as608DebugLevel >= 2){
                            printf("[AS608][DBG2] Match ID=%u Score=%u\r\n", (unsigned)g_scan.lastId, (unsigned)g_scan.lastScore);
                        }
                        g_scan.st = ST_DONE_MATCH;
                    } else {
                        // esperar a completar el frame según header
                        return AS608_SCAN_INPROGRESS;
                    }
                } else {
                    g_scan.st = ST_DONE_NOMATCH;
                }
            } else {
                if ((tickRead() - g_scan.tCmdStart) > AS608_SEARCH_TIMEOUT_MS){
                    g_scan.st = ST_DONE_NOMATCH;
                }
            }
            break;
        }
        case ST_DONE_MATCH:
            // Escribir resultados almacenados antes de retornar
            if (idOut) *idOut = g_scan.lastId;
            if (scoreOut) *scoreOut = g_scan.lastScore;
            g_scan.st = ST_IDLE; // listo para próximo intento
            return AS608_SCAN_MATCH;
        case ST_DONE_NOMATCH:
            g_scan.st = ST_IDLE;
            return AS608_SCAN_NOMATCH;
        case ST_DONE_ERROR:
            g_scan.st = ST_IDLE;
            return AS608_SCAN_ERROR;
    }
    return AS608_SCAN_INPROGRESS;
}

int as608GetTemplateCount(void){
    return as608CmdTemplateCount();
}
