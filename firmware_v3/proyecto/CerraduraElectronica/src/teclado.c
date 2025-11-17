#include "teclado.h"
#include "sapi.h"
#include <string.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
   CONFIGURACIÓN HARDWARE DEL TECLADO
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

/* ---------------------------------------------------------------------------
   INICIALIZACIÓN
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
   LEE UN PIN DE 5 DÍGITOS
   - Retorna true cuando se completan los 5 dígitos.
   - Copia el PIN completo en el parámetro 'pin'.
--------------------------------------------------------------------------- */
bool tecladoLeerPin(char *pin) {
    uint16_t teclaIndex;
    char teclaChar;

    if (keypadRead(&keypad, &teclaIndex)) {
        uint16_t fila = teclaIndex / KEYPAD_COLS;
        uint16_t columna = teclaIndex % KEYPAD_COLS;
        teclaChar = keypadMap[fila][columna];

        if (pos < PIN_LENGTH) {
            pinIngresado[pos++] = teclaChar;
            printf("*");
            fflush(stdout);
        }

        if (pos == PIN_LENGTH) {
            pinIngresado[PIN_LENGTH] = '\0';
            strcpy(pin, pinIngresado);
            tecladoReset();
            printf("\r\n[TECLADO] PIN completo: %s\r\n", pin);
            return true;
        }
    }

    delay(40); // antirrebote
    return false;
}

/* ---------------------------------------------------------------------------
   LEE UNA SOLA TECLA (para menús u opciones)
   - Devuelve true si se presionó una tecla válida.
   - La tecla leída se guarda en el parámetro 'tecla'.
--------------------------------------------------------------------------- */
bool tecladoLeerTecla(char *tecla) {
    uint16_t teclaIndex;

    if (keypadRead(&keypad, &teclaIndex)) {
        uint16_t fila = teclaIndex / KEYPAD_COLS;
        uint16_t columna = teclaIndex % KEYPAD_COLS;
        *tecla = keypadMap[fila][columna];
        printf("\r\n[TECLADO] Tecla presionada: %c\r\n", *tecla);
        delay(80);  // Anti-rebote
        return true;
    }

    return false;
}