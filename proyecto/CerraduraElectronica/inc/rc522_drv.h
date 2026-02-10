#ifndef RC522_DRV_H
#define RC522_DRV_H
#include "sapi.h"
#include <stdint.h>
#include <stdbool.h>


// Config pines 
#define RC522_SPI SPI0
#define RC522_CS_GPIO GPIO6 
#define RC522_RST_GPIO GPIO7 


// API m�nima para ISO14443A (REQA + anticollision CL1 + UID corto)
void rc522_hw_init(void);
void rc522_init_iso14443a(void);
uint8_t rc522_version(void); // lee VersionReg
uint8_t rc522_txcontrol(void); // lee TxControlReg
bool rc522_requestA(uint8_t atqa[2]); // REQA -> ATQA
bool rc522_anticoll_cl1(uint8_t uid5[5]); // 4B UID + BCC

// Limpia las IRQs internas del MFRC522 (CommIrqReg)
void rc522_clear_irqs(void);
bool leer_rfid_str(char *out, size_t maxlen); // REQA + anticollision, devuelve UID como string hex (sin 0x, mayusculas), o false si no detecta nada
#endif // RC522_DRV_H