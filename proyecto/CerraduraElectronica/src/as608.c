#include "as608.h"
#include "sapi.h"
#include <string.h>
#include <stdio.h>

/* Selección del UART para el AS608.
 * En la placa ciaa_nxp (LPC4337) los UART expuestos por sAPI son:
 *   UART_485 (valor 1)  -> Transceiver RS485 (periférico UART0)
 *   UART_USB (valor 3)  -> Puerto DEBUG USB (periférico UART2)
 *   UART_232 (valor 5)  -> Conector DB9 RS232 (periférico UART3)
 * No existe macro UART_0; para usar los pines U0_TXD/U0_RXD debes emplear UART_485
 * si el transceiver RS485 está disponible o re-mapear manualmente pines fuera de sAPI.
 */
#ifndef AS608_UART
    /* Por defecto usar UART0 via GPIO1(TX)/GPIO2(RX) en header P0.
       UART_GPIO = 0 (sAPI index) mapea a LPC_USART0 con pines SCU 6_4/6_5 (FUNC2)
       Este valor corresponde a UART_GPIO en edu_ciaa_nxp */
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
#define AS608_GETIMAGE_TIMEOUT_MS      250
#define AS608_GETIMAGE_LONG_TIMEOUT_MS 300
#define AS608_IMAGE2TZ_PRE_DELAY_MS    80
#define AS608_IMAGE2TZ_TIMEOUT_MS      600
#define AS608_SEARCH_TIMEOUT_MS        250
#define AS608_PROBE_TIMEOUT_MS         3000
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

/* Nivel de debug runtime (configurable) */
static uint8_t g_as608DebugLevel;
void as608SetDebug(uint8_t level){ g_as608DebugLevel = level; }

/* Estadísticas de resultados del polling */
static struct {
    uint32_t intentos;      // total de llamadas a as608PollHuella
    uint32_t imgOk;         // r == 0x00
    uint32_t sinDedo;       // r == 0x02
    uint32_t ackInvalid;    // r == 0xFF (timeout / paquete inválido)
    uint32_t otrosCodigos;  // cualquier otro código GetImage
    uint32_t image2tzFail;  // tz != 0x00
    uint32_t searchOk;      // coincidencia sr == 0
    uint32_t searchNoMatch; // sr == 0x09 / 0x08
    uint32_t searchError;   // otros sr
} g_as608Stats;

void as608PrintStats(void){
    printf("\r\n[AS608][STATS] intentos=%lu imgOk=%lu sinDedo=%lu ackInvalid=%lu otros=%lu tzFail=%lu searchOk=%lu noMatch=%lu searchErr=%lu\r\n",
        (unsigned long)g_as608Stats.intentos,
        (unsigned long)g_as608Stats.imgOk,
        (unsigned long)g_as608Stats.sinDedo,
        (unsigned long)g_as608Stats.ackInvalid,
        (unsigned long)g_as608Stats.otrosCodigos,
        (unsigned long)g_as608Stats.image2tzFail,
        (unsigned long)g_as608Stats.searchOk,
        (unsigned long)g_as608Stats.searchNoMatch,
        (unsigned long)g_as608Stats.searchError);
}

/*
 * Nota sobre protocolo:
 * El AS608/R305 usa un protocolo binario (paquetes con encabezado 0xEF01,
 * dirección, tipo de paquete y longitud). Implementar el protocolo completo
 * excede este alcance; aquí se provee una interfaz mínima y hooks para
 * integrar comandos clave. Las funciones retornan el ID textual (4 chars)
 * que el módulo suele asignar/retornar.
 */

