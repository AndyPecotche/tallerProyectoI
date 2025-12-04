#ifndef MEF_CONFIG_H_
#define MEF_CONFIG_H_

#include <stdint.h>
#include <stdbool.h>

/* Parámetros de MEF y sensores */
#define TIMEOUT_MS                 30000  /* ms para ingresar PIN */
#define OMITIR_SENSOR_CIERRE       0      /* 1=omitir sensor cierre (pruebas) */
#define OMITIR_MOTOR               0      /* 1=omitir control motor (pruebas) */
#define SIMULAR_RFID_INTERRUPT     0      /* 1=usar TEC2 como RFID */
#define SIMULAR_PRESENCIA_INTERRUPT 0     /* 1=usar TEC1 como presencia */
#define ALERTAS_ENABLE             1      /* 1=habilitar alertas */
#define SENSOR_WIFI_ENABLE         1      /* 1=habilitar módulo WiFi ESP AT */
#define SENSOR_HUELLA_ENABLE       1      /* 1=habilitar sensor de huella */

/* URL del servidor para sincronización */
#define ESP_SERVER_URL "http://192.168.0.190:3000/syncPins/?allPins=true"

/* GPIO para sensor de cierre */
#define SENSORHALL_GPIO_PORT 0
#define SENSORHALL_GPIO_PIN  1

/* Configuración de pines y interrupciones (declaraciones) */
void configurarPines(void);
void configurarInterrupcionPRESENCIA(void);
void configurarInterrupcionRFID(void);

#endif /* MEF_CONFIG_H_ */