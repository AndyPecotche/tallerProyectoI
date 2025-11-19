#include "stepperMotor.h"

/**
 *@brief Configura los pines de control del driver DRV8825 (STEP, DIR, ENABLE)
 *        y deja el driver inicialmente deshabilitado.
 *
 * Esta función configura los pines STEP, DIR y EN como salida y coloca
 * el pin EN_PIN en nivel alto (HIGH), lo cual **deshabilita** el driver
 * DRV8825 según su lógica activa en bajo (Active Low).
 *
 * Esto evita movimientos no deseados durante la inicialización.
 *
 * @note Esta funcion se llama 1 vez antes del loop principal
 * @note Para habilitar el driver, se debe poner EN_PIN en LOW antes de comenzar a generar pulsos de STEP.
 */
void driverConfig()
{   
    gpioConfig(STEP_PIN, GPIO_OUTPUT);
    gpioConfig(DIR_PIN, GPIO_OUTPUT);
    gpioConfig(EN_PIN, GPIO_OUTPUT);
    gpioWrite(EN_PIN, HIGH);

}

/**
 * @brief Genera un pulso de paso (STEP) para el driver del motor.
 *
 * Mantiene el pin STEP en nivel alto durante STEP_PULSE_HIGH
 * microsegundos. Para el driver DRV8825, el ancho mínimo de pulso
 * HIGH requerido (tWH) es de 1.9 µs, por lo que este valor suele
 * configurarse entre 2 µs y 10 µs para garantizar un registro seguro.
 *
 * Luego el pin vuelve a nivel bajo, completando el ciclo del pulso.
 *
 * @note Esta función es llamada STEPS veces por step_move(), una por cada paso que debe realizar el motor.
 */
static void step_pulse(void)
{
   gpioWrite(STEP_PIN, HIGH);
   delayInaccurateUs(STEP_PULSE_HIGH);
   gpioWrite(STEP_PIN, LOW);
}

/**
 * @brief Genera el pulso necesario para mover el motor un paso.
 *
 * Configura la dirección y emite un pulso en el pin STEP para
 * producir un único paso del motor. La velocidad dependerá del
 * tiempo entre llamadas sucesivas a esta función.
 *
 * @param[in] dir
 * 		Direccion de giro:
 * 			-true  (1 / ON / HIGH)  -> Giro antihorario (Abrir)
 * 			-false (0 / OFF / LOW)  -> Giro horario (Cerrar)
 *
 * @note Esta función es llamada desde el loop principal cada vez que debe ejecutarse un paso del motor.
 */
void step_move(bool_t dir)
{	gpioWrite(EN_PIN, LOW);
    gpioWrite(DIR_PIN, dir ? LOW : HIGH);

    for (long i = 0; i < STEPS; i++) {
        step_pulse();
        delayInaccurateUs(STEP_PULSE_LOW);
    }
    gpioWrite(EN_PIN, HIGH);
}

