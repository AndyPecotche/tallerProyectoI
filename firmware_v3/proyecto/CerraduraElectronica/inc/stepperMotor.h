#ifndef __STEPPERMOTOR_H__
#define __STEPPERMOTOR_H__

#include <sapi.h>

/**Mapeo de puertos (EDU-CIAA-NXP → DRV8825)
 * EDU-CIAA    → DRV8825     → Función
 * ------------------------------------
 * GPIO8       → STEP        → Pulso de avance
 * GPIO4       → DIR         → Selección de dirección
 * GPIO0       → EN (ENABLE) → Habilitación del driver (activo en LOW)
 */
#define STEP_PIN  GPIO8
#define DIR_PIN   GPIO4
#define EN_PIN    GPIO0

/**Pasos por revolución del motor paso a paso (200 pasos = 1 vuelta completa)
 *
 * La mayoría de los motores NEMA 17 tienen 200 pasos completos por vuelta, lo que equivale a 1.8° por paso (360° / 200).
 *
 * Nota: Este valor corresponde a paso completo (full-step).
 * 		 Si se activa microstepping en el DRV8825, la cantidad de pasos por vuelta efectiva será: 200 * (factor de microstepping).
 */
#define STEPS 200

/**Variables para definir el pulso en microsegundos
 * STEP_PULSE_HIGH + STEP_PULSE_LOW = periodo del pulso(T)
 * 1/T = frecuencia del pulso(f)
 * f alta = T corto = motor rapido
 * f baja = T largo = motor lento
 */
#define STEP_PULSE_HIGH 5U //Duracion del pulso alto(2us a 10us)
#define STEP_PULSE_LOW 2500U //Duracion del pulso bajo --> Controla la velocidad de giro.


void driverConfig(void);
void step_move(bool_t);

#endif /* __STEPPERMOTOR_H__ */
