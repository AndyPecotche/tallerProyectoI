#ifndef MEF_CONFIG_H_
#define MEF_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

/* Parámetros de MEF y sensores */
#define TIMEOUT_MS                 40000  /* ms para ingresar PIN */
#define OMITIR_SENSOR_CIERRE       0      /* 1=omitir sensor cierre (pruebas) */
#define OMITIR_MOTOR               0      /* 1=omitir control motor (pruebas) */
#define ALERTAS_ENABLE             1      /* 1=habilitar alertas */
#define SENSOR_WIFI_ENABLE         1      /* 1=habilitar modulo WiFi ESP AT */
#define SENSOR_HUELLA_ENABLE       1      /* 1=habilitar sensor de huella */

// 1. Define tu IP y Puerto aqu�
#define SERVER_IP "10.42.46.87"
#define SERVER_PORT "8000"
/* URL del servidor para sincronización */
#define ESP_SERVER_URL "http://" SERVER_IP ":" SERVER_PORT "/syncPins/?allPins=true"
//#define ESP_SERVER_URL "http://192.168.100.78:8000/syncPins/?allPins=true"

/* GPIO para sensor de cierre */
#define SENSORHALL_GPIO_PORT 0
#define SENSORHALL_GPIO_PIN  1

/* Configuración de pines y interrupciones (declaraciones) */
void configurarPines(void);
void configurarInterrupcionPRESENCIA(void);
void configurarInterrupcionRFID(void);

#endif /* MEF_CONFIG_H_ */
