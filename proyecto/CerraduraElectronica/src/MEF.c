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
#include <stdlib.h>
#include <stdint.h>
//#include "rc522.h"
#include "rc522_drv.h"

/* ---------------------------------------------------------------------------
   Variables de control
--------------------------------------------------------------------------- */
static EstadoMEF_t estadoActual;
static char pinIngresado[6];
static int intentosFallidos = 0;
static long int ultimaPresencia = 0;
static double ultimaVersionConocida = 0.0;
char rfid[10];

bool_t open = false;

//Variables para la configuracion del escaneo de huellas
static long int agendaHuella = 0;
static bool escaneoActivo = false;
const long int INTERVALO_HUELLA_MS = 600;

/* ---------------------------------------------------------------------------
   Eventos de interrupción
--------------------------------------------------------------------------- */

volatile bool eventoPresencia = false;   // Presencia detectada (GPIO0[12] o TEC3)
volatile bool eventoRFID = false;        // Pulso de RFID (GPIO2[5] o TEC2)

/* ---------------------------------------------------------------------------
   Modo de bajo consumo
--------------------------------------------------------------------------- */
static void entrarModoSleep(void){
    //printf("\r\n[SLEEP] Entrando en modo de bajo consumo...\r\n");
	display_clear();
	display_println("ENTRANDO EN MODO", 10);
	display_println("BAJO CONSUMO", 30);
	display_update();
    delay(100); // Dar tiempo a que se transmita el mensaje
    
    eventoPresencia = false;
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
    NVIC_ClearPendingIRQ(PIN_INT0_IRQn);

    NVIC_EnableIRQ(PIN_INT0_IRQn);

    // Deshabilitar SysTick para evitar despertares constantes (ahorro real de energía)
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    
    // Usar sleep mode NORMAL (no deep) para mantener clocks de GPIO/PININT activos
    SCB->SCR &= ~SCB_SCR_SLEEPDEEP_Msk;
    display_clear();
   	display_println("MODO BAJO", 10);
   	display_println("CONSUMO ACTIVADO", 30);
   	display_update();
    
    __WFI(); // Wait For Interrupt - despierta con GPIO (presencia/RFID)

	NVIC_DisableIRQ(PIN_INT0_IRQn);
    // Rehabilitar SysTick al despertar
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    display_clear();
	display_println("DESPERTANDO...", 10);
	display_update();
    //printf("\r\n[SLEEP] Despertando...\r\n");
	delay(500);
	display_clear();
	display_println("VERIFICANDO", 10);
	display_println("ACTUALIZACIONES...", 30);
	display_update();
	double versionServer = espObtenerTiempoServidor();
	// 2. �Fall�? -> Intentamos revivir el WiFi
	if(versionServer < 0) {
		printf("\r\n[SLEEP] WiFi parece desconectado. Reintentando...\r\n");
		display_clear();
		display_println("RECONECTANDO", 10);
		display_println("WIFI...", 30);
		display_update();

		// Llamamos a tu funci�n blindada que fuerza la conexi�n
		// Si logra conectar, nos devolver� true
		if (inicializarESP(15000)) {
			// �Conectado! Intentamos pedir la versi�n de nuevo
			versionServer = espObtenerTiempoServidor();
		}
	}
	// Si devuelve -1 es error de conexi�n
	if(versionServer < 0) {
		 display_println("ERROR RED", 40);
		 display_println("SIN CONEXION", 50);
		 display_update();
		 delay(2000);
		 display_clear();
		 return;
	}
	// 2. Comparar
	// Si la versi�n del servidor es MAYOR, significa que hubo cambios.
	else if (versionServer > ultimaVersionConocida) {
		display_clear();
		display_println("DESCARGANDO", 10);
		display_println("DATOS NUEVOS...", 22);
		display_update();
		 delay(1000);
		unsigned char valor = sincronizarConServidor(); // Sincronizar al despertar
		if (valor == 0)
			ultimaVersionConocida = versionServer;
		delay(1000);

	} else {
		// 3. Si son iguales, no hacemos nada
		printf("\r\n[SYNC] Sistema ya actualizado. Saltando descarga.\r\n");
		display_clear();
		display_println("SISTEMA", 10);
		display_println("ACTUALIZADO", 30);
		display_update();
		delay(1000); // Peque�a pausa para que el usuario vea que est� todo OK
	}
}

