#include "alertas.h"
#include "sapi.h"

/* ---------------------------------------------------------------------------
   Configuración de pines de señalización
--------------------------------------------------------------------------- */
#define LED_ALERTA     LEDB        // LED indicador (puede ser LED1 o LEDB)
#define BUZZER_PIN     GPIO0       // Cambiar por el pin conectado al buzzer

/* ---------------------------------------------------------------------------
   Inicialización de periféricos de alerta
--------------------------------------------------------------------------- */
void alertasInit(void) {
    gpioConfig(LED_ALERTA, GPIO_OUTPUT);
    gpioConfig(BUZZER_PIN, GPIO_OUTPUT);
    gpioWrite(LED_ALERTA, OFF);
    gpioWrite(BUZZER_PIN, OFF);
}

/* ---------------------------------------------------------------------------
   Alerta de ÉXITO:
   - Enciende LED fijo por 1 segundo
   - Emite un solo pitido corto del buzzer
--------------------------------------------------------------------------- */
void alertaExito(void) {
    printf("\r\n[ALERTA] Éxito\r\n");

    gpioWrite(LED_ALERTA, ON);
    gpioWrite(BUZZER_PIN, ON);
    delay(150);
    gpioWrite(BUZZER_PIN, OFF);

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
        gpioWrite(BUZZER_PIN, ON);
        delay(100);

        gpioWrite(BUZZER_PIN, OFF);
        gpioWrite(LED_ALERTA, OFF);
        delay(150);
    }
}
