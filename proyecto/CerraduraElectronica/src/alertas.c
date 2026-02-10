#include "alertas.h"
#include "sapi.h"

/* ---------------------------------------------------------------------------
   Configuraciï¿½n de pines de seï¿½alizaciï¿½n
--------------------------------------------------------------------------- */
// ConfiguraciÃ³n para GPIO0[0]
#define BUZZER_GPIO_PORT 0
#define BUZZER_GPIO_PIN  0

/* ---------------------------------------------------------------------------
   Inicializaciï¿½n de perifï¿½ricos de alerta
--------------------------------------------------------------------------- */
void alertasInit(void) {
    
    // Configurar GPIO0[0] para el buzzer
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, false);
}

/* ---------------------------------------------------------------------------
   Alerta de ï¿½XITO:
--------------------------------------------------------------------------- */
void alertaExito(void) {
    printf("\r\n[ALERTA] Acceso Correcto\r\n");

    // Primer beep
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, true);
    delay(100);
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, false);

    // Pequeña pausa
    delay(120);

    // Segundo beep
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, true);
    delay(100);
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, false);
}

/* ---------------------------------------------------------------------------
   Alerta de ERROR:
--------------------------------------------------------------------------- */
void alertaError(void) {
    printf("\r\n[ALERTA] Error\r\n");
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, true);
	delay(1000);
	Chip_GPIO_SetPinState(LPC_GPIO_PORT, BUZZER_GPIO_PORT, BUZZER_GPIO_PIN, false);
}
