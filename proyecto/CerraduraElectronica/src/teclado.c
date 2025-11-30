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
// Estado para evitar repetición al mantener apretada una tecla
static bool keyHeld = false;
static uint16_t lastIndex = 0xFFFF;

/* ---------------------------------------------------------------------------
   INICIALIZACI�N
--------------------------------------------------------------------------- */
void tecladoInit(void) {
    keypadConfig(&keypad, rowPins, KEYPAD_ROWS, colPins, KEYPAD_COLS);
    tecladoReset();
    printf("\r\n[TECLADO] Inicializado.\r\n");
}

/* ---------------------------------------------------------------------------
   REINICIA EL BUFFER DEL TECLADO
--------------------------------------------------------------------------- */
void tecladoReset(void){
    memset(pinIngresado, 0, sizeof(pinIngresado));
    pos = 0;
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
    uint16_t teclaIndex;
    char teclaChar;

    if (keypadRead(&keypad, &teclaIndex)) {
        // Edge detection: solo procesar primera detección o cambio de tecla
        if (!keyHeld || teclaIndex != lastIndex) {
            keyHeld = true;
            lastIndex = teclaIndex;

            uint16_t fila = teclaIndex / KEYPAD_COLS;
            uint16_t columna = teclaIndex % KEYPAD_COLS;
            teclaChar = keypadMap[fila][columna];

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
    } else {
        // Liberación de tecla: listo para próxima detección
        keyHeld = false;
        lastIndex = 0xFFFF;
    }

    delay(20); // antirrebote ligero; edge evita repetidos
    return 0;
}

/* ---------------------------------------------------------------------------
   LEE UNA SOLA TECLA (para men�s u opciones)
   - Devuelve true si se presion� una tecla v�lida.
   - La tecla le�da se guarda en el par�metro 'tecla'.
--------------------------------------------------------------------------- */
bool tecladoLeerTecla(char *tecla) {
    uint16_t teclaIndex;

    if (keypadRead(&keypad, &teclaIndex)) {
        // Edge detection: reportar solo primera detección o cambio
        if (!keyHeld || teclaIndex != lastIndex) {
            keyHeld = true;
            lastIndex = teclaIndex;

            uint16_t fila = teclaIndex / KEYPAD_COLS;
            uint16_t columna = teclaIndex % KEYPAD_COLS;
            *tecla = keypadMap[fila][columna];
            printf("\r\n[TECLADO] Tecla presionada: %c\r\n", *tecla);
            delay(20);
            return true;
        }
    } else {
        // Liberación
        keyHeld = false;
        lastIndex = 0xFFFF;
    }

    return false;
}