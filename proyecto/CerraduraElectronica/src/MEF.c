#include "MEF.h"
#include "sapi.h"
#include "teclado.h"
#include "validacion.h"
#include "alertas.h"
#include "espAT.h"
#include "as608.h"
#include "mef_config.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ---------------------------------------------------------------------------
   Variables de control
--------------------------------------------------------------------------- */
static EstadoMEF_t estadoActual;
static char pinIngresado[6];
static int intentosFallidos = 0;

bool_t open = false;

/* ---------------------------------------------------------------------------
   Eventos de interrupción
--------------------------------------------------------------------------- */
volatile bool eventoTeclado = false;     // Tecla presionada (simulada aparte si se requiere)
volatile bool eventoPresencia = false;   // Presencia detectada (GPIO0[12] o TEC3)
volatile bool eventoRFID = false;        // Pulso de RFID (GPIO2[5] o TEC2)

/* ---------------------------------------------------------------------------
   Modo de bajo consumo
--------------------------------------------------------------------------- */
static void entrarModoSleep(void){
    printf("\r\n[SLEEP] Entrando en modo de bajo consumo...\r\n");
    delay(100); // Dar tiempo a que se transmita el mensaje
    
    // Deshabilitar SysTick para evitar despertares constantes (ahorro real de energía)
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    
    // Usar sleep mode NORMAL (no deep) para mantener clocks de GPIO/PININT activos
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    
    __WFI(); // Wait For Interrupt - despierta con GPIO (presencia/RFID)
    
    // Rehabilitar SysTick al despertar
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    
    printf("\r\n[SLEEP] Despertando...\r\n");
    sincronizarConServidor(); // Sincronizar al despertar
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
   Inicialización de la MEF
--------------------------------------------------------------------------- */
void mefInit(void){
    boardConfig();
    driverConfig();
    configurarPines();
    tecladoInit();
    if (ALERTAS_ENABLE) alertasInit();
    if (SENSOR_WIFI_ENABLE) { espATInit(115200); sincronizarConServidor(); }
    if (SENSOR_HUELLA_ENABLE) {as608Init(0); as608SetDebug(1); as608ScanReset();}
                        // Nivel de debug fingerprint (1=básico, 2=detallado) // Preparar mini MEF del AS608
    configurarInterrupcionPRESENCIA();
    configurarInterrupcionRFID();
    printf("\r\n[SISTEMA] Cerradura electrónica iniciada.\r\n");
    estadoActual = ESPERANDO_ACCION;
}

/* ---------------------------------------------------------------------------
   Actualización de la MEF
--------------------------------------------------------------------------- */
void mefUpdate(void){
    //printf("\r . \r\n");
    static uint32_t ultimaPresencia = 0;

    switch(estadoActual){

        case REPOSO:
            // Dormir esperando eventos GPIO (presencia/RFID)
            entrarModoSleep();
            // Al despertar, ir a ESPERANDO_ACCION que maneja los eventos
            estadoActual = ESPERANDO_ACCION;
            break;

        case ESPERANDO_ACCION:
            // Refresh timeout on presence pulses
            if (eventoPresencia){
                eventoPresencia = false;
                ultimaPresencia = tickRead();
                printf("\r\n[DEBUG] Timeout reiniciado por presencia\r\n");
            }
            // If RFID arrives during PIN input, process immediately
            if (eventoRFID){
                eventoRFID = false;
                estadoActual = LEER_RFID;
                break;
            }
            // Mini MEF del AS608: pasos breves en cada ciclo
            {
                static uint32_t agendaHuella = 0;
                static bool escaneoActivo = false;
                const uint32_t INTERVALO_HUELLA_MS = 600;

                // Programar inicio de escaneo cada cierto intervalo
                if (!escaneoActivo && (tickRead() - agendaHuella) >= INTERVALO_HUELLA_MS){
                    as608ScanReset();
                    escaneoActivo = true;
                    agendaHuella = tickRead();
                }

                if (escaneoActivo){
                    uint16_t id=0, score=0;
                    as608ScanStatus_t st = as608ScanStep(&id, &score);
                    if (st == AS608_SCAN_MATCH){
                        char idStr[5];
                        snprintf(idStr, sizeof(idStr), "%04u", (unsigned)id);
                        if (validarHuella(idStr)){
                            alertaExito();
                            intentosFallidos = 0;
                            if (!OMITIR_MOTOR) {
                                abrirCerradura();
                            } else {
                                printf("\r\n[OMITIR MOTOR] Simulando apertura de cerradura\r\n");
                            }
                            estadoActual = SENSOR_CIERRE;
                            escaneoActivo = false;
                            break;
                        } else {
                            printf("\r\n[ACCESO] Huella no registrada (ID=%s)\r\n", idStr);
                            escaneoActivo = false;
                        }
                    } else if (st == AS608_SCAN_NOMATCH || st == AS608_SCAN_ERROR){
                        escaneoActivo = false; // liberar hasta el próximo intento programado
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
                if (!OMITIR_MOTOR) {
                    abrirCerradura();
                } else {
                    printf("\r\n[OMITIR MOTOR] Simulando apertura de cerradura\r\n");
                }
                estadoActual = SENSOR_CIERRE;
            } else {
                printf("\r\n[ACCESO] RFID desconocido\r\n");
                alertaError();
                intentosFallidos++;
            }
            // Si la lectura otorgó acceso, leerRFID cambió el estado a SENSOR_CIERRE
            if (estadoActual == SENSOR_CIERRE) {
                break;
            }
            // Si no validó, retomamos el flujo anterior
                estadoActual = ESPERANDO_ACCION;
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
                  if (!OMITIR_MOTOR) {
                      abrirCerradura();
                  } else {
                      printf("\r\n[OMITIR MOTOR] Simulando apertura de cerradura\r\n");
                  }
                  //Esperamos cierre de cerradura antes de volver a REPOSO
                  estadoActual = SENSOR_CIERRE;
            } else {
               alertaError();
               intentosFallidos++;
               //printf("\r\n[ACCESO] DENEGADO (%d/%d)\r\n",intentosFallidos, MAX_INTENTOS);
               printf("\r\n[ACCESO] INCORRECTO \r\n");
               tecladoReset();
               estadoActual = ESPERANDO_ACCION;
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
                        cerrarCerradura();
                    } else {
                        printf("\r\n[OMITIR MOTOR] Simulando cierre de cerradura\r\n");
                    }                      
                        estadoActual = ESPERANDO_ACCION;
                    break;
                  }
		          {
		              bool_t sensorState = Chip_GPIO_GetPinState(LPC_GPIO_PORT, SENSORHALL_GPIO_PORT, SENSORHALL_GPIO_PIN);
		              printf("\r\n[DEBUG] Estado sensor GPIO0[1]: %d (0=bajo/cerrado, 1=alto/abierto)\r\n", sensorState);
		              
		              if(!sensorState){
                        printf("\r\n[SENSOR] Puerta cerrada detectada\r\n");
                        if (!OMITIR_MOTOR) {
                            cerrarCerradura();
                        } else {
                            printf("\r\n[OMITIR MOTOR] Simulando cierre de cerradura\r\n");
                        }
                            estadoActual = ESPERANDO_ACCION;
		              } else {
		                  printf("\r\n [SENSOR] Esperando cierre de puerta...\r\n");
		              }
		              delay(500); // Delay para no saturar el puerto serie
		          }
            break;
            
         case MENU_ADMIN:
                printf("\r\n[ADMIN] OPCIONES: 1=RFID | 2=Registrar huella | 3=Reset WiFi | 4=Limpiar huellas sensor \r\n");
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
                    estadoActual = ESPERANDO_ACCION;
                    break;
                    case '4':
                        printf("\r\n[ADMIN] Limpiando todas las huellas del sensor...\r\n");
                        if (as608ClearAllTemplates()) {
                            printf("\r\n[ADMIN] Huellas del sensor borradas correctamente\r\n");
                        } else {
                            printf("\r\n[ADMIN] Error al borrar huellas del sensor\r\n");
                        }
                        estadoActual = ESPERANDO_ACCION;
                        break;
                default:
                    printf("\r\n[ADMIN] Opción inválida\r\n");
                    estadoActual = ESPERANDO_ACCION;
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

            estadoActual = ESPERANDO_ACCION;
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
                bool finalizar = false;
                while(!finalizar){
                    printf("\r\n[ADMIN] Iniciando ENROLL de huella para PIN %s\r\n", pinValidado);
                    
                    // Obtener ID objetivo desde base de usuarios
                    char idObjetivoStr[5] = {0};
                    if(!obtenerHuellaAsignadaPorPin(pinValidado, idObjetivoStr)){
                        printf("\r\n[ERROR] El PIN no tiene ID de huella asignado; asigne uno en el servidor/app\r\n");
                        finalizar = true;
                        break;
                    }
                    
                    // Convertir a entero
                    uint16_t idObjetivo = (uint16_t)strtoul(idObjetivoStr, NULL, 10);
                    char idHuella[5] = {0};
                    if (as608EnrollAtId(idObjetivo, idHuella)) {
                        if (asociarHuellaaPin(pinValidado, idHuella)) {
                            printf("\r\n[ADMIN] Huella asociada al PIN %s (ID=%s)\r\n", pinValidado, idHuella);
                        } else {
                            printf("\r\n[ERROR] No se pudo asociar la huella al PIN\r\n");
                        }
                        finalizar = true; // Éxito, salir del bucle
                    } else {
                        printf("\r\n[ERROR] Falló el ENROLL de la huella\r\n");
                        printf("\r\n[ADMIN] Reintentar? 1=Sí 2=Salir\r\n");
                        char opcionRetry = 0;
                        uint32_t tRetry = tickRead();
                        while(!opcionRetry){
                            char t;
                            if (tecladoLeerTecla(&t)){
                                if(t=='1' || t=='2') opcionRetry = t;
                            }
                            if((tickRead() - tRetry) > 30000){ // timeout opcional 30s
                                opcionRetry = '2';
                            }
                            delay(60);
                        }
                        if(opcionRetry == '1'){
                            printf("\r\n[ADMIN] Reintentando ENROLL...\r\n");
                            // loop continúa
                        } else {
                            printf("\r\n[ADMIN] Cancelado por usuario\r\n");
                            finalizar = true;
                        }
                    }
                }
            } else {
                printf("\r\n[ERROR] PIN inválido\r\n");
            }

            estadoActual = ESPERANDO_ACCION;
            break;
        }
    }
}