/* Canal 0: Presencia (vector nombrado como GPIO0_IRQHandler en este BSP) */
void GPIO0_IRQHandler(void){
    Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH0);
    eventoPresencia = true;
    printf("\r\n[INTERRUPCIÓN] Presencia detectada\r\n");
}

///* Canal 1: RFID interrupt */
// void PIN_INT1_IRQHandler(void){
// 	// 1. Limpiamos la marca de interrupci�n para poder recibir la siguiente
//     Chip_PININT_ClearIntStatus(LPC_GPIO_PIN_INT, PININTCH1);
//     // 2. Avisamos al programa principal
//     eventoRFID = true;
//     //display_clear();
//     //display_println("RFID DETECTADO", 30);
//     //display_update();
// 	printf("\r\n[INTERRUPCIÓN] RFID detectado\r\n");
// }



/* ---------------------------------------------------------------------------
   Inicialización de la MEF
--------------------------------------------------------------------------- */
void mefInit(void){
    configurarPines();
    // Inicializar el lector RFID
    //PCD_Init();
    //PCD_ActivarIRQ();
    //configurarInterrupcionRFID();

    if (ALERTAS_ENABLE) alertasInit();
    if (SENSOR_WIFI_ENABLE) {
    	display_clear();
		display_println("CONECTANDO WIFI...", 20);
		display_update();
    	espATInit(115200);
    	while(!conectado){
			display_clear();
			display_println("CONECTANDO WIFI...", 20);
			display_update();
			inicializarESP(10000);
    	}
    	sincronizarConServidor();
    }
    if (SENSOR_HUELLA_ENABLE) {as608Init(0); as608SetDebug(1); as608ScanReset();}
                        // Nivel de debug fingerprint (1=básico, 2=detallado) // Preparar mini MEF del AS608
    configurarInterrupcionPRESENCIA();
    NVIC_DisableIRQ(PIN_INT0_IRQn);

    // --- BLOQUE DE DIAGN�STICO ---
   	//printf("\r\n[DIAGNOSTICO] Verificando hardware RC522...\r\n");

   	// Leemos el registro de versi�n (Direcci�n 0x37)
   	//uint8_t version = PCD_ReadRegister(VersionReg);

   	//printf("[DIAGNOSTICO] Version Hex: 0x%02X\r\n", version);

   	// if (version == 0x00 || version == 0xFF) {
   	// 	printf("[ERROR CRITICO] EL MICRO NO VE AL SENSOR.\r\n");
   	// 	printf("CAUSAS: Cables sueltos, MISO/MOSI invertidos o falta energia.\r\n");
   	// 	while(1); // Bloqueamos aqu� porque no tiene sentido seguir
   	// } else {
   	// 	printf("[EXITO] Comunicacion SPI Correcta. Chip detectado.\r\n");
   	// }
   	// -----------------------------
	//Inicializaciion RFID
	printf("[INICIALIZACION] Iniciando RC522...\r\n");
	rc522_hw_init();
  	rc522_init_iso14443a();

    display_clear();
    display_println("CERRADURA ELECTRONICA", 20);
    display_println("INICIADA", 35);
    display_update();
    //printf("\r\n[SISTEMA] Cerradura electrónica iniciada.\r\n");
    delay(1000);
    display_clear();
    ultimaPresencia = tickRead();
    estadoActual = ESPERANDO_ACCION;
}

