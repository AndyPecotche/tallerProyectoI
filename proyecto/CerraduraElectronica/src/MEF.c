#include "MEF.h"
#include "sapi.h"
#include "teclado.h"
#include "validacion.h"
#include "alertas.h"
#include "espAT.h"
#include "as608.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
   Variables de control
--------------------------------------------------------------------------- */
static EstadoMEF_t estadoActual;
static char pinIngresado[6];
static int intentosFallidos = 0;

#define TIMEOUT_MS     20000  // N segundos para ingresar PIN

#define OMITIR_SENSOR_CIERRE 1 // 1 = omitir chequeo de sensor de cierre (para pruebas)
#define OMITIR_MOTOR 1         // 1 = omitir control de motor (para pruebas)
#define SIMULAR_RFID_INTERRUPT 1 // 1 = simular interrupción de RFID con TEC2
#define SIMULAR_PRESENCIA_INTERRUPT 1 // 1 = simular interrupción de presencia con TEC1


// Configuración del sensor de cierre en GPIO0[1]
#define SENSOR_GPIO_PORT 0
#define SENSOR_GPIO_PIN  1

bool_t open = false;

/* ---------------------------------------------------------------------------
   Eventos de interrupción
--------------------------------------------------------------------------- */
volatile bool eventoTeclado = false;     // Tecla presionada (simulada aparte si se requiere)
volatile bool eventoPresencia = false;   // Presencia detectada (GPIO0[12] o TEC3)
volatile bool eventoRFID = false;        // Pulso de RFID (GPIO2[5] o TEC2)

/* ---------------------------------------------------------------------------
   Modo de bajo consumo profundo
--------------------------------------------------------------------------- */
static void entrarModoSleep(void){
    static bool yaEnSleep = false;
    
    if(!yaEnSleep){
        printf("\r\n[SLEEP] Entrando en modo de bajo consumo...\r\n");
        printf("[SLEEP] Estado eventos - Presencia:%d RFID:%d\r\n", eventoPresencia, eventoRFID);
        delay(100); // Dar tiempo a que se transmita el mensaje
        
        // Deshabilitar SysTick para evitar despertares constantes (ahorro real de energía)
        SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
        
        yaEnSleep = true;
    }
    
    // Usar sleep mode NORMAL (no deep) para mantener clocks de GPIO/PININT activos
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    
    __WFI(); // Wait For Interrupt - ahora solo despierta con GPIO (presencia/RFID)
    printf("\r\n[SLEEP] Despertando de modo de bajo consumo...\r\n");
    // Verificar inmediatamente si hay eventos GPIO
    if(eventoPresencia || eventoRFID){
        // Rehabilitar SysTick al despertar
        SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
        
        printf("\r\n[SLEEP] Despertado - Presencia:%d RFID:%d\r\n", eventoPresencia, eventoRFID);
        yaEnSleep = false;
    }
}

/* Canal 0: Presencia (vector nombrado como GPIO0_IRQHandler en este BSP) */
void GPIO0_IRQHandler(void){
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
    eventoPresencia = true;
    printf("\r\n[INTERRUPCIÓN] Presencia detectada\r\n");
}

/* Canal 1: RFID interrupt */
void GPIO1_IRQHandler(void){
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);
    eventoRFID = true;
    printf("\r\n[INTERRUPCIÓN] RFID detectado\r\n");
}
/* ---------------------------------------------------------------------------
   Configuración de interrupción en TEC1
--------------------------------------------------------------------------- */
static void configurarInterrupcionPRESENCIA(void){
    Chip_PININT_Init(LPC_GPIO_PIN_INT);
    if (SIMULAR_PRESENCIA_INTERRUPT) {
        Chip_SCU_GPIOIntPinSel(0, 0, 4); // TEC1 -> GPIO0[4]
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
        Chip_SCU_GPIOIntPinSel(1, 0, 8); // Canal 1: TEC2 -> GPIO0[8]
    } else {
        Chip_SCU_GPIOIntPinSel(1, 2, 5); // Canal 1: RFID -> GPIO2[5]
    }
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);
    Chip_PININT_SetPinModeEdge(LPC_GPIO_PIN_INT, PININTCH1);
    Chip_PININT_EnableIntLow(LPC_GPIO_PIN_INT, PININTCH1);
    NVIC_ClearPendingIRQ(PIN_INT1_IRQn);
    NVIC_EnableIRQ(PIN_INT1_IRQn);
}

/* ---------------------------------------------------------------------------
   Simulación lectura RFID por SPI
--------------------------------------------------------------------------- */
static void leerRFID(void){
    printf("\r\n[RFID] leyendo RFID por SPI...\r\n");
    const char *codigoRFIDSimulado = "9876543210FF"; // Simulación
    if (validarRFID(codigoRFIDSimulado)){
        const char *tag = obtenerTagPorRFID(codigoRFIDSimulado);
        if (tag) {
            printf("\r\n[ACCESO] RFID válido - Bienvenido, %s\r\n", tag);
        } else {
            printf("\r\n[ACCESO] RFID válido\r\n");
        }
        alertaExito();
        intentosFallidos = 0;
        printf("\r\n [MOTOR] Abriendo cerradura...\r\n");
        if (!OMITIR_MOTOR) {
            step_move(ON);
        } else {
            printf("\r\n[OMITIR MOTOR] Simulando apertura de cerradura\r\n");
        }
        printf("\r\n [SISTEMA] Cerradura abierta \r\n");
        estadoActual = SENSOR_CIERRE;
    } else {
        printf("\r\n[ACCESO] RFID desconocido\r\n");
        alertaError();
        intentosFallidos++;
    }
}

