#include "stepperMotor.h"

/**
 *@brief Configura los pines de control del driver DRV8825 (STEP, DIR, ENABLE)
 *        y deja el driver inicialmente deshabilitado.
 *
 * Esta funci�n configura los pines STEP, DIR y EN como salida y coloca
 * el pin EN_PIN en nivel alto (HIGH), lo cual **deshabilita** el driver
 * DRV8825 seg�n su l�gica activa en bajo (Active Low).
 *
 * Esto evita movimientos no deseados durante la inicializaci�n.
 *
 * @note Esta funcion se llama 1 vez antes del loop principal
 * @note Para habilitar el driver, se debe poner EN_PIN en LOW antes de comenzar a generar pulsos de STEP.
 */
void driverConfig()
{   
    // Configurar GPIO2[8] para STEP_PIN
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, STEP_GPIO_PORT, STEP_GPIO_PIN);
    
    // Configurar GPIO5[16] para DIR_PIN
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, DIR_GPIO_PORT, DIR_GPIO_PIN);
    
    // Configurar GPIO3[0] para EN_PIN
    Chip_GPIO_SetPinDIROutput(LPC_GPIO_PORT, EN_GPIO_PORT, EN_GPIO_PIN);
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, EN_GPIO_PORT, EN_GPIO_PIN, true); // HIGH = deshabilitado
}

/**
 * @brief Genera un pulso de paso (STEP) para el driver del motor.
 *
 * Mantiene el pin STEP en nivel alto durante STEP_PULSE_HIGH
 * microsegundos. Para el driver DRV8825, el ancho m�nimo de pulso
 * HIGH requerido (tWH) es de 1.9 �s, por lo que este valor suele
 * configurarse entre 2 �s y 10 �s para garantizar un registro seguro.
 *
 * Luego el pin vuelve a nivel bajo, completando el ciclo del pulso.
 *
 * @note Esta funci�n es llamada STEPS veces por step_move(), una por cada paso que debe realizar el motor.
 */
static void step_pulse(void)
{
   Chip_GPIO_SetPinState(LPC_GPIO_PORT, STEP_GPIO_PORT, STEP_GPIO_PIN, true);  // HIGH
   delayInaccurateUs(STEP_PULSE_HIGH);
   Chip_GPIO_SetPinState(LPC_GPIO_PORT, STEP_GPIO_PORT, STEP_GPIO_PIN, false); // LOW
}

/**
 * @brief Genera el pulso necesario para mover el motor un paso.
 *
 * Configura la direcci�n y emite un pulso en el pin STEP para
 * producir un �nico paso del motor. La velocidad depender� del
 * tiempo entre llamadas sucesivas a esta funci�n.
 *
 * @param[in] dir
 * 		Direccion de giro:
 * 			-true  (1 / ON / HIGH)  -> Giro antihorario (Abrir)
 * 			-false (0 / OFF / LOW)  -> Giro horario (Cerrar)
 *
 * @note Esta funci�n es llamada desde el loop principal cada vez que debe ejecutarse un paso del motor.
 */
void step_move(bool_t dir)
{	
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, EN_GPIO_PORT, EN_GPIO_PIN, false); // LOW = habilitado
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, DIR_GPIO_PORT, DIR_GPIO_PIN, dir ? false : true); // dir=true -> LOW (antihorario), dir=false -> HIGH (horario)

    for (long i = 0; i < STEPS; i++) {
        step_pulse();
        delayInaccurateUs(STEP_PULSE_LOW);
    }
    Chip_GPIO_SetPinState(LPC_GPIO_PORT, EN_GPIO_PORT, EN_GPIO_PIN, true); // HIGH = deshabilitado
}

