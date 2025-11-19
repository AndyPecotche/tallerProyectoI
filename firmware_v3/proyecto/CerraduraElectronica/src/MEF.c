#include "mef.h"
#include "sapi.h"
#include "teclado.h"
#include "validacion.h"
#include "alertas.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
   Variables de control
--------------------------------------------------------------------------- */
static EstadoMEF_t estadoActual;
static char pinIngresado[6];
static int intentosFallidos = 0;

#define MAX_INTENTOS   3
#define TIMEOUT_MS     10000  // 10 segundos para ingresar PIN
#define SENSOR_PIN GPIO5
bool_t open = false;

/* ---------------------------------------------------------------------------
   Interrupción simulada: TEC1 activa el modo teclado
--------------------------------------------------------------------------- */
volatile bool eventoTeclado = false;

void GPIO0_IRQHandler(void){
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
    eventoTeclado = true;
}

/* ---------------------------------------------------------------------------
   Configuración de interrupción en TEC1
--------------------------------------------------------------------------- */
static void configurarInterrupcionTEC1(void){
    Chip_PININT_Init(LPC_GPIO_PIN_INT);
    Chip_SCU_GPIOIntPinSel(0, 0, 4); // TEC1 -> GPIO0[4]
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);
    Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH0);
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn);
    NVIC_EnableIRQ(PIN_INT0_IRQn);
}

/* ---------------------------------------------------------------------------
   Inicialización de la MEF
--------------------------------------------------------------------------- */
void mefInit(void){
   boardConfig();
   tecladoInit();
   alertasInit();
   configurarInterrupcionTEC1();
   gpioConfig(SENSOR_PIN, GPIO_INPUT_PULLUP);
   estadoActual = REPOSO;
   intentosFallidos = 0;
   printf("\r\n[SISTEMA] Cerradura electrónica iniciada.\r\n");
}

/* ---------------------------------------------------------------------------
   Actualización de la MEF
--------------------------------------------------------------------------- */
void mefUpdate(void){
    static uint32_t tiempoInicio = 0;

    switch(estadoActual){

        case REPOSO:
            if((eventoTeclado){
                eventoTeclado = false;
                tecladoReset();
                tiempoInicio = tickRead();
                printf("\r\n[EVENTO] Ingrese PIN de 5 dígitos\r\n");
                estadoActual = LEER_PIN;
            }
         
            __WFI(); // bajo consumo
            break;

        case LEER_PIN:
           delay(40);
            if (tecladoLeerPin(pinIngresado)){
                estadoActual = VALIDAR;
            } else if (tickRead() - tiempoInicio > TIMEOUT_MS){
                printf("\r\n[TIMEOUT] Tiempo agotado\r\n");
                estadoActual = REPOSO;
            }
            break;
         
        case VALIDAR:
            if (validarPin(pinIngresado)){
               if (esPinMaestro(pinIngresado)){
                  printf("\r\n PIN MAESTRO\r\n");
                  estadoActual = MENU_ADMIN;
               } else {
                  printf("\r\n[ACCESO] CORRECTO\r\n");
                  alertaExito();
                  intentosFallidos = 0;
                  step_move(ON); //Gira 1 vuelta en sentido antihorario(Abrir)
                  estadoActual = SENSOR_CIERRE;
               }
            } else {
               alertaError();
               intentosFallidos++;
               printf("\r\n[ACCESO] DENEGADO (%d/%d)\r\n",intentosFallidos, MAX_INTENTOS);
               tiempoInicio = tickRead();
               if(intentosFallidos >= MAX_INTENTOS){
                  printf("\r\n[BLOQUEO] 3 intentos mal - Cerradura bloqueada temporalmente\r\n");
                  estadoActual = BLOQUEADO;
               } else {
                  tecladoReset();
                  estadoActual = LEER_PIN;
               }
            }
            break;
   
         case SENSOR_CIERRE:
               //Checkea el sensor y cierra en caso que este en bajo.
		          if(!gpioRead(SENSOR_PIN)){
			           step_move(OFF); //Gira 1 vuelta en sentido horario(Cerrar)
                    estadoActual = REPOSO;
		          }
            break;
   
         case BLOQUEADO:
            if(tickRead() - tiempoInicio > 10000){
                printf("\r\n[SISTEMA] Bloqueo finalizado\r\n");
                intentosFallidos = 0;
                estadoActual = REPOSO;
            }
         break;
            
         case MENU_ADMIN:
            printf("\r\n[ADMIN] OPCIONES: 1 = Nuevo RFID | 2 = Nueva huella\r\n");
            char opcion = 0;

            while (!opcion) {
                char tecla;
                if (tecladoLeerTecla(&tecla)) {
                    opcion = tecla;
                }
                delay(60);
            }

            switch (opcion) {
                case '1':
                    estadoActual = REGISTRAR_RFID;
                    break;
                case '2':
                    estadoActual = REGISTRAR_HUELLA;
                    break;
                default:
                    printf("\r\n[ADMIN] Opción inválida\r\n");
                    estadoActual = REPOSO;
                    break;
            }
         break;

        case REGISTRAR_RFID: {
            printf("\r\n[ADMIN] Registrar nuevo RFID\r\n");
            printf("Ingrese PIN válido asociado al usuario:\r\n");

            char pinValidado[6] = {0};
            while (!tecladoLeerPin(pinValidado)) delay(50);

            if (validarPin(pinValidado)) {
                printf("\r\nIngrese código RFID (simulado):\r\n");
                //char nuevoRFID[20];
                //scanf("%s", nuevoRFID); // simulado desde consola UART
                //asociarRFIDaPin(pinValidado, nuevoRFID);
                delay(100);
                printf("\r\nRFID GUARDADO\r\n");
            } else {
                printf("\r\n[ERROR] PIN inválido\r\n");
            }

            estadoActual = REPOSO;
            break;
        }

        case REGISTRAR_HUELLA: {
            printf("\r\n[ADMIN] Registrar nueva huella\r\n");
            printf("Ingrese PIN válido asociado al usuario:\r\n");

            char pinValidado[6] = {0};
            while (!tecladoLeerPin(pinValidado)) delay(50);

            if (validarPin(pinValidado)) {
                printf("\r\nApoye la huella en el lector (simulado)\r\n");
               // char nuevaHuella[20];
               // scanf("%s", nuevaHuella); // simulado desde consola UART
               // asociarHuellaaPin(pinValidado, nuevaHuella);
                printf("\r\nHUELLA GUARDADO\r\n");
            } else {
                printf("\r\n[ERROR] PIN inválido\r\n");
            }

            estadoActual = REPOSO;
            break;
        }
    }
}
