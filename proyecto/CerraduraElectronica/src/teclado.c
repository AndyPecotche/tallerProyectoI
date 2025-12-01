#include "teclado.h"
#include "sapi.h"
#include <string.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
   CONFIGURACI�N HARDWARE DEL TECLADO
--------------------------------------------------------------------------- */
#define KEYPAD_ROWS 4
#define KEYPAD_COLS 3

static gpioMap_t rowPins[KEYPAD_ROWS] = { T_FIL0, T_FIL1, T_FIL2, T_FIL3 };
static gpioMap_t colPins[KEYPAD_COLS] = { T_COL0, T_COL1, T_COL2 };

static const char keypadMap[KEYPAD_ROWS][KEYPAD_COLS] = {
    {'1','2','3'},
    {'4','5','6'},
    {'7','8','9'},
    {'*','0','#'}
};

/* ---------------------------------------------------------------------------
   VARIABLES INTERNAS
--------------------------------------------------------------------------- */
static keypad_t keypad;
#define PIN_LENGTH 5

static char pinIngresado[PIN_LENGTH + 1];
static uint8_t pos = 0;
// Cola de teclas detectadas por ISR (sin bloqueo)
#define KQ_SIZE 8
static volatile char keyQueue[KQ_SIZE];
static volatile uint8_t kqHead = 0;
static volatile uint8_t kqTail = 0;
// Antirrebote por tiempo en ISR
static volatile uint32_t lastIrqMs = 0;
#define DEBOUNCE_MS 40

// Mapeo de columnas a canales PININT usados (evita colisiones con 0 y 1 usados en MEF)
#define KEYPAD_IRQ_CH_COL0 2
#define KEYPAD_IRQ_CH_COL1 3
#define KEYPAD_IRQ_CH_COL2 4

// Mapeo GPIO (puerto y pin) de columnas segun EDU-CIAA-NXP
// T_COL0 -> GPIO1[8], T_COL1 -> GPIO3[12], T_COL2 -> GPIO3[13]
#define COL0_GPIO_PORT 1
#define COL0_GPIO_PIN  8
#define COL1_GPIO_PORT 3
#define COL1_GPIO_PIN  12
#define COL2_GPIO_PORT 3
#define COL2_GPIO_PIN  13

static inline bool kqIsEmpty(void){ return kqHead == kqTail; }
static inline bool kqIsFull(void){ return (uint8_t)(kqHead + 1) % KQ_SIZE == kqTail; }
static inline void kqPush(char c){ if(!kqIsFull()){ keyQueue[kqHead] = c; kqHead = (uint8_t)(kqHead + 1) % KQ_SIZE; } }
static inline bool kqPop(char *c){ if(kqIsEmpty()) return false; *c = keyQueue[kqTail]; kqTail = (uint8_t)(kqTail + 1) % KQ_SIZE; return true; }

// Helper: pone todas las filas en nivel bajo (estado armado para despertar por GPIO)
static inline void filasTodasBajo(void){
    for (uint8_t r = 0; r < KEYPAD_ROWS; r++) {
        gpioWrite(rowPins[r], LOW);
    }
}

// Escaneo rápido para una columna específica, retorna char o 0 si no se identificó
static char escanearColumna(uint8_t cIndex){
    // Poner todas las filas en ALTO excepto la actual al iterar
    for (uint8_t r = 1; r < KEYPAD_ROWS; r++) {
        gpioWrite(rowPins[r], HIGH);
    }
    for (uint8_t r = 0; r < KEYPAD_ROWS; r++) {
        if (r > 0) gpioWrite(rowPins[r-1], HIGH);
        gpioWrite(rowPins[r], LOW);
        // Leer la columna indicada
        if (!gpioRead(colPins[cIndex])) {
            // Restaurar estado armado (todas bajo) y devolver
            filasTodasBajo();
            return keypadMap[r][cIndex];
        }
    }
    // Restaurar estado armado por las dudas
    filasTodasBajo();
    return 0;
}

static void configurarInterrupcionesTeclado(void){
    // Inicializar controlador de interrupciones de GPIO
    Chip_PININT_Init(LPC_GPIO_PIN_INT);

    // Seleccionar pines de columnas para canales 2,3,4
    Chip_SCU_GPIOIntPinSel(KEYPAD_IRQ_CH_COL0, COL0_GPIO_PORT, COL0_GPIO_PIN);
    Chip_SCU_GPIOIntPinSel(KEYPAD_IRQ_CH_COL1, COL1_GPIO_PORT, COL1_GPIO_PIN);
    Chip_SCU_GPIOIntPinSel(KEYPAD_IRQ_CH_COL2, COL2_GPIO_PORT, COL2_GPIO_PIN);

    // Limpiar estados previos
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL0));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL1));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL2));

    // Interrupciones por flanco (caída: tecla conecta fila baja a columna con pull-up)
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL0));
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL1));
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL2));

    Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL0));
    Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL1));
    Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL2));

    NVIC_ClearPendingIRQ(PIN_INT0_IRQn + KEYPAD_IRQ_CH_COL0);
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn + KEYPAD_IRQ_CH_COL1);
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn + KEYPAD_IRQ_CH_COL2);
    NVIC_EnableIRQ(PIN_INT0_IRQn + KEYPAD_IRQ_CH_COL0);
    NVIC_EnableIRQ(PIN_INT0_IRQn + KEYPAD_IRQ_CH_COL1);
    NVIC_EnableIRQ(PIN_INT0_IRQn + KEYPAD_IRQ_CH_COL2);
}