/* ---------------------------------------------------------------------------
   Inicialización de la MEF
--------------------------------------------------------------------------- */
void mefInit(void){
    boardConfig();
    tecladoInit();
    alertasInit();
    //espATInit(115200);
    configurarInterrupcionPRESENCIA();
    configurarInterrupcionRFID();
    as608Init(0);
    as608SetDebug(1);// Activar nivel de debug para fingerprint (1 = básico, 2 = detallado)
    //sincronizarConServidor();
    // Configurar GPIO0[1] como entrada para el sensor de cierre
    // Pin físico P0_1 mapeado a GPIO0[1] como entrada con buffer habilitado
    Chip_SCU_PinMux(0, 1, SCU_MODE_INACT | SCU_MODE_INBUFF_EN | SCU_MODE_ZIF_DIS, FUNC0);
    Chip_GPIO_SetPinDIRInput(LPC_GPIO_PORT, SENSOR_GPIO_PORT, SENSOR_GPIO_PIN);

    estadoActual = LEER_PIN;
    printf("\r\n[SISTEMA] Cerradura electrónica iniciada.\r\n");
    //leerHuella(); // Prueba inicial de huella (deshabilitada para evitar spam)
}

/* ---------------------------------------------------------------------------
   Actualización de la MEF
--------------------------------------------------------------------------- */
void mefUpdate(void){
    //printf("\r . \r\n");
    static uint32_t tiempoInicio = 0;
    static uint32_t ultimaPresencia = 0;
    static EstadoMEF_t estadoPrevio = REPOSO;

    switch(estadoActual){

        case REPOSO:
            if(eventoPresencia || eventoRFID){
                if (eventoRFID){
                    eventoRFID = false;
                    estadoPrevio = REPOSO;
                    estadoActual = LEER_RFID;
                    break;
                }
                printf("\r\n[EVENTO] Presencia detectada\r\n");
                printf("\r\n[DEBUG] Llamando sincronizarConServidor()\r\n");
                bool okSync = sincronizarConServidor();
                printf("\r\n[DEBUG] Resultado sincronización: %s\r\n", okSync?"OK":"FALLÓ");
                eventoPresencia = false;
                tecladoReset();
                tiempoInicio = tickRead();
                ultimaPresencia = tiempoInicio;
                printf("\r\n[EVENTO] Ingrese PIN de 5 dígitos o '*' para menú\r\n");
                estadoActual = LEER_PIN;
            } else {
                // Solo dormir si no hay eventos pendientes
                entrarModoSleep();
            }
            break;

        case LEER_PIN:
            delay(40);
            // Refresh timeout on presence pulses
            if (eventoPresencia){
                eventoPresencia = false;
                ultimaPresencia = tickRead();
                printf("\r\n[DEBUG] Timeout reiniciado por presencia\r\n");
            }
            // If RFID arrives during PIN input, process immediately
            if (eventoRFID){
                eventoRFID = false;
                estadoPrevio = LEER_PIN;
                estadoActual = LEER_RFID;
                break;
            }
            // Poll huella con intervalo para no saturar UART
            {
                static uint32_t ultimoIntentoHuella = 0;
                const uint32_t INTERVALO_HUELLA_MS = 1000; // escaneo huella cada 1000ms para menor carga UART
                if (tickRead() - ultimoIntentoHuella >= INTERVALO_HUELLA_MS){
                    ultimoIntentoHuella = tickRead();
                    uint16_t id=0, score=0;
                    if (as608PollHuella(&id, &score)){
                        char idStr[5];
                        snprintf(idStr, sizeof(idStr), "%04u", (unsigned)id);
                        printf("\r\n[HUELLA] Dedo detectado (ID bruto=%s score=%u)\r\n", idStr, (unsigned)score);
                        // Centraliza mensaje de bienvenida en validacion
                        if (validarHuella(idStr)){
                            alertaExito();
                            intentosFallidos = 0;
                            printf("\r\n [MOTOR] Abriendo cerradura...\r\n");
                            if (!OMITIR_MOTOR) {
                                step_move(ON);
                            } else {
                                printf("\r\n[OMITIR MOTOR] Simulando apertura de cerradura\r\n");
                            }
                            printf("\r\n [SISTEMA] Cerradura abierta \r\n");
                            estadoActual = SENSOR_CIERRE;
                            break;
                        } else {
                            printf("\r\n[ACCESO] Huella no registrada (ID=%s)\r\n", idStr);
                        }
                    }
                }
            }
            // Teclado: si completa 5 dígitos, validar
            // tecladoLeerPin internamente resetea el timeout en cada tecla
            int resultado = tecladoLeerPin(pinIngresado, &ultimaPresencia);
            if (resultado == 1){
                // PIN completo, validar
                estadoActual = VALIDAR;
            } else if (resultado == -1){
                // Tecla '*' presionada, ir a menú
                estadoActual = MENU_ADMIN;
            } else if (tickRead() - ultimaPresencia > TIMEOUT_MS){
                printf("\r\n[TIMEOUT] Sin actividad por %d ms, durmiendo\r\n", TIMEOUT_MS);
                estadoActual = REPOSO;
            }
            break;

        case LEER_RFID:
            // Lógica de lectura/validación RFID en un estado dedicado
            // Reiniciar timeout al procesar RFID
            ultimaPresencia = tickRead();
            leerRFID();
            // Si la lectura otorgó acceso, leerRFID cambió el estado a SENSOR_CIERRE
            if (estadoActual == SENSOR_CIERRE) {
                break;
            }
            // Si no validó, retomamos el flujo anterior
            estadoActual = LEER_PIN;
            break;
         
        case VALIDAR:
            if (validarPin(pinIngresado)){
                  const char *tag = obtenerTagPorPin(pinIngresado);
                  if (tag) {
                      printf("\r\n[ACCESO] CORRECTO - Bienvenido, %s\r\n", tag);
                  } else {
                      printf("\r\n[ACCESO] CORRECTO\r\n");
                  }
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
                tecladoReset();
                estadoActual = LEER_PIN;
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
                    estadoActual = LEER_PIN;
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
                        estadoActual = LEER_PIN;
		              } else {
		                  printf("\r\n [SENSOR] Esperando cierre de puerta...\r\n");
		              }
		              delay(500); // Delay para no saturar el puerto serie
		          }
            break;
            
         case MENU_ADMIN:
                printf("\r\n[ADMIN] OPCIONES: 1=RFID | 2=Registrar huella | 3=Reset WiFi | 4=Leer huella\r\n");
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
                case '3':
                    printf("\r\n[ADMIN] Resetear credenciales WiFi...\r\n");
                    if(resetearCredencialesESP()){
                        printf("\r\n[ADMIN] Esperando reconexión ESP...\r\n");
                        // Esperar que el ESP se reinicie
                        delay(3000);
                        // Reinicializar comunicación
                        inicializarESP(60000);
                        printf("\r\n[ADMIN] Use la app ESP BluFi para configurar WiFi\r\n");
                    } else {
                        printf("\r\n[ADMIN] Error al resetear ESP\r\n");
                    }
                    estadoActual = LEER_PIN;
                    break;
                case '4':
                    printf("\r\n[ADMIN] Lectura huella se realiza en paralelo al PIN\r\n");
                    estadoActual = LEER_PIN;
                    break;
                default:
                    printf("\r\n[ADMIN] Opción inválida\r\n");
                    estadoActual = LEER_PIN;
                    break;
            }
         break;
        // Se elimina LEER_HUELLA como estado separado; se integra polling en LEER_PIN

        case REGISTRAR_RFID: {
            printf("\r\n[ADMIN] Registrar nuevo RFID\r\n");
            printf("Ingrese PIN válido asociado al usuario:\r\n");

            char pinValidado[6] = {0};
            uint32_t actividadAdmin = tickRead();
            while (1) {
                int res = tecladoLeerPin(pinValidado, &actividadAdmin);
                if (res == 1) break; // PIN completo
                // Mantener vivo el contador de inactividad general mientras se ingresa admin PIN
                ultimaPresencia = actividadAdmin;
                delay(50);
            }

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

            estadoActual = LEER_PIN;
            break;
        }

        case REGISTRAR_HUELLA: {
            printf("\r\n[ADMIN] Registrar nueva huella\r\n");
            printf("Ingrese PIN válido asociado al usuario:\r\n");

            char pinValidado[6] = {0};
            uint32_t actividadAdmin = tickRead();
            while (1) {
                int res = tecladoLeerPin(pinValidado, &actividadAdmin);
                if (res == 1) break; // PIN completo
                // Mantener vivo el contador de inactividad general mientras se ingresa admin PIN
                ultimaPresencia = actividadAdmin;
                delay(50);
            }

            if (validarPin(pinValidado)) {
                printf("\r\n[ADMIN] Iniciando ENROLL de huella para PIN %s\r\n", pinValidado);
                char idHuella[5] = {0};
                if (as608Enroll(idHuella)) {
                    if (asociarHuellaaPin(pinValidado, idHuella)) {
                        printf("\r\n[ADMIN] Huella asociada al PIN %s (ID=%s)\r\n", pinValidado, idHuella);
                    } else {
                        printf("\r\n[ERROR] No se pudo asociar la huella al PIN\r\n");
                    }
                } else {
                    printf("\r\n[ERROR] Falló el ENROLL de la huella\r\n");
                }
            } else {
                printf("\r\n[ERROR] PIN inválido\r\n");
            }

            estadoActual = LEER_PIN;
            break;
        }
    }
}
