#ifndef MEF_H_
#define MEF_H_

#include <stdbool.h>
#include "stepperMotor.h"
/* ---------------------------------------------------------------------------
   Definición de estados de la MEF principal
--------------------------------------------------------------------------- */
typedef enum {
   REPOSO,
   LEER_PIN,
   VALIDAR,
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