static bool as608Send(const uint8_t *data, size_t len){
    for(size_t i=0; i<len; i++){
        uartWriteByte(AS608_UART, data[i]);
    }
    if(g_as608DebugLevel >= 3){
        printf("[AS608][TX] ");
        for(size_t i=0;i<len;i++){
            printf("%02X", data[i]);
            if(i+1<len) printf(" ");
        }
        printf("\r\n");
    }
    return true;
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
    if(n>0 && g_as608DebugLevel >= 3){
        printf("[AS608][RX] ");
        for(size_t i=0;i<n;i++){
            printf("%02X", buf[i]);
            if(i+1<n) printf(" ");
        }
        printf("\r\n");
    }
    return n;
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

/* Comandos básicos */
static uint8_t as608CmdGetImage(void){
    uint8_t payload[] = { 0x01 }; // GetImage
    as608FlushRx(AS608_FLUSH_MS_SHORT);
    delay(AS608_DELAY_BEFORE_SEND_MS);
    as608WritePacket(0x01, payload, sizeof(payload));
    uint8_t resp[32];
    // Fast empty scan: usar timeout corto (60 ms). Respuestas típicas ~10-20 ms.
    // Conservador: mayor timeout para asegurar captura del ACK
    return as608GetAck(resp, sizeof(resp), AS608_GETIMAGE_TIMEOUT_MS);
}

/* Variante con mayor timeout usada en ENROLL para dar tiempo al sensor */
static uint8_t as608CmdGetImageLong(void){
    uint8_t payload[] = { 0x01 };
    as608FlushRx(AS608_FLUSH_MS_SHORT);
    delay(AS608_DELAY_BEFORE_SEND_MS);
    as608WritePacket(0x01, payload, sizeof(payload));
    uint8_t resp[32];
    return as608GetAck(resp, sizeof(resp), AS608_GETIMAGE_LONG_TIMEOUT_MS); // más holgado
}

static uint8_t as608CmdImage2Tz(uint8_t slot /*1 o 2*/){
    uint8_t payload[] = { 0x02, slot };
    // Dar tiempo a que el sensor procese la imagen antes de convertir
    delay(AS608_IMAGE2TZ_PRE_DELAY_MS);
    as608FlushRx(AS608_FLUSH_MS_SHORT);
    as608WritePacket(0x01, payload, sizeof(payload));
    uint8_t resp[32];
    return as608GetAck(resp, sizeof(resp), AS608_IMAGE2TZ_TIMEOUT_MS);
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
    static uint32_t intentos = 0;
    // Usar versión "long" para mayor robustez
    uint8_t r = as608CmdGetImageLong();
    g_as608Stats.intentos++;
    if (r != 0x00){
        if(r == 0x02) g_as608Stats.sinDedo++; else if (r == 0xFF) g_as608Stats.ackInvalid++; else g_as608Stats.otrosCodigos++;
        if(g_as608DebugLevel >= 2){
            if(r == 0x02){
                // Sin dedo: mostrar cada cierto número para confirmar vida del sensor
                if((g_as608Stats.intentos % 20) == 0){
                    printf("[AS608][DBG] Sin dedo (r=0x02) intentos=%lu\r\n", (unsigned long)g_as608Stats.intentos);
                }
            } else if (r == 0xFF){
                if((g_as608Stats.intentos % 10) == 0){
                    printf("[AS608][DBG] ACK inválido/timeout (r=0xFF) intentos=%lu\r\n", (unsigned long)g_as608Stats.intentos);
                }
            } else {
                // Otros códigos ocasionales
                if((g_as608Stats.intentos % 10) == 0){
                    printf("[AS608][DBG] GetImage code=0x%02X intentos=%lu\r\n", r, (unsigned long)g_as608Stats.intentos);
                }
            }
        } else if (g_as608DebugLevel == 1){
            // Nivel 1: solo errores distintos a 'sin dedo' cada cierto tiempo
            if(r != 0x02 && (g_as608Stats.intentos % 15) == 0){
                printf("[AS608][DBG] GetImage code=0x%02X intentos=%lu\r\n", r, (unsigned long)g_as608Stats.intentos);
            }
        }
        if(g_as608Stats.intentos != 0 && (g_as608Stats.intentos % 25) == 0 && g_as608DebugLevel >= 2){
            as608PrintStats();
        }
        return false; // sin dedo o error
    }
    g_as608Stats.imgOk++;
    if(g_as608DebugLevel >= 1){
        printf("[AS608][DBG] Dedo detectado (GetImage OK)\r\n");
    }
    // Pequeña espera adicional antes de convertir
    delay(AS608_ENROLL_STEP_DELAY_MS);
    uint8_t tz = as608CmdImage2Tz(0x01);
    if (tz != 0x00){
        g_as608Stats.image2tzFail++;
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
        g_as608Stats.searchOk++;
        if(g_as608DebugLevel >= 1){
            printf("[AS608][DBG] Coincidencia ID=%u score=%u\r\n", (unsigned)id, (unsigned)score);
        }
        return true; // éxito; el mensaje se mostrará fuera (MEF/validación)
    }
    if(sr == 0x09 || sr == 0x08) g_as608Stats.searchNoMatch++; else g_as608Stats.searchError++;
    if(g_as608DebugLevel >= 1){
        printf("[AS608][DBG] Búsqueda sin coincidencia (sr=0x%02X)\r\n", (unsigned)sr);
    }
    if(g_as608Stats.intentos != 0 && (g_as608Stats.intentos % 25) == 0 && g_as608DebugLevel >= 2){
        as608PrintStats();
    }
    return false;
}

/* Comando RegModel (0x05) para combinar CharBuffer1 y CharBuffer2 en un template */
static uint8_t as608CmdRegModel(void){
    uint8_t payload[] = { 0x05 };
    as608FlushRx(AS608_FLUSH_MS_TINY);
    as608WritePacket(0x01, payload, sizeof(payload));
    uint8_t resp[32];
    return as608GetAck(resp, sizeof(resp), AS608_REGMODEL_TIMEOUT_MS);
}

/* Comando Store (0x06) buffer=0x01, ID destino */
static uint8_t as608CmdStore(uint16_t id){
    uint8_t payload[] = { 0x06, 0x01, (uint8_t)(id>>8), (uint8_t)(id&0xFF) };
    as608FlushRx(AS608_FLUSH_MS_TINY);
    as608WritePacket(0x01, payload, sizeof(payload));
    uint8_t resp[32];
    return as608GetAck(resp, sizeof(resp), AS608_STORE_TIMEOUT_MS);
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

bool as608Enroll(char idOut[5]){
    if(!idOut) return false;
    memset(idOut,0,5);
    printf("\r\n[AS608][ENROLL] Inicio de registro de huella\r\n");
    printf("[AS608][ENROLL] Paso 1: Coloque dedo (imágen 1)\r\n");
    uint32_t start = tickRead();
    uint32_t lastAttempt = 0;
    int errorCount = 0;
    while ((tickRead() - start) < 10000){
        if((tickRead() - lastAttempt) < AS608_ENROLL_ATTEMPT_INTERVAL_MS){
            delay(10);
            continue;
        }
        lastAttempt = tickRead();
        uint8_t r = as608CmdGetImageLong();
        if(r == 0x00){
            printf("[AS608][ENROLL] Imagen 1 capturada\r\n");
            delay(AS608_ENROLL_STEP_DELAY_MS);
            uint8_t tz = as608CmdImage2Tz(0x01);
            if(tz != 0x00){
                printf("[AS608][ENROLL] Error Image2Tz(1) code=0x%02X\r\n", tz);
                return false;
            }
            break;
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
    if((tickRead() - start) >= 10000){
        printf("[AS608][ENROLL] Timeout esperando dedo (imagen 1)\r\n");
        return false;
    }

    printf("[AS608][ENROLL] Retire dedo...\r\n");
    delay(1500);
    printf("[AS608][ENROLL] Paso 2: Coloque nuevamente el mismo dedo (imágen 2)\r\n");
    start = tickRead();
    lastAttempt = 0;
    errorCount = 0;
    while ((tickRead() - start) < 10000){
        if((tickRead() - lastAttempt) < AS608_ENROLL_ATTEMPT_INTERVAL_MS){
            delay(10);
            continue;
        }
        lastAttempt = tickRead();
        uint8_t r = as608CmdGetImageLong();
        if(r == 0x00){
            printf("[AS608][ENROLL] Imagen 2 capturada\r\n");
            delay(AS608_ENROLL_STEP_DELAY_MS);
            uint8_t tz = as608CmdImage2Tz(0x02);
            if(tz != 0x00){
                printf("[AS608][ENROLL] Error Image2Tz(2) code=0x%02X\r\n", tz);
                return false;
            }
            break;
        } else if (r == 0x02){
            delay(AS608_ENROLL_NOFINGER_DELAY_MS);
        } else {
            errorCount++;
            if(errorCount % 5 == 0){
                printf("[AS608][ENROLL] GetImage errores acumulados=%d (último=0x%02X)\r\n", errorCount, r);
            }
            delay(AS608_ENROLL_ERROR_DELAY_MS);
        }
    }
    if((tickRead() - start) >= 10000){
        printf("[AS608][ENROLL] Timeout esperando dedo (imagen 2)\r\n");
        return false;
    }

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

/*
 * Probing utilitario: prueba distintos baudios y comandos para detectar vida.
 * Secuencia: intenta 9600 -> 57600 -> 115200. En cada uno:
 *  - Envia VfyPwd y espera cualquier respuesta (>=1 byte)
 *  - Si ve bytes, reporta baudio y retorna true
 */
bool as608Probe(void){
    const uint32_t bauds[] = {9600, 57600, 115200};
    const size_t nBauds = sizeof(bauds)/sizeof(bauds[0]);
    for(size_t i=0; i<nBauds; i++){
        uint32_t br = bauds[i];
        uartConfig(AS608_UART, br);
        printf("\r\n[AS608][PROBE] Probando a %u baud...\r\n", (unsigned)br);
        as608FlushRx(10);

        // Reusar comando VfyPwd
        const uint8_t cmdVfyPwd[] = {
            0xEF,0x01, 0xFF,0xFF,0xFF,0xFF, 0x01, 0x00,0x07, 0x13, 0x00,0x00,0x00,0x00, 0x00,0x1B
        };
        as608Send(cmdVfyPwd, sizeof(cmdVfyPwd));
        delay(AS608_DELAY_BEFORE_SEND_MS);
        uint8_t resp[64];
        size_t n = as608Recv(resp, sizeof(resp), AS608_PROBE_TIMEOUT_MS);
        printf("[AS608][PROBE] Bytes recibidos: %u\r\n", (unsigned)n);
        if(n>0){
            printf("[AS608][PROBE] Actividad detectada en %u baud\r\n", (unsigned)br);
            return true;
        }
    }
    printf("[AS608][PROBE] Sin actividad en 9600/57600/115200\r\n");
    return false;
}

/*
 * as608Check: verifica comunicación con el sensor
 * Envía comando VfyPwd (0x13) con contraseña por defecto 0x00000000
 * Espera ACK con código de confirmación 0x00
 */
bool as608Check(void){
    printf("\r\n[AS608][INIT] Verificando comunicación con sensor...\r\n");
    // Paquete VfyPwd: Header(2) + Addr(4) + PID(1) + Len(2) + Instr(1) + Pwd(4) + Chksum(2)
    // Ejemplo: EF01 FFFFFFFF 01 0008 13 00000000 001C
    const uint8_t cmdVfyPwd[] = {
        0xEF, 0x01,                     // Header
        0xFF, 0xFF, 0xFF, 0xFF,         // Address (default broadcast)
        0x01,                           // Package identifier (command)
        0x00, 0x07,                     // Length (7 bytes: instr + pwd + chksum)
        0x13,                           // Instruction VfyPwd
        0x00, 0x00, 0x00, 0x00,         // Password (default 0x00000000)
        0x00, 0x1B                      // Checksum (0x01+0x00+0x07+0x13+0x00*4 = 0x1B)
    };
    // Vaciar RX antes del envío
    as608FlushRx(AS608_FLUSH_MS_TINY);
    as608Send(cmdVfyPwd, sizeof(cmdVfyPwd));
    // Dar tiempo mínimo al módulo a responder
    delay(AS608_CHECK_DELAY_MS);
    uint8_t resp[32];
    size_t n = as608Recv(resp, sizeof(resp), AS608_CHECK_TIMEOUT_MS);
    
    if(n < 12){
        printf("[AS608][CHECK] Respuesta insuficiente (%u bytes)\r\n", (unsigned)n);
        return false;
    }
    
    // Verificar header EF01
    if(resp[0] != 0xEF || resp[1] != 0x01){
        printf("[AS608][CHECK] Header inválido\r\n");
        return false;
    }
    
    // Verificar ACK package type (0x07)
    if(resp[6] != 0x07){
        printf("[AS608][CHECK] No es paquete ACK (tipo=%02X)\r\n", resp[6]);
        return false;
    }
    
    // Byte 9 es el código de confirmación (0x00 = OK)
    uint8_t confirmCode = resp[9];
    if(confirmCode == 0x00){
        printf("[AS608][CHECK] Sensor responde OK\r\n");
        return true;
    } else {
        printf("[AS608][CHECK] Código de error: 0x%02X\r\n", confirmCode);
        return false;
    }
}

/*
 * grabarHuella: flujo típico (simplificado)
 * 1) Pedir imagen (collect finger)
 * 2) Convertir a características (image2Tz)
 * 3) Crear modelo (regModel)
 * 4) Guardar en índice libre (store)
 * 5) Obtener ID asignado (aquí simulado/extraído si el sensor lo reporta)
 *
 * Esta implementación solo esqueleto: envía comandos placeholders y
 * espera una respuesta que contenga un ID ASCII de 4 chars.
 */
bool grabarHuella(char idOut[5]){
    if(!idOut) return false;
    memset(idOut, 0, 5);

    // Placeholder: comandos binarios reales deben construirse según datasheet AS608/R305.
    // Por ahora, pedimos al usuario apoyar huella y leemos cualquier respuesta del módulo.
    printf("\r\n[AS608] Apoye el dedo para registrar...\r\n");

    // Envío de un comando ficticio (por ejemplo, GetImage)
    const uint8_t cmdGetImage[] = { 0xEF,0x01, 0xFF,0xFF,0xFF,0xFF, 0x01, 0x00,0x03, 0x01, 0x00, 0x05 };
    as608Send(cmdGetImage, sizeof(cmdGetImage));

    uint8_t resp[64];
    size_t n = as608Recv(resp, sizeof(resp), 3000);
    if(n == 0){
        printf("[AS608] Tiempo de espera sin respuesta\r\n");
        return false;
    }

    // Buscar un posible ID ASCII de 4 chars dentro de la respuesta
    for(size_t i=0; i+4<=n; i++){
        if(resp[i] >= '0' && resp[i] <= '9'){
            if(resp[i+1] >= '0' && resp[i+1] <= '9' &&
               resp[i+2] >= '0' && resp[i+2] <= '9' &&
               resp[i+3] >= '0' && resp[i+3] <= '9'){
                memcpy(idOut, &resp[i], 4);
                idOut[4] = '\0';
                printf("[AS608] Huella registrada ID=%s\r\n", idOut);
                return true;
            }
        }
    }

    // Si no se encontró ID, simular uno (para pruebas)
    strcpy(idOut, "0001");
    printf("[AS608] Huella registrada (SIMULADA!!!) ID=%s\r\n", idOut);
    return true;
}

/* leerHuella: flujo típico de búsqueda
 * 1) Pedir imagen
 * 2) Convertir a características
 * 3) Buscar en base (search)
 * 4) El sensor devuelve posición/ID; extraer y retornar
 */
bool leerHuella(void){
    printf("\r\n[AS608] Lectura manual de huella iniciada\r\n");
    uint32_t start = tickRead();
    while ((tickRead() - start) < 8000){
        uint8_t r = as608CmdGetImage();
        if (r == 0x00){
            uint8_t tz = as608CmdImage2Tz(0x01);
            if (tz != 0x00){
                return false;
            }
            uint16_t id=0, score=0;
            int sr = as608CmdHiSpeedSearch(0x0000, 0x00A3, &id, &score);
            if (sr == 0){
                return true;
            } else if (sr == 0x09 /*NOTFOUND*/ || sr == 0x08 /*NOMATCH*/){
                return false;
            } else {
                return false;
            }
        } else if (r == 0x02){
            // No finger
            delay(50);
            continue;
        } else {
            delay(100);
        }
    }
    return false;
}
