#ifndef MEF_H_
#define MEF_H_

#include <stdbool.h>
#include "stepperMotor.h"
#include "display.h"
#include "espAT.h"

/* ---------------------------------------------------------------------------
   Definición de estados de la MEF principal
--------------------------------------------------------------------------- */
typedef enum {
   REPOSO,
   ESPERANDO_ACCION,
   VALIDAR_RFID,
   VALIDAR,
   SENSOR_CIERRE,
   BLOQUEADO,
   MENU_ADMIN,
   REGISTRAR_RFID,
   REGISTRAR_HUELLA
} EstadoMEF_t;

/* ---------------------------------------------------------------------------
   Prototipos públicos
--------------------------------------------------------------------------- */
void mefInit(void);
void mefUpdate(void);

#endif /* MEF_H_ */
