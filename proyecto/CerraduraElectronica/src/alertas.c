#include "alertas.h"
#include "sapi.h"

/* ---------------------------------------------------------------------------
   Configuraci�n de pines de se�alizaci�n
--------------------------------------------------------------------------- */
#define LED_ALERTA     LEDB        // LED indicador (puede ser LED1 o LEDB)

// Configuración para GPIO0[0]
#define BUZZER_GPIO_PORT 0
#define BUZZER_GPIO_PIN  0

/* ---------------------------------------------------------------------------
   Inicializaci�n de perif�ricos de alerta
--------------------------------------------------------------------------- */
void alertasInit(void) {
    gpioConfig(LED_ALERTA, GPIO_OUTPUT);
    
    // Configurar GPIO0[0] para el buzzer
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, false);
    
    gpioWrite(LED_ALERTA, OFF);
}

/* ---------------------------------------------------------------------------
   Alerta de �XITO:
   - Enciende LED fijo por 1 segundo
   - Emite un solo pitido corto del buzzer
--------------------------------------------------------------------------- */
void alertaExito(void) {
    printf("\r\n[ALERTA] �xito\r\n");

    gpioWrite(LED_ALERTA, ON);
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, true);
    delay(150);
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, false);

    delay(850);
    gpioWrite(LED_ALERTA, OFF);
}

/* ---------------------------------------------------------------------------
   Alerta de ERROR:
   - Hace titilar el LED 3 veces
   - Emite 3 pitidos cortos del buzzer
--------------------------------------------------------------------------- */
void alertaError(void) {
    printf("\r\n[ALERTA] Error\r\n");

    for (int i = 0; i < 3; i++) {
        gpioWrite(LED_ALERTA, ON);
        Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, true);
        delay(100);

        Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, false);
        gpioWrite(LED_ALERTA, OFF);
        delay(150);
    }
}
