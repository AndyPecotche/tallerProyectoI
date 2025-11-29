#include "MEF.h"
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

#define MAX_INTENTOS   99
#define TIMEOUT_MS     5000  // 20 segundos para ingresar PIN

#define OMITIR_SENSOR_CIERRE 1 // 1 = omitir chequeo de sensor de cierre (para pruebas)
#define OMITIR_MOTOR 1         // 1 = omitir control de motor (para pruebas)
#define SIMULAR_RFID_INTERRUPT 1 // 1 = simular interrupción de RFID con TEC2
#define SIMULAR_PRESENCIA_INTERRUPT 1 // 1 = simular interrupción de HUELLA con TEC3


// Configuración del sensor de cierre en GPIO0[1]
#define SENSOR_GPIO_PORT 0
#define SENSOR_GPIO_PIN  1

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
static void configurarInterrupcionPRESENCIA(void){
    Chip_PININT_Init(LPC_GPIO_PIN_INT);
    if (SIMULAR_PRESENCIA_INTERRUPT) {
        Chip_SCU_GPIOIntPinSel(0, 0, 4); // TEC3 -> GPIO0[4] //PRESENCIA interrumpe en pin GPIO0[12]
    } else {
        Chip_SCU_GPIOIntPinSel(0, 0, 12); // PRESENCIA -> GPIO0[12]
    }
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH0);
    Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH0);
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn);
    NVIC_EnableIRQ(PIN_INT0_IRQn);
}

static void configurarInterrupcionRFID(void){
    Chip_PININT_Init(LPC_GPIO_PIN_INT);
    if (SIMULAR_RFID_INTERRUPT) {
        // TEC2 -> GPIO0[8] //RFID interrumpe en pin GPIO2[5]
    } else {
        // RFID -> GPIO2[5]
    }
}

/* ---------------------------------------------------------------------------
   Inicialización de la MEF
--------------------------------------------------------------------------- */
void mefInit(void){
    boardConfig();
    tecladoInit();
    alertasInit();
    configurarInterrupcionPRESENCIA();
    configurarInterrupcionRFID();

    // Configurar GPIO0[1] como entrada para el sensor de cierre
    // Pin físico P0_1 mapeado a GPIO0[1] como entrada con buffer habilitado
    Chip_SCU_PinMux(0, 1, SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS, FUNC0);
    Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, SENSOR_GPIO_PORT, SENSOR_GPIO_PIN);

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
            if(eventoTeclado){
                printf("\r\n[EVENTO] Teclado activado\r\n");
                eventoTeclado = false;
                tecladoReset();
                tiempoInicio = tickRead();
                printf("\r\n[EVENTO] Ingrese PIN de 5 dígitos\r\n");
                estadoActual = LEER_PIN;
            }
            __WFI(); // bajo consumo?
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
            if (esPinMenu(pinIngresado)){
                printf("\r\n CODIGO DE ACCESO A MENU \r\n");
                estadoActual = MENU_ADMIN;
                break;
            }
            if (validarPin(pinIngresado)){
                  printf("\r\n[ACCESO] CORRECTO\r\n");
                  alertaExito();
                  intentosFallidos = 0;
                  printf("\r\n [MOTOR] Abriendo cerradura...\r\n");
                  if (!OMITIR_MOTOR) {
                      step_move(ON); //Gira 1 vuelta en sentido antihorario(Abrir)
                  } else {
                      printf("\r\n[OMITIR MOTOR] Simulando apertura de cerradura\r\n");
                  }
                  printf("\r\n [SISTEMA] Cerradura abierta \r\n");
                  //Esperamos cierre de cerradura antes de volver a REPOSO
                  estadoActual = SENSOR_CIERRE;
            } else {
               alertaError();
               intentosFallidos++;
               //printf("\r\n[ACCESO] DENEGADO (%d/%d)\r\n",intentosFallidos, MAX_INTENTOS);
               printf("\r\n[ACCESO] INCORRECTO \r\n");
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
		          // GPIO0[1] en bajo (false) indica que la puerta está cerrada
                  if (OMITIR_SENSOR_CIERRE)
                  {
                    printf("\r\n[OMITIR SENSOR] Simulando deteccion de cierre en 2 segundos...\r\n");
                    if (!OMITIR_MOTOR) {
                        delay(2000);
                        printf("\r\n [MOTOR] Cerrando cerradura...\r\n");
                        step_move(OFF);
                    } else {
                        printf("\r\n[OMITIR MOTOR] Simulando cierre de cerradura\r\n");
                    }                      
                    printf("\r\n [SISTEMA] Cerradura cerrada \r\n");
                    estadoActual = REPOSO;
                    break;
                  }
		          {
		              bool_t sensorState = Chip_GPIO_GetPinState(LPC_GPIO_PORT, SENSOR_GPIO_PORT, SENSOR_GPIO_PIN);
		              printf("\r\n[DEBUG] Estado sensor GPIO0[1]: %d (0=bajo/cerrado, 1=alto/abierto)\r\n", sensorState);
		              
		              if(!sensorState){
                        printf("\r\n[SENSOR] Puerta cerrada detectada\r\n");
                        if (!OMITIR_MOTOR) {
                            printf("\r\n [MOTOR] Cerrando cerradura...\r\n");
                            step_move(OFF);
                        } else {
                            printf("\r\n[OMITIR MOTOR] Simulando cierre de cerradura\r\n");
                        }
                        printf("\r\n [SISTEMA] Cerradura cerrada \r\n");
                        estadoActual = REPOSO;
		              } else {
		                  printf("\r\n [SENSOR] Esperando cierre de puerta...\r\n");
		              }
		              delay(500); // Delay para no saturar el puerto serie
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