/* ---------------------------------------------------------------------------
   INICIALIZACI�N
--------------------------------------------------------------------------- */
void tecladoInit(void) {
    keypadConfig(&keypad, rowPins, KEYPAD_ROWS, colPins, KEYPAD_COLS);
    // Estado armado: filas en bajo para que cualquier tecla provoque flanco de bajada en una columna
    filasTodasBajo();
    configurarInterrupcionesTeclado();
    tecladoReset();
    printf("\r\n[TECLADO] Inicializado.\r\n");
}

/* ---------------------------------------------------------------------------
   REINICIA EL BUFFER DEL TECLADO
--------------------------------------------------------------------------- */
void tecladoReset(void){
    memset(pinIngresado, 0, sizeof(pinIngresado));
    pos = 0;
    // Vaciar cola de teclas pendientes
    kqHead = kqTail = 0;
}

/* ---------------------------------------------------------------------------
   LEE UN PIN DE 5 D�GITOS
   - Retorna 1 cuando se completan los 5 d�gitos.
   - Retorna -1 si se presiona '*' (c�digo de men�).
   - Retorna 0 si a�n se est� ingresando.
   - Copia el PIN completo en el par�metro 'pin'.
   - Actualiza ultimaActividad cada vez que se presiona una tecla.
--------------------------------------------------------------------------- */
int tecladoLeerPin(char *pin, uint32_t *ultimaActividad) {
    char teclaChar;
    if (kqPop(&teclaChar)) {
        // Actualizar timestamp de actividad
        if (ultimaActividad != NULL) {
            *ultimaActividad = tickRead();
            printf("\r\n[DEBUG] Timeout reiniciado por tecla: %c\r\n", teclaChar);
        }

        // Detectar tecla '*' para menú
        if (teclaChar == '*') {
            tecladoReset();
            printf("\r\n[TECLADO] Acceso a menú admin\r\n");
            return -1;
        }

        // Acumular dígito
        if (pos < PIN_LENGTH) {
            pinIngresado[pos++] = teclaChar;
            printf("*");
            fflush(stdout);
        }

        // PIN completo
        if (pos == PIN_LENGTH) {
            pinIngresado[PIN_LENGTH] = '\0';
            strcpy(pin, pinIngresado);
            tecladoReset();
            printf("\r\n[TECLADO] PIN completo: %s\r\n", pin);
            return 1;
        }
    }

    // Nada aún
    return 0;
}

/* ---------------------------------------------------------------------------
   LEE UNA SOLA TECLA (para men�s u opciones)
   - Devuelve true si se presion� una tecla v�lida.
   - La tecla le�da se guarda en el par�metro 'tecla'.
--------------------------------------------------------------------------- */
bool tecladoLeerTecla(char *tecla) {
    if (kqPop(tecla)) {
        printf("\r\n[TECLADO] Tecla presionada: %c\r\n", *tecla);
        return true;
    }
    return false;
}

bool tecladoDisponible(void){
    return !kqIsEmpty();
}

/* ------------------------ ISR de teclado por columnas --------------------- */
// Nota: Los canales 0 y 1 están usados en MEF para presencia y RFID.
// Aquí usamos 2,3,4 mapeados a T_COL0, T_COL1 y T_COL2 respectivamente.

void GPIO2_IRQHandler(void){
    // Antirrebote simple por ventana de tiempo
    uint32_t now = tickRead();
    if ((now - lastIrqMs) >= DEBOUNCE_MS) {
        char c = escanearColumna(0);
        if (c) { kqPush(c); lastIrqMs = now; }
    }
    Chip_PININT_ClearFallStates(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL0));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL0));
}

void GPIO3_IRQHandler(void){
    uint32_t now = tickRead();
    if ((now - lastIrqMs) >= DEBOUNCE_MS) {
        char c = escanearColumna(1);
        if (c) { kqPush(c); lastIrqMs = now; }
    }
    Chip_PININT_ClearFallStates(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL1));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL1));
}

void GPIO4_IRQHandler(void){
    uint32_t now = tickRead();
    if ((now - lastIrqMs) >= DEBOUNCE_MS) {
        char c = escanearColumna(2);
        if (c) { kqPush(c); lastIrqMs = now; }
    }
    Chip_PININT_ClearFallStates(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL2));
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH(KEYPAD_IRQ_CH_COL2));
}