/* ---------------------------------------------------------------------------
   Actualización de la MEF
--------------------------------------------------------------------------- */
void mefUpdate(void){

    switch(estadoActual){

        case REPOSO:
            // Dormir esperando eventos GPIO (presencia/RFID)
            entrarModoSleep();
            // Al despertar, ir a ESPERANDO_ACCION que maneja los eventos
            display_clear();
            estadoActual = ESPERANDO_ACCION;
            break;

        case ESPERANDO_ACCION:{
            // Refresh timeout on presence pulses
            if (eventoPresencia){
                eventoPresencia = false;
                ultimaPresencia = tickRead();
                printf("\r\n[DEBUG] Timeout reiniciado por presencia\r\n");
            }

            // Teclado: si completa 5 dígitos, validar
			// tecladoLeerPin internamente resetea el timeout en cada tecla
			//display_clear();
			display_println("SISTEMA DE ACCESO", 0);
			display_println("INGRESE PIN:", 18);
			//display_println("cerradura cerrada", 50);
			display_update();
			int resultado = tecladoLeerPin(pinIngresado, &ultimaPresencia);
			if (resultado == 1){ // PIN completo, pasar a validar
				ultimaPresencia = tickRead(); 
				estadoActual = VALIDAR;
				break;
			} else if (resultado == -1){ // Acceso a menú admin por tecla '*'
					display_clear();
					display_println("SEGURIDAD", 10);
					display_println("PIN ADMIN?", 22);
					display_update();
					char pinAdmin[6] = {0};
					long int tAuth = tickRead();
					bool accesoConcedido = false;
					tecladoReset();
					while((tickRead() - tAuth) < 10000) {
						 // Reutilizamos tu funci�n de lectura
						 int res = tecladoLeerPin(pinAdmin, &tAuth);
						 if (res == 1) {
							 // El usuario termin� de escribir (#)
							 if (esPinMaster(pinAdmin)) {
								 accesoConcedido = true;
							 }
							 break; // Salimos del bucle
						 }
						 if (res == -1) {
							 // Si presiona * de nuevo, cancelamos
							 break;
						 }
						 delay(50);
					}

					// 4. Evaluar resultado
					if (accesoConcedido) {
						display_clear();
						display_println("ACCESO CONCEDIDO", 20);
						display_update();
						alertaExito();
						delay(1000);
						ultimaPresencia = tickRead();
						estadoActual = MENU_ADMIN;
					} else {
						display_clear();
						display_println("PIN INCORRECTO", 20);
						display_update();
						alertaError();
						delay(1500);

						// Limpiamos y volvemos al estado normal
						display_clear();
						tecladoReset();
					}
					break;

				} else if (tickRead() - ultimaPresencia > TIMEOUT_MS){
					printf("\r\n[TIMEOUT] Sin actividad por %d ms, durmiendo\r\n", TIMEOUT_MS);
					estadoActual = REPOSO;
			}
			
			// RFID: si detecta tarjeta, pasar a validar

			if (leer_rfid_str(rfid, sizeof(rfid))) {
				printf("\r\n[RFID] Leido: %s\r\n", rfid);
				ultimaPresencia = tickRead(); // Mantiene el sistema despierto
				display_clear();
				display_println("PROCESANDO...", 20);
				display_update();
				estadoActual = VALIDAR_RFID;
				break;
			}

            // Mini MEF del AS608: pasos breves en cada ciclo
            {

                // Programar inicio de escaneo cada cierto intervalo
                if (!escaneoActivo && (tickRead() - agendaHuella) >= INTERVALO_HUELLA_MS){
                    as608ScanReset();
                    escaneoActivo = true;
                    agendaHuella = tickRead();
                }

                if (escaneoActivo){
					estadoActual = LEER_Y_VALIDAR_HUELLA; // Pasamos a estado de validación de huella para evaluar el resultado del escaneo
					break; // Salimos del switch para procesar el escaneo en el próximo ciclo
                }
		}
        break;
        }

		case LEER_Y_VALIDAR_HUELLA: {
		short int id=0, score=0;
        as608ScanStatus_t st = as608ScanStep(&id, &score);
		if (st == AS608_SCAN_MATCH){
			char idStr[5];
			snprintf(idStr, sizeof(idStr), "%04u", (unsigned)id);
			// ultimaPresencia = tickRead();
			if (validarHuella(idStr)){
				const char *tag = obtenerTagPorHuella(idStr);
				display_clear();
				display_println("ACCESO CONCEDIDO", 20);
				char buffer[30];
				sprintf(buffer, "BIENVENIDO, %s", tag);
				display_println(buffer, 35);
				display_update();
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
			}
		} else if (st == AS608_SCAN_NOMATCH){
				display_clear();
				display_println("HUELLA NO", 20);
				display_println("REGISTRADA", 35);
				display_update();
				alertaError();
				delay(1500);
				display_clear();
				escaneoActivo = false;
				ultimaPresencia = tickRead();

		} else if (st == AS608_SCAN_NOFINGER || st == AS608_SCAN_ERROR){
		 	escaneoActivo = false; // liberar hasta el proximo intento programado
		}	
			estadoActual = ESPERANDO_ACCION;
			break;
		}

        case VALIDAR_RFID: {
			const char *tag = obtenerTagPorRFID(rfid);
			if (tag) {
				display_clear();
				display_println("ACCESO CONCEDIDO", 20);
				char buffer[30];
				sprintf(buffer, "BIENVENIDO, %s", tag);
				display_println(buffer,10);
				display_update();
				alertaExito();
				estadoActual = SENSOR_CIERRE;
				rfid[0] = '\0'; // Limpiar RFID leído
			}else{
				display_clear();
				display_println("ACCESO DENEGADO", 20);
				display_println("RFID NO REGISTRADO", 35);
				display_update();
				alertaError();
				tecladoReset();
				estadoActual = ESPERANDO_ACCION;
			}
			break;
		}
		
        case VALIDAR:
        	display_clear();
            if (validarPin(pinIngresado)){
                  const char *tag = obtenerTagPorPin(pinIngresado);
                  if (tag) {
                	  display_println("ACCESO CONCEDIDO", 20);
                	  char buffer[30];
                	  sprintf(buffer, "BIENVENIDO, %s", tag);
                	  display_println(buffer, 35);
                      //printf("\r\n[ACCESO] CORRECTO - Bienvenido, %s\r\n", tag);
                  } else {
                	  display_println("ACCESO CONCEDIDO", 20);
                	  display_println("SIN ETIQUETA", 35);
                      //printf("\r\n[ACCESO] CORRECTO\r\n");
                  }

                  alertaExito();
                  intentosFallidos = 0;
                  estadoActual = SENSOR_CIERRE;
            } else {
               alertaError();
               intentosFallidos++;
               //printf("\r\n[ACCESO] DENEGADO (%d/%d)\r\n",intentosFallidos, MAX_INTENTOS);
               display_println("ACCESO DENEGADO ", 10);
               char buffer[40];
			   sprintf(buffer, "INTENTOS = %d de 3", intentosFallidos);
			   display_println(buffer, 25);
			   if(intentosFallidos == 3){
				   display_println("ALERTA INTRUSO ", 40);
				   intentosFallidos = 0;
				   alertaError();
			   }
               //printf("\r\n[ACCESO] INCORRECTO \r\n");
               tecladoReset();
               display_update();
               delay(1000);
               display_clear();
               estadoActual = ESPERANDO_ACCION;
               break;
            }
            display_update();
            abrirCerradura();
            delay(1200);
            display_clear();
            break;
   
         case SENSOR_CIERRE:
               //Checkea el sensor y cierra en caso que este en bajo.
		          // GPIO0[1] en bajo (false) indica que la puerta está cerrada
        	  //display_clear();
			  display_println("CERRADURA", 40);
			  display_println("ABIERTA", 60);
			  display_update();

			  //CODIGO DE PRUEBA
			  if (OMITIR_SENSOR_CIERRE) {
				delay(4000);
				cerrarCerradura();
				display_clear();
				estadoActual = ESPERANDO_ACCION;
			  }else{
				  bool_t sensorState = Chip_GPIO_GetPinState(LPC_GPIO_PORT, SENSORHALL_GPIO_PORT, SENSORHALL_GPIO_PIN);
				  printf("\r\n[DEBUG] Estado sensor GPIO0[1]: %d (0=bajo/cerrado, 1=alto/abierto)\r\n", sensorState);

				  if(!sensorState){
					printf("\r\n[SENSOR] Puerta cerrada detectada\r\n");
					if (!OMITIR_MOTOR) {
						cerrarCerradura();
					} else {
						printf("\r\n[OMITIR MOTOR] Simulando cierre de cerradura\r\n");
					}
						ultimaPresencia = tickRead();
						estadoActual = ESPERANDO_ACCION;
						display_clear();
				  } else {
					  printf("\r\n [SENSOR] Esperando cierre de puerta...\r\n");
				  }
				  delay(500); // Delay para no saturar el puerto serie
			  }
            break;
            
         case MENU_ADMIN:
        	 display_clear();
			 display_println("MENU ADMINISTRADOR", 0);
			 // Usamos coordenadas separadas por 12px para que no se pisen
			 display_printAt("1=Registrar RFID", 0, 16);
			 display_printAt("2=Registrar Huella", 0, 30);
			 display_printAt("3=Reset WiFi", 0, 35);
			 display_printAt("4=Borrar Huellas", 0, 42);
			 display_printAt("5=Sinc. con servidor", 0, 49);
			 display_update(); // Refrescamos pantalla UNA vez
             //printf("\r\n[ADMIN] OPCIONES: 1=RFID | 2=Registrar huella | 3=Reset WiFi | 4=Limpiar huellas sensor \r\n");
            char opcion = 0;

            while (!opcion) {
                char tecla;
                if (tecladoLeerTecla(&tecla)) {
                    opcion = tecla;
//                    char buffer[1];
//					sprintf(buffer, "OPCION INGRESADA: %c", opcion);
//					display_println(buffer, 49);
//					display_update();
                }
                delay(60);
            }
            if(opcion){
				switch (opcion) {
					case '1':
						estadoActual = REGISTRAR_RFID;
						break;
					case '2':
						estadoActual = REGISTRAR_HUELLA;
						break;
					case '3':
						//printf("\r\n[ADMIN] Resetear credenciales WiFi...\r\n");
						display_clear();
						display_println("RESET WiFi ", 30);
						display_update();
						//delay(1000);
						if(resetearCredencialesESP()){
							//printf("\r\n[ADMIN] Esperando reconexión ESP...\r\n");
							display_clear();
							display_println("RECONECTANDO ESP...", 30);
							display_update();

							// Esperar que el ESP se reinicie
							delay(3000);
							// Reinicializar comunicación
							inicializarESP(60000);
							//printf("\r\n[ADMIN] Use la app ESP BluFi para configurar WiFi\r\n");
							display_clear();
							display_println("CONFIGURAR WiFi:", 10);
							display_println("USA LA APP", 25);
							display_println("\"ESP BluFI\"", 40);
							display_update();
						} else {
							//printf("\r\n[ADMIN] Error al resetear ESP\r\n");
							display_clear();
							display_println("ERROR AL RESETEAR ESP", 30);
							display_update();
						}
						delay(1000);
						display_clear();
						estadoActual = ESPERANDO_ACCION;
						break;

					case '4': {
						// ------------------------------------------------
						// 1. SEGURIDAD: PEDIR PIN MASTER
						// ------------------------------------------------
						display_clear();
						display_println("SEGURIDAD:", 10);
						display_println("PIN MASTER?", 22);
						display_update();

						char pinAuth[6] = {0};
						long int tAuth = tickRead();
						while(1) {
							if(tecladoLeerPin(pinAuth, &tAuth) == 1) break;
							if((tickRead() - tAuth) > 10000) { // Timeout 10s
								opcion = 0; // Salir del switch
								break;
							}
							delay(50);
						}
						if(opcion == 0 || !esPinMaster(pinAuth)) {
							 display_clear();
							 display_println("ERROR SEGURIDAD", 30);
							 display_update();
							 alertaError();
							 delay(1500);
							 break;
						}

						// ------------------------------------------------
						// 2. SUB-MENU DE BORRADO
						// ------------------------------------------------
						display_clear();
						display_println("1=BORRAR TODAS", 10);
						display_println("2=BORRAR UNA", 22);
						display_update();

						char subOp = 0;
						long int tSub = tickRead();
						while(!subOp && (tickRead()-tSub < 10000)){
							char k;
							if(tecladoLeerTecla(&k)) {
								if(k=='1' || k=='2') subOp = k;
							}
							delay(50);
						}

						// ------------------------------------------------
						// OPCION 1: BORRAR TODAS (FACTORY RESET DE HUELLAS)
						// ------------------------------------------------
						if (subOp == '1') {
							display_clear();
							display_println("BORRANDO...", 10);
							display_update();

							// A. Borrar Sensor
							if(as608ClearAllTemplates()) {
								// B. Borrar Local
								eliminarTodasHuellasLocal();
								// C. Borrar Servidor
								espBorrarTodasHuellasServidor();

								display_println("SENSOR SIN", 30);
								display_println("HUELLAS", 40);
								alertaExito();
							} else {
								display_println("3RROR SENSOR", 40);
								alertaError();
							}
							display_update();
							delay(2000);
							display_clear();
						}

						// ------------------------------------------------
						// OPCION 2: BORRAR UNA ESPECIFICA
						// ------------------------------------------------
						else if (subOp == '2') {
							display_clear();
							display_println("PIN DE USUARIO", 10);
							display_println("A BORRAR:", 22);
							display_update();

							char pinTarget[6] = {0};
							tSub = tickRead();
							while(1){
								if(tecladoLeerPin(pinTarget, &tSub) == 1) break;
								if((tickRead()-tSub) > 15000) break;
								delay(50);
							}

							// Buscar qu� ID de sensor tiene ese PIN
							char idStr[5] = {0};
							if (validarPin(pinTarget) && obtenerHuellaAsignadaPorPin(pinTarget, idStr)) {

								uint16_t idSensor = (uint16_t)atoi(idStr);

								display_clear();
								display_println("SE BORRARA", 20);
								char buffer[30];
								sprintf(buffer, "ID = , %d", idSensor);
								display_println(buffer, 35);
								display_update();
								delay(5000);
								// A. Borrar del Sensor
								if(as608DeleteId(idSensor)) {
									// B. Borrar Local
									eliminarHuellaLocal(pinTarget);
									// C. Borrar Servidor
									espBorrarHuellaServidor(pinTarget);

									display_clear();
									display_println("HUELLA BORRADA", 20);
									alertaExito();
								} else {
									display_println("ERROR SENSOR", 40);
									alertaError();
								}
							} else {
								display_clear();
								display_println("NO TIENE HUELLA", 20);
								display_println("O PIN INVALIDO", 40);
								alertaError();
							}
							display_update();
							delay(2000);
							display_clear();
						}
						ultimaPresencia = tickRead();
						estadoActual = ESPERANDO_ACCION;
						break;
					}
					case '5':
						sincronizarConServidor();
						ultimaPresencia = tickRead();
						estadoActual = ESPERANDO_ACCION;
						break;
					default:
						display_clear();
						display_println("OPCION INVALIDA", 30);
						//printf("\r\n[ADMIN] Opción inválida\r\n");
						display_update();
						display_clear();
						ultimaPresencia = tickRead();
						estadoActual = ESPERANDO_ACCION;
						break;
				}
            }
         break;
        // Se elimina LEER_HUELLA como estado separado; se integra polling en LEER_PIN

        case REGISTRAR_RFID: {
			int reintentosFallidos=0;
			display_clear();
			display_println("REGISTRAR TARJETA", 20);
			display_println("INGRESE PIN USUARIO", 30);
			display_update();

			char pinValidado[6] = {0};
			long int timer = tickRead();

			// 1. Pedir PIN
			while (1) {
				if (tecladoLeerPin(pinValidado, &timer) == 1) break;
				if ((tickRead() - timer) > 15000) { // Timeout
					display_clear();
					ultimaPresencia = tickRead();
					estadoActual = ESPERANDO_ACCION;
					break;
				}
				delay(50);
			}
			timer = tickRead();
			while (!validarPin(pinValidado)) {
				reintentosFallidos++;
				display_clear();
				display_println("PIN NO EXISTE", 20);
				display_println("INTENTE DE NUEVO", 40);
				display_update();
				delay(3000);
				display_clear();
				if ((tickRead() - timer) > 15000 || reintentosFallidos >= 3) { // Timeout o demasiados intentos{
					ultimaPresencia = tickRead();
					estadoActual = ESPERANDO_ACCION;
					break;
				}
			}
			display_println("ACERQUE TARJETA", 20);
			display_update();
			while (!leer_rfid_str(rfid, sizeof(rfid))) {
				if ((tickRead() - timer) > 20000) { // Timeout largo para acercar tarjeta
					display_clear();
					ultimaPresencia = tickRead();
					estadoActual = ESPERANDO_ACCION;
					break;
				}
				delay(50);
			}
			alertaExito();
			printf("\r\n[RFID] Leido: %s\r\n", rfid);

			asociarRFIDaPin(pinValidado, rfid);
			display_clear();
			display_println("TARJETA GUARDADA", 20);
			display_println("Sincronizando servidor", 40);
			display_update();
			if (espEnviarNuevoRFID(pinValidado, rfid)) {
				display_clear();
				display_println("NUBE: OK", 40);
			} else {
				display_clear();
				display_println("NUBE: ERROR", 40);
			}
			display_update();
			delay(2000);
			display_clear();
			ultimaPresencia = tickRead();
			estadoActual = ESPERANDO_ACCION;
			break;

		}

         case REGISTRAR_HUELLA: {
					int reintentosFallidos=0;
                     display_clear();
                     display_println("REGISTRAR HUELLA", 10);
                     display_println("INGRESE PIN USUARIO", 22);
                     display_update();

                     char pinValidado[6] = {0};
                     long int timer = tickRead();

                     // 1. Pedir PIN
                     while (1) {
                         if (tecladoLeerPin(pinValidado, &timer) == 1) break;
                         if ((tickRead() - timer) > 15000) { // Timeout
                        	 display_clear();
                        	 ultimaPresencia = tickRead();
                             estadoActual = ESPERANDO_ACCION;
                             break;
                         }
                         delay(50);
                     }
                     if (estadoActual == ESPERANDO_ACCION) break;

                     // 2. Validar y Obtener ID
                     char idHuellaStr[5] = {0};

                     if (validarPin(pinValidado)) {

                         // Usamos la nueva funci�n que genera ID si no existe
                         if (obtenerHuellaAsignadaPorPin(pinValidado, idHuellaStr)) {
                            short int idObjetivo = (uint16_t)atoi(idHuellaStr);
                             display_clear();
                             display_println("REGISTRAR HUELLA:", 10);
                             display_println("COLOCAR DEDO EN", 20);
                             display_println("SENSOR", 30);
                             char buff[20];
                             sprintf(buff, "ID ASIGNADO: %d", idObjetivo);
                             display_println(buff, 50);
                             display_update();
                             delay(2000);
                             // 3. Enrolar en Sensor (Funci�n bloqueante del driver AS608)
                             char idResultado[5] = {0};
                             if (as608EnrollAtId(idObjetivo, idResultado)) {

                                 // 4. Guardar Localmente
                                 asociarHuellaaPin(pinValidado, idResultado);

                                 display_clear();
                                 display_println("HUELLA GUARDADA", 10);
                                 display_println("SUBIENDO...", 30);
                                 display_update();
                                 alertaExito();
                                 // 5. Sincronizar con Servidor
                                 if (espEnviarNuevaHuella(pinValidado, idResultado)) {
                                      display_println("NUBE: OK", 50);
                                 } else {
                                      display_println("NUBE: ERROR", 50);
                                 }
                                 display_update();
								 reintentosFallidos=0;
								 estadoActual = ESPERANDO_ACCION;

                             }  else {
                                 display_clear();
                                 display_println("ERROR SENSOR", 20);
                                 display_println("INTENTE DE NUEVO", 40);
                                 display_update();
                                 alertaError();
								 reintentosFallidos++;
                             }

                         } else {
                              display_println("ERROR BASE DATOS", 30);
                              display_update();
                         }
                     } else {
                         display_clear();
                         display_println("PIN NO EXISTE", 20);
                         display_update();

                     }
                     delay(1500);
                     display_clear();
                     ultimaPresencia = tickRead();
					 
					 if (reintentosFallidos >= 4) {
						reintentosFallidos=0;
                     	estadoActual = ESPERANDO_ACCION;
					 }
                     break;
                 }
    }
}